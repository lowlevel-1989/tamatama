/* arm/threads.c: los threads que pide el juego, por turnos. No son threads del sistema: corren
   en el mismo que el principal, cada uno con su propio stack, y se van pasando el turno. Así se
   comportan igual en el PC, bajo QEMU, y en la Raspberry Pi, y nunca hay dos a la vez hablando
   con el loader por el mismo canal.

   Todo vive en loader-arm, nunca en una función nativa: en el PC, las nativas corren en otro
   proceso, que no puede cambiar el stack de este.

   El thread principal es el que ejecuta app_main, y hace de scheduler:
     - thread_create prepara el contexto de un thread y vuelve enseguida, sin ejecutarlo.
     - Cada vez que termina una llamada de app_main al código ARM (init, step...), el
       principal da una ronda (thread_call_end): cada thread cuyo plazo ya ha pasado corre hasta que cede el turno.
     - Un thread cede el turno en usleep: queda dormido hasta su plazo y el scheduler no lo
       despierta antes. usleep puede tardar algo más de lo pedido, pero nunca vuelve antes.
     - Si el principal llama a usleep, da rondas a los threads mientras espera, y tampoco vuelve
       antes de su plazo.
     - Si la función de un thread termina, su stack se libera y su hueco queda libre. */
#include "arm.h"
#include <stddef.h>
#include <time.h>

/* asm/context_switch.s usa estos offsets: si Context cambia, no compila. */
_Static_assert(offsetof(Context, sp) == 32, "Context.sp must be at +32");
_Static_assert(offsetof(Context, lr) == 36, "Context.lr must be at +36");
_Static_assert(offsetof(Context, d8_d15) == 40, "Context.d8_d15 must be at +40");

/* El stack de cada thread: en la arena, para que el loader pueda leer en él los argumentos que
   el juego pasa por el stack a una función nativa. */
#define THREAD_STACK_BYTES (1024 * 1024)
#define MAX_THREADS 8

enum { THREAD_FREE, THREAD_READY, THREAD_FINISHED };

typedef struct {
    Context context;
    uint32_t function, argument; /* la función del juego y su argumento */
    uint8_t *stack;
    uint64_t wake_ns; /* no se le da el turno antes de este momento */
    int state;
} Thread;

static Thread threads[MAX_THREADS];
/* El contexto del principal, que es también el scheduler. */
static Context main_context;
/* El thread que tiene el turno, o NULL si lo tiene el principal. */
static Thread *current;
/* Cuántas llamadas al código ARM tiene en curso el principal: más de una si una nativa vuelve a
   llamar a ARM. Solo cuando vuelve a 0 ha terminado una llamada de app_main. */
static int main_calls;

/* Nanosegundos de un reloj que no retrocede. */
static uint64_t now_ns(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec * 1000000000u + (uint64_t)t.tv_nsec;
}

/* Duerme de verdad hasta el momento indicado. Si lo corta una signal, vuelve a medir. */
static void sleep_until(uint64_t deadline) {
    for (uint64_t now; (now = now_ns()) < deadline;) {
        uint64_t left = deadline - now;
        struct timespec wait = {(time_t)(left / 1000000000u), (long)(left % 1000000000u)};
        nanosleep(&wait, NULL);
    }
}

/* Donde empieza un thread, la primera vez que recibe el turno: context_switch salta aquí con
   el stack nuevo. Ejecuta la función del juego y, al terminar, devuelve el turno al scheduler
   para siempre: un thread terminado no se vuelve a elegir. */
static void thread_start(void) {
    Thread *thread = current;
    uint32_t words[1] = {thread->argument};
    uint32_t result = (uint32_t)call_words((void *)thread->function, words, 1);
    log_msg("[thread] %d: 0x%x finished: %d\n", (int)(thread - threads), thread->function, (int)result);
    thread->state = THREAD_FINISHED;
    context_switch(&thread->context, &main_context);
    die("a finished thread got the turn");
}

int thread_create(uint32_t function, uint32_t argument) {
    Thread *thread = NULL;
    for (int i = 0; i < MAX_THREADS && !thread; i++)
        if (threads[i].state == THREAD_FREE) thread = &threads[i];
    if (!thread) {
        log_msg("[thread] 0x%x refused: the limit is %d threads\n", function, MAX_THREADS);
        return -1;
    }
    uint8_t *stack = arena_malloc(THREAD_STACK_BYTES);
    if (!stack) die("no memory for the thread stack");
    memset(thread, 0, sizeof *thread);
    thread->function = function;
    thread->argument = argument;
    thread->stack = stack;
    /* El final del stack, porque crece hacia abajo, y aligned a 8 como pide AAPCS. */
    thread->context.sp = (uint32_t)(stack + THREAD_STACK_BYTES) & ~7u;
    thread->context.lr = (uint32_t)thread_start;
    thread->state = THREAD_READY; /* wake_ns = 0: corre en la próxima ronda */
    log_msg("[thread] %d: 0x%x(0x%x), stack 0x%x-0x%x\n", (int)(thread - threads), function, argument,
            (uint32_t)stack, thread->context.sp);
    return 0;
}

/* Le da el turno a un thread hasta que lo devuelva. Solo lo llama el principal. */
static void run_thread(Thread *thread) {
    current = thread;
    context_switch(&main_context, &thread->context);
    current = NULL;
    if (thread->state == THREAD_FINISHED) {
        arena_free(thread->stack);
        thread->state = THREAD_FREE;
    }
}

/* Una ronda: cada thread cuyo plazo ya ha pasado corre hasta que cede el turno. Devuelve el
   plazo más cercano de los que siguen durmiendo, o UINT64_MAX si no queda ninguno. */
static uint64_t thread_round(void) {
    uint64_t next = UINT64_MAX;
    for (int i = 0; i < MAX_THREADS; i++) {
        Thread *thread = &threads[i];
        if (thread->state == THREAD_READY && thread->wake_ns <= now_ns()) run_thread(thread);
        if (thread->state == THREAD_READY && thread->wake_ns < next) next = thread->wake_ns;
    }
    return next;
}

/* Alrededor de cada llamada al código ARM. Al terminar una llamada de app_main, el principal da
   una ronda. Las anidadas y las que hace un thread no cuentan. */
void thread_call_begin(void) {
    if (!current) main_calls++;
}
void thread_call_end(void) {
    if (!current && --main_calls == 0) thread_round();
}

/* El usleep que recibe el juego. */
int thread_usleep(uint32_t microseconds) {
    uint64_t deadline = now_ns() + (uint64_t)microseconds * 1000u;
    if (current) {
        /* Un thread: duerme hasta su plazo y cede el turno. El scheduler no lo despierta
           antes, así que al volver aquí el plazo ya se ha cumplido. */
        current->wake_ns = deadline;
        context_switch(&current->context, &main_context);
        return 0;
    }
    /* El principal: mientras espera, los threads que estén listos corren. Duerme solo hasta el
       primer plazo, el suyo o el de un thread. */
    for (;;) {
        uint64_t next = thread_round();
        if (now_ns() >= deadline) return 0;
        sleep_until(next < deadline ? next : deadline);
    }
}

/* OnThreadCreate(II)I y similares: function y argument son los dos primeros argumentos del
   método de Java. Devuelve 0 si se creó, y -1 si no, como espera el juego. */
uint64_t java_thread_create(const uint32_t *words) {
    return (uint32_t)thread_create(words[0], words[1]);
}
