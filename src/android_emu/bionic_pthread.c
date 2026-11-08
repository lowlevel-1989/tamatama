/* android_emu/bionic_pthread.c: threads para el juego. En Android,
   pthread_mutex_t, pthread_cond_t y sem_t miden 4 bytes, y las versiones de
   glibc no caben ahí. Por eso esos 4 bytes guardan un pointer al objeto real,
   que se crea la primera vez que se usa. */
/* el juego se compiló con time_t de 32 bits; debe ir antes de cualquier
   #include */
#undef _TIME_BITS
#undef _FILE_OFFSET_BITS
#include "arm.h"
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <semaphore.h>
#include <signal.h>
#include <time.h>

/* Valores que puede tener un mutex de bionic antes de usarse por primera
   vez. */
enum { MUTEX_STATIC = 0, MUTEX_STATIC_RECURSIVE = 0x4000, MUTEX_STATIC_ERRORCHECK = 0x8000 };
/* Protege la creación de los objetos reales. */
static pthread_mutex_t creation_lock = PTHREAD_MUTEX_INITIALIZER;

/* Leer o escribir 4 bytes aligned es atómico. La memory barrier lo ordena con
   el objeto que publica. */
static uint32_t load_slot(uint32_t *slot) {
    uint32_t v = *(volatile uint32_t *)slot;
    __sync_synchronize();
    return v;
}
static void publish_slot(uint32_t *slot, uint32_t v) {
    __sync_synchronize();
    *(volatile uint32_t *)slot = v;
}

/* 1 si el valor es uno de los iniciales, y por tanto aún no hay mutex real. */
static int is_static_mutex(uint32_t v) {
    return v == MUTEX_STATIC || v == MUTEX_STATIC_RECURSIVE || v == MUTEX_STATIC_ERRORCHECK;
}
/* Crea un mutex real del tipo indicado. */
static pthread_mutex_t *new_mutex(int type) {
    pthread_mutexattr_t a;
    pthread_mutexattr_init(&a);
    pthread_mutexattr_settype(&a, type);
    pthread_mutex_t *m = malloc(sizeof *m);
    pthread_mutex_init(m, &a);
    pthread_mutexattr_destroy(&a);
    return m;
}
/* Encuentra el mutex real, y lo crea si es su primer uso. */
static pthread_mutex_t *real_mutex(uint32_t *slot) {
    uint32_t v = load_slot(slot);
    if (is_static_mutex(v)) {
        pthread_mutex_lock(&creation_lock);
        v = *slot;
        if (is_static_mutex(v)) {
            int type = v == MUTEX_STATIC_RECURSIVE    ? PTHREAD_MUTEX_RECURSIVE
                       : v == MUTEX_STATIC_ERRORCHECK ? PTHREAD_MUTEX_ERRORCHECK
                                                      : PTHREAD_MUTEX_DEFAULT;
            v = (uint32_t)(uintptr_t)new_mutex(type);
            publish_slot(slot, v);
        }
        pthread_mutex_unlock(&creation_lock);
    }
    return (pthread_mutex_t *)(uintptr_t)v;
}
/* Crea el mutex real. El pthread_mutexattr_t de bionic y el de glibc guardan un
   tipo de 4 bytes, con los mismos valores. */
static int bionic_mutex_init(uint32_t *slot, const pthread_mutexattr_t *attr) {
    pthread_mutex_t *m = malloc(sizeof *m);
    if (!m) return ENOMEM;
    int r = pthread_mutex_init(m, attr);
    if (r) free(m);
    else *slot = (uint32_t)(uintptr_t)m;
    return r;
}
/* Destruye el mutex real y deja el valor inicial. */
static int bionic_mutex_destroy(uint32_t *slot) {
    if (is_static_mutex(*slot)) return 0;
    pthread_mutex_t *m = (pthread_mutex_t *)(uintptr_t)*slot;
    int r = pthread_mutex_destroy(m);
    if (!r) {
        free(m);
        *slot = MUTEX_STATIC;
    }
    return r;
}
/* Las operaciones del juego, sobre el mutex real. */
static int bionic_mutex_lock(uint32_t *s) { return pthread_mutex_lock(real_mutex(s)); }
static int bionic_mutex_trylock(uint32_t *s) { return pthread_mutex_trylock(real_mutex(s)); }
static int bionic_mutex_unlock(uint32_t *s) { return pthread_mutex_unlock(real_mutex(s)); }

/* Una hora absoluta en el reloj que usan las condiciones de glibc, dentro de ns
   nanosegundos. */
static struct timespec realtime_after(uint64_t ns) {
    struct timespec t;
    clock_gettime(CLOCK_REALTIME, &t);
    ns += (uint64_t)t.tv_nsec;
    t.tv_sec += (time_t)(ns / 1000000000u);
    t.tv_nsec = (long)(ns % 1000000000u);
    return t;
}
/* Intenta bloquear durante msecs milisegundos. */
static int bionic_mutex_lock_timeout(uint32_t *s, unsigned msecs) {
    struct timespec t = realtime_after((uint64_t)msecs * 1000000u);
    return pthread_mutex_timedlock(real_mutex(s), &t);
}

/* Encuentra la condición real, y la crea si es su primer uso. Una condición sin
   usar vale 0. */
static pthread_cond_t *real_cond(uint32_t *slot) {
    uint32_t v = load_slot(slot);
    if (!v) {
        pthread_mutex_lock(&creation_lock);
        v = *slot;
        if (!v) {
            pthread_cond_t *c = malloc(sizeof *c);
            pthread_cond_init(c, NULL);
            v = (uint32_t)(uintptr_t)c;
            publish_slot(slot, v);
        }
        pthread_mutex_unlock(&creation_lock);
    }
    return (pthread_cond_t *)(uintptr_t)v;
}
/* Crea la condición. Los atributos se ignoran. */
static int bionic_cond_init(uint32_t *slot, const void *attr) {
    (void)attr;
    *slot = 0;
    real_cond(slot);
    return 0;
}
/* Destruye la condición real y deja el valor inicial. */
static int bionic_cond_destroy(uint32_t *slot) {
    if (!*slot) return 0;
    pthread_cond_t *c = (pthread_cond_t *)(uintptr_t)*slot;
    int r = pthread_cond_destroy(c);
    if (!r) {
        free(c);
        *slot = 0;
    }
    return r;
}
/* Las esperas del juego, sobre los objetos reales. */
static int bionic_cond_wait(uint32_t *c, uint32_t *m) {
    return pthread_cond_wait(real_cond(c), real_mutex(m));
}
static int bionic_cond_timedwait(uint32_t *c, uint32_t *m, const struct timespec *t) {
    return pthread_cond_timedwait(real_cond(c), real_mutex(m), t);
}
/* Espera como mucho ns nanosegundos. */
static int wait_relative(uint32_t *c, uint32_t *m, uint64_t ns) {
    struct timespec t = realtime_after(ns);
    return pthread_cond_timedwait(real_cond(c), real_mutex(m), &t);
}
/* Espera con un tiempo relativo. */
static int bionic_cond_timedwait_relative(uint32_t *c, uint32_t *m, const struct timespec *r) {
    return wait_relative(c, m, (uint64_t)r->tv_sec * 1000000000u + (uint64_t)r->tv_nsec);
}
/* El juego da una hora absoluta en el reloj monótono: se convierte en el tiempo
   que queda por esperar. */
static int bionic_cond_timedwait_monotonic(uint32_t *c, uint32_t *m, const struct timespec *t) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    int64_t ns = ((int64_t)t->tv_sec - now.tv_sec) * 1000000000 + (t->tv_nsec - now.tv_nsec);
    return wait_relative(c, m, ns > 0 ? (uint64_t)ns : 0);
}
/* Espera como mucho msecs milisegundos. */
static int bionic_cond_timeout(uint32_t *c, uint32_t *m, unsigned msecs) {
    return wait_relative(c, m, (uint64_t)msecs * 1000000u);
}
/* Despierta a uno de los threads que esperan, o a todos. */
static int bionic_cond_signal(uint32_t *c) { return pthread_cond_signal(real_cond(c)); }
static int bionic_cond_broadcast(uint32_t *c) { return pthread_cond_broadcast(real_cond(c)); }

/* Un sem_t no tiene valor inicial estático: siempre se llama antes a
   sem_init. */
/* Crea el semaphore real y guarda su dirección en el hueco de 4 bytes. */
static int bionic_sem_init(uint32_t *slot, int shared, unsigned value) {
    sem_t *s = malloc(sizeof *s);
    if (!s) return -1;
    if (sem_init(s, shared, value)) {
        free(s);
        return -1;
    }
    *slot = (uint32_t)(uintptr_t)s;
    return 0;
}
/* El sem_t real al que apunta el hueco de 4 bytes. */
static sem_t *real_sem(uint32_t *slot) { return (sem_t *)(uintptr_t)*slot; }
/* Destruye el semaphore real y deja el hueco a cero. */
static int bionic_sem_destroy(uint32_t *slot) {
    sem_t *s = real_sem(slot);
    int r = sem_destroy(s);
    free(s);
    *slot = 0;
    return r;
}
/* Las operaciones del juego, sobre el semaphore real. */
static int bionic_sem_wait(uint32_t *s) { return sem_wait(real_sem(s)); }
static int bionic_sem_trywait(uint32_t *s) { return sem_trywait(real_sem(s)); }
static int bionic_sem_post(uint32_t *s) { return sem_post(real_sem(s)); }
static int bionic_sem_getvalue(uint32_t *s, int *out) { return sem_getvalue(real_sem(s), out); }
static int bionic_sem_timedwait(uint32_t *s, const struct timespec *t) {
    return sem_timedwait(real_sem(s), t);
}

/* Los atributos de un thread: 24 bytes en Android, de los que solo miramos unos
   pocos. */
typedef struct {
    uint32_t flags; /* bit 0: separado (detached) */
    void *stack_base;
    size_t stack_size;
    size_t guard_size;
    int32_t sched_policy;
    int32_t sched_priority;
} BionicAttr;

/* Valores por defecto de Android: 1 MiB de stack y una guard page. */
static int bionic_attr_init(BionicAttr *a) {
    memset(a, 0, sizeof *a);
    a->stack_size = 1024 * 1024;
    a->guard_size = 4096;
    return 0;
}
/* No hay nada que liberar. Las funciones siguientes solo leen o escriben la
   estructura. */
static int bionic_attr_destroy(BionicAttr *a) {
    (void)a;
    return 0;
}
static int bionic_attr_setdetachstate(BionicAttr *a, int state) {
    a->flags = state == PTHREAD_CREATE_DETACHED ? a->flags | 1 : a->flags & ~1u;
    return 0;
}
static int bionic_attr_getdetachstate(const BionicAttr *a, int *state) {
    *state = a->flags & 1 ? PTHREAD_CREATE_DETACHED : PTHREAD_CREATE_JOINABLE;
    return 0;
}
static int bionic_attr_setstacksize(BionicAttr *a, size_t n) {
    a->stack_size = n;
    return 0;
}
static int bionic_attr_getstacksize(const BionicAttr *a, size_t *n) {
    *n = a->stack_size;
    return 0;
}
static int bionic_attr_setguardsize(BionicAttr *a, size_t n) {
    a->guard_size = n;
    return 0;
}
static int bionic_attr_getguardsize(const BionicAttr *a, size_t *n) {
    *n = a->guard_size;
    return 0;
}
static int bionic_attr_setschedpolicy(BionicAttr *a, int policy) {
    a->sched_policy = policy;
    return 0;
}
static int bionic_attr_getschedpolicy(const BionicAttr *a, int *policy) {
    *policy = a->sched_policy;
    return 0;
}
static int bionic_attr_setschedparam(BionicAttr *a, const struct sched_param *p) {
    a->sched_priority = p->sched_priority;
    return 0;
}
static int bionic_attr_getschedparam(const BionicAttr *a, struct sched_param *p) {
    p->sched_priority = a->sched_priority;
    return 0;
}
/* Devuelve los valores por defecto, sin consultar el thread real. */
static int bionic_getattr_np(pthread_t t, BionicAttr *a) {
    (void)t;
    return bionic_attr_init(a);
}
/* El canal con el loader atiende una llamada cada vez, de modo que un segundo
   thread podría mezclar sus mensajes. Es mejor rechazarlo que fallar más tarde
   de una forma incomprensible. */
static int bionic_create(pthread_t *out, const BionicAttr *a, void *(*start)(void *), void *arg) {
    if (has_host) {
        log_msg("[loader] pthread_create refused: threads need the ARM code on the CPU\n");
        return EAGAIN;
    }
    pthread_attr_t real;
    pthread_attr_init(&real);
    if (a) {
        if (a->flags & 1) pthread_attr_setdetachstate(&real, PTHREAD_CREATE_DETACHED);
        if (a->stack_size >= 65536) pthread_attr_setstacksize(&real, a->stack_size);
    }
    int r = pthread_create(out, &real, start, arg);
    pthread_attr_destroy(&real);
    return r;
}
/* pthread_cleanup_push y pop: el compiler deja esta estructura en el stack del
   juego. */
typedef struct Cleanup {
    struct Cleanup *prev;
    void (*routine)(void *);
    void *arg;
} Cleanup;
/* Guarda la función de limpieza en la estructura del juego. */
static void bionic_cleanup_push(Cleanup *c, void (*routine)(void *), void *arg) {
    c->routine = routine;
    c->arg = arg;
}
/* La ejecuta si se pide. No se ejecuta si el thread termina con
   pthread_exit. */
static void bionic_cleanup_pop(Cleanup *c, int execute) {
    if (execute) c->routine(c->arg);
}

/* Macro de la tabla: el nombre que pide el juego y la función que lo
   atiende. */
#define FUNC(sym, fn) {sym, (const void *)(fn), 0}
/* Lo que atiende este fichero. bionic_find la consulta después de su propia
   tabla. */
const BionicEntry pthread_entries[] = {
    FUNC("pthread_mutex_init", bionic_mutex_init),
    FUNC("pthread_mutex_destroy", bionic_mutex_destroy),
    FUNC("pthread_mutex_lock", bionic_mutex_lock),
    FUNC("pthread_mutex_trylock", bionic_mutex_trylock),
    FUNC("pthread_mutex_unlock", bionic_mutex_unlock),
    FUNC("pthread_mutex_lock_timeout_np", bionic_mutex_lock_timeout),
    FUNC("pthread_mutexattr_init", pthread_mutexattr_init),
    FUNC("pthread_mutexattr_destroy", pthread_mutexattr_destroy),
    FUNC("pthread_mutexattr_settype", pthread_mutexattr_settype),
    FUNC("pthread_mutexattr_gettype", pthread_mutexattr_gettype),
    FUNC("pthread_cond_init", bionic_cond_init),
    FUNC("pthread_cond_destroy", bionic_cond_destroy),
    FUNC("pthread_cond_wait", bionic_cond_wait),
    FUNC("pthread_cond_timedwait", bionic_cond_timedwait),
    FUNC("pthread_cond_timedwait_relative_np", bionic_cond_timedwait_relative),
    FUNC("pthread_cond_timedwait_monotonic_np", bionic_cond_timedwait_monotonic),
    FUNC("pthread_cond_timeout_np", bionic_cond_timeout),
    FUNC("pthread_cond_signal", bionic_cond_signal),
    FUNC("pthread_cond_broadcast", bionic_cond_broadcast),
    FUNC("pthread_condattr_init", pthread_condattr_init),
    FUNC("pthread_condattr_destroy", pthread_condattr_destroy),
    FUNC("sem_init", bionic_sem_init),
    FUNC("sem_destroy", bionic_sem_destroy),
    FUNC("sem_wait", bionic_sem_wait),
    FUNC("sem_trywait", bionic_sem_trywait),
    FUNC("sem_post", bionic_sem_post),
    FUNC("sem_getvalue", bionic_sem_getvalue),
    FUNC("sem_timedwait", bionic_sem_timedwait),
    FUNC("pthread_attr_init", bionic_attr_init),
    FUNC("pthread_attr_destroy", bionic_attr_destroy),
    FUNC("pthread_attr_setdetachstate", bionic_attr_setdetachstate),
    FUNC("pthread_attr_getdetachstate", bionic_attr_getdetachstate),
    FUNC("pthread_attr_setstacksize", bionic_attr_setstacksize),
    FUNC("pthread_attr_getstacksize", bionic_attr_getstacksize),
    FUNC("pthread_attr_setguardsize", bionic_attr_setguardsize),
    FUNC("pthread_attr_getguardsize", bionic_attr_getguardsize),
    FUNC("pthread_attr_setschedpolicy", bionic_attr_setschedpolicy),
    FUNC("pthread_attr_getschedpolicy", bionic_attr_getschedpolicy),
    FUNC("pthread_attr_setschedparam", bionic_attr_setschedparam),
    FUNC("pthread_attr_getschedparam", bionic_attr_getschedparam),
    FUNC("pthread_getattr_np", bionic_getattr_np),
    FUNC("pthread_create", bionic_create),
    FUNC("__pthread_cleanup_push", bionic_cleanup_push),
    FUNC("__pthread_cleanup_pop", bionic_cleanup_pop),
    /* El mismo tamaño y el mismo significado en bionic y glibc: no hace falta
       wrapper. Un rwlock mide 40 bytes en Android y menos en glibc, y todo a
       cero es un estado inicial válido. */
    FUNC("pthread_join", pthread_join),
    FUNC("pthread_detach", pthread_detach),
    FUNC("pthread_exit", pthread_exit),
    FUNC("pthread_self", pthread_self),
    FUNC("pthread_equal", pthread_equal),
    FUNC("pthread_kill", pthread_kill),
    FUNC("pthread_once", pthread_once),
    FUNC("pthread_setname_np", pthread_setname_np),
    FUNC("pthread_getschedparam", pthread_getschedparam),
    FUNC("pthread_setschedparam", pthread_setschedparam),
    FUNC("pthread_key_create", pthread_key_create),
    FUNC("pthread_key_delete", pthread_key_delete),
    FUNC("pthread_getspecific", pthread_getspecific),
    FUNC("pthread_setspecific", pthread_setspecific),
    FUNC("pthread_rwlock_init", pthread_rwlock_init),
    FUNC("pthread_rwlock_destroy", pthread_rwlock_destroy),
    FUNC("pthread_rwlock_rdlock", pthread_rwlock_rdlock),
    FUNC("pthread_rwlock_tryrdlock", pthread_rwlock_tryrdlock),
    FUNC("pthread_rwlock_wrlock", pthread_rwlock_wrlock),
    FUNC("pthread_rwlock_trywrlock", pthread_rwlock_trywrlock),
    FUNC("pthread_rwlock_unlock", pthread_rwlock_unlock),
    FUNC("sched_yield", sched_yield),
    FUNC("sched_get_priority_max", sched_get_priority_max),
    FUNC("sched_get_priority_min", sched_get_priority_min),
};
const unsigned pthread_entry_count = sizeof pthread_entries / sizeof pthread_entries[0];
