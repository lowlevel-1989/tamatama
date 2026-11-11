/* arm/trap.c: quién escribe en una variable del juego. watch dice cuándo
   cambia; trap dice además qué instrucción la escribió. En el fichero de
   reglas:
     trap NOMBRE LUGAR [TIPO] [VALOR]
   LUGAR y TIPO son los de watch (debug/watch.c). Con VALOR, escrito como lo
   muestra watch, solo avisa cuando la variable toma ese valor; sin él, de
   cualquier cambio. Por ejemplo:
     trap m_endDraw m_endDraw u32 0

   Con la dirección solo se puede leer la variable: no dice quién la escribe.
   Para enterarse en el momento de la escritura, su página (4096 bytes) se
   deja de solo lectura. Leer sigue funcionando; escribir interrumpe con
   SIGSEGV. Pero en la página hay otras variables, así que cada escritura se
   deja pasar y se mira después, con un breakpoint en la instrucción
   siguiente:
     safe point -> ¿la variable tiene otro valor que el buscado?
       -> no: ya lo tiene y no puede pasar a él: la página queda abierta
       -> sí: página de solo lectura
     una instrucción escribe en la página -> SIGSEGV -> on_write:
       guarda el valor de la variable, abre la página
       y pone un udf (instrucción no válida) en la instrucción siguiente. No
       se usan los udf de los debuggers (0xde01 y 0xe7f001f0): esos los
       convierte en SIGTRAP
       -> la escritura se repite y se hace
       -> la instrucción siguiente es el udf -> SIGILL -> on_step:
          ¿cambió la variable? -> apunta el pc de la escritura y quién
          corría: la llamada de app_main (init, step...) o el thread N (el
          de "[thread] N:")
          quita el udf y vuelve a cerrar la página
     siguiente safe point -> ¿tomó el valor buscado? -> muestra quién lo puso

   Así la página solo está abierta durante una instrucción y se ve cada
   escritura, también varias en un turno: se muestra la última que cambió la
   variable, que es la que dejó el valor. Un caso raro se escapa: si la
   instrucción siguiente está en un bloque IT de Thumb y su condición no se
   cumple, el udf no se ejecuta y la página se queda abierta hasta el safe
   point. Entonces sale "not caught": nunca da un pc equivocado sin avisar.

   Solo ve lo que escribe el código ARM. Bajo QEMU, lo que escriben las
   funciones nativas lo hace loader (x86_64) por su propio mapping de la arena,
   y no pasa por aquí. */
#include "arm.h"
#include "memory.h"
#include <signal.h>
#include <sys/mman.h>
#include <ucontext.h>

#define MAX_TRAPS 8
#define MAX_TEXT 64
#define PAGE 4096u
#define PAGE_OF(address) ((address) & ~(PAGE - 1))
#define THUMB_BIT 0x20u /* en cpsr: el código es Thumb */

typedef struct {
    const char *name;
    const char *place;  /* el LUGAR, tal como se escribió */
    const char *wanted; /* el VALOR buscado, o NULL: cualquier cambio */
    ValueType type;
    arm_ptr address;      /* 0 hasta que el .so está cargado */
    char last[MAX_TEXT];  /* el valor en el safe point anterior */
    uint8_t before[8];    /* los bytes antes de la escritura en curso */
    volatile uint32_t pc;     /* la última escritura que la cambió; 0 si no
                                 hubo */
    volatile uint32_t lr;     /* su lr: en una función hoja, quién la llamó */
    volatile int thread;      /* el thread que la hizo, o -1: el principal */
    char call[64];            /* con el principal: la llamada de app_main */
} Trap;

static Trap traps[MAX_TRAPS];
static int trap_count;

/* La escritura en curso: entre on_write y on_step. */
static struct {
    uint32_t page;       /* la página abierta; 0 si no hay ninguna */
    uint32_t pc;         /* la instrucción que escribe */
    uint32_t lr;         /* y su lr */
    int thread;          /* quién la ejecuta */
    const char *call;    /* con el principal: la llamada de app_main */
    uint32_t step;       /* dónde está el udf */
    uint16_t saved[2];   /* lo que había donde está el udf */
    int thumb;
} pending;

static int value_size(const Trap *t) {
    int bytes = value_bytes(t->type);
    return bytes > 8 ? 8 : bytes;
}

/* Escribe en el código, que es de solo lectura y ejecución. */
static void patch_code(uint32_t address, const void *bytes, size_t n) {
    void *page = (void *)PAGE_OF(address);
    mprotect(page, PAGE, PROT_READ | PROT_WRITE | PROT_EXEC);
    memcpy((void *)address, bytes, n);
    flush_code((void *)address, n);
    mprotect(page, PAGE, PROT_READ | PROT_EXEC);
}

/* SIGSEGV. Solo se hacen cosas que valen dentro de una señal: copiar bytes y
   llamar a mprotect. Mostrar lo que pasó espera al safe point. */
static void on_write(int signal_number, siginfo_t *info, void *context) {
    (void)signal_number;
    uint32_t page = PAGE_OF((uint32_t)info->si_addr);
    int ours = 0;
    for (int i = 0; i < trap_count; i++)
        if (traps[i].address && PAGE_OF(traps[i].address) == page) {
            ours = 1;
            memcpy(traps[i].before, (void *)traps[i].address, (size_t)value_size(&traps[i]));
        }
    if (!ours || pending.page) {
        /* Un fallo de verdad: sin manejador, la instrucción se repite y el
           programa termina como siempre. */
        signal(SIGSEGV, SIG_DFL);
        return;
    }
    const mcontext_t *mc = &((const ucontext_t *)context)->uc_mcontext;
    pending.page = page;
    pending.pc = mc->arm_pc;
    pending.lr = mc->arm_lr;
    pending.thread = thread_current();
    pending.call = thread_main_call();
    pending.thumb = (mc->arm_cpsr & THUMB_BIT) != 0;
    /* La instrucción siguiente: en Thumb ocupa 4 bytes si los 5 bits altos de
       su primera mitad son 11101, 11110 o 11111, y si no 2; en ARM, 4. */
    uint32_t length = 4;
    if (pending.thumb) length = (*(uint16_t *)pending.pc >> 11) >= 0x1D ? 4 : 2;
    pending.step = pending.pc + length;
    memcpy(pending.saved, (void *)pending.step, pending.thumb ? 2 : 4);
    if (pending.thumb) {
        const uint16_t udf = 0xDE7A; /* udf #0x7a */
        patch_code(pending.step, &udf, 2);
    } else {
        const uint32_t udf = 0xE7F7A7FA; /* udf #0x7a7a */
        patch_code(pending.step, &udf, 4);
    }
    /* Al volver, la escritura se repite y esta vez se hace. */
    mprotect((void *)page, PAGE, PROT_READ | PROT_WRITE);
}

/* SIGILL: el udf de la instrucción siguiente a la escritura. */
static void on_step(int signal_number, siginfo_t *info, void *context) {
    (void)info;
    const mcontext_t *mc = &((const ucontext_t *)context)->uc_mcontext;
    if (!pending.page || mc->arm_pc != pending.step) {
        /* una instrucción no válida, o un breakpoint, de verdad */
        signal(signal_number, SIG_DFL);
        return;
    }
    for (int i = 0; i < trap_count; i++) {
        Trap *t = &traps[i];
        if (t->address && PAGE_OF(t->address) == pending.page &&
            memcmp(t->before, (void *)t->address, (size_t)value_size(t))) {
            t->pc = pending.pc;
            t->lr = pending.lr;
            t->thread = pending.thread;
            memcpy(t->call, pending.call, sizeof t->call);
        }
    }
    /* Al volver se ejecuta la instrucción de verdad. */
    patch_code(pending.step, pending.saved, pending.thumb ? 2 : 4);
    mprotect((void *)pending.page, PAGE, PROT_READ);
    pending.page = 0;
}

/* Lo llama rules_load por cada línea trap. spec es lo que va detrás del
   nombre: "LUGAR [TIPO] [VALOR]". */
void trap_add(const char *name, const char *spec) {
    if (trap_count == MAX_TRAPS) {
        log_msg("[trap] too many traps, the limit is %d\n", MAX_TRAPS);
        return;
    }
    Trap *t = &traps[trap_count];
    memset(t, 0, sizeof *t);
    t->name = name;
    t->type = TYPE_I32;
    char *copy = strdup(spec);
    t->place = copy;
    for (char *word = next_word(copy); *word && *word != '#';) {
        char *next = next_word(word);
        if (!value_type_parse(word, &t->type)) t->wanted = word;
        word = next;
    }
    if (!trap_count) {
        struct sigaction action = {0};
        action.sa_flags = SA_SIGINFO;
        action.sa_sigaction = on_write;
        sigaction(SIGSEGV, &action, NULL);
        action.sa_sigaction = on_step;
        sigaction(SIGILL, &action, NULL);
        sigaction(SIGTRAP, &action, NULL); /* por si algún sistema lo da así */
    }
    trap_count++;
}

/* 1 si hay que vigilar la variable: sin VALOR siempre, y con VALOR solo
   mientras tenga otro. */
static int needs_page(const Trap *t) {
    return t->address && (!t->wanted || strcmp(t->last, t->wanted));
}

/* En cada safe point: muestra lo que pasó en el turno y vuelve a proteger las
   páginas que hacen falta. La primera vez que el .so está cargado, calcula
   las direcciones. */
void trap_check(void) {
    for (int i = 0; i < trap_count; i++) {
        Trap *t = &traps[i];
        if (!t->address) {
            t->address = place_resolve(t->place);
            if (!t->address) continue; /* el .so aún no está cargado */
            value_format(t->type, t->address, t->last, MAX_TEXT);
            log_msg("[trap] %s at 0x%x = %s, waiting for %s\n", t->name, t->address, t->last,
                    t->wanted ? t->wanted : "any change");
            continue;
        }
        char now[MAX_TEXT];
        value_format(t->type, t->address, now, MAX_TEXT);
        if (strcmp(now, t->last) && (!t->wanted || !strcmp(now, t->wanted))) {
            if (t->pc) {
                char written[160], from[160], who[80];
                elf_format_address(t->pc, written, sizeof written);
                elf_format_address(t->lr & ~1u, from, sizeof from);
                if (t->thread < 0) snprintf(who, sizeof who, "%s", t->call);
                else snprintf(who, sizeof who, "thread %d", t->thread);
                log_msg("[trap] %s %s -> %s, written at %s (lr %s) by %s\n", t->name, t->last, now,
                        written, from, who);
            } else
                log_msg("[trap] %s %s -> %s, not caught: its page was open\n", t->name, t->last,
                        now);
        }
        memcpy(t->last, now, MAX_TEXT);
        t->pc = 0;
    }
    /* Si un udf no llegó a ejecutarse, se quita aquí. */
    if (pending.page) {
        patch_code(pending.step, pending.saved, pending.thumb ? 2 : 4);
        pending.page = 0;
    }
    /* Una página se protege si alguna variable suya lo necesita, y si no, se
       abre: puede seguir protegida del turno anterior. */
    for (int i = 0; i < trap_count; i++) {
        if (!traps[i].address) continue;
        uint32_t page = PAGE_OF(traps[i].address);
        int needed = 0;
        for (int k = 0; k < trap_count; k++)
            if (needs_page(&traps[k]) && PAGE_OF(traps[k].address) == page) needed = 1;
        mprotect((void *)page, PAGE, needed ? PROT_READ : PROT_READ | PROT_WRITE);
    }
}
