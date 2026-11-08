/* arm/hooks.c: el punto por el que pasa toda función con hook. Solo se compila en loader-arm. */
#include "arm.h"
#include "bridge.h"
#include "debug.h"

/* El código y los datos van en mappings separados. Si el contador compartiera página con
   las instrucciones, cada llamada obligaría a QEMU a tirar su traducción de esa página y
   rehacerla. */
typedef struct HookThunk {
    uint32_t code[2]; /* ldr ip, [pc, #0] ; ldr pc, [pc, #0] */
    uint32_t literal[2];  /* el Hook de este código ; hook_common */
} HookThunk;
struct Hook {
    HookThunk *thunk;
    const char *name;
    void *target;      /* nuestra implementación, si la hay */
    uint32_t ret;      /* valor que devuelve un stub */
    uint32_t calls;    /* para el informe final */
    int mode;          /* qué hace dispatch con él */
    int native;        /* posición en native_table, o -1 */
    int unimplemented; /* 1: es un stub solo porque nadie lo ha escrito aún */
};
/* Lo que puede hacer un hook: saltar a nuestra implementación, devolver un valor o ir a una
   función nativa. */
enum { DO_TARGET, DO_STUB, DO_NATIVE, DO_EXIT };
/* 4096 hooks: 64 KiB de código y 128 KiB de datos. */
#define MAX_HOOKS 4096
/* Las dos zonas, y cuántos hooks hay. */
static Hook *hooks;
static HookThunk *hook_thunks;
static int hook_count;
int has_host; /* 1: las funciones nativas están en otro proceso (x86_64) */
/* Está en asm/hook_common.s. */
extern void hook_common(void);
/* Crea, o reutiliza, la entrada de un nombre y decide qué hace. La primera vez reserva las dos
   zonas: el código en HOOK_CODE_ADDRESS y los datos en HOOK_DATA_ADDRESS. El hook número k
   queda en HOOK_CODE_ADDRESS + 16 k. */
static Hook *hook_entry(const char *name, void *target) {
    if (!hooks) {
        hook_thunks = map_at(HOOK_CODE_ADDRESS, sizeof(HookThunk) * MAX_HOOKS, 7);
        hooks = map_at(HOOK_DATA_ADDRESS, sizeof(Hook) * MAX_HOOKS, 3);
    }
    for (int i = 0; i < hook_count; i++)
        if (!strcmp(hooks[i].name, name)) return &hooks[i];
    if (hook_count == MAX_HOOKS) die("too many hooks");
    Hook *hook = &hooks[hook_count];
    HookThunk *thunk = &hook_thunks[hook_count];
    hook_count++;
    /* En modo ARM, leer pc da la dirección de la instrucción + 8. */
    thunk->code[0] = 0xE59FC000; /* ldr ip, [pc, #0]: ip = lit[0] */
    thunk->code[1] = 0xE59FF000; /* ldr pc, [pc, #0]: pc = lit[1] */
    thunk->literal[0] = (uint32_t)hook;
    thunk->literal[1] = (uint32_t)hook_common;
    flush_code(thunk, sizeof *thunk);
    hook->thunk = thunk;
    hook->name = name;
    hook->target = target;
    hook->native = native_active(name);
    const Rule *r = rule_find(name);
    /* La decisión: una regla stub, después tu función nativa y después nuestra implementación.
       Sin nada de eso, es un stub por defecto. */

    /*
     * El orden es importante y es el siguiente:
     *
     * El orden completo: 
     * 1. las reglas (stub, exit): mandan sobre todo lo demás.
     * 2. las natives.
     * 3. Sin implementación: un stub por defecto, avisando con UNIMPLEMENTED.
     * 4. Lo que implementa el loader.
     */
    if (r && r->mode == RULE_STUB) {
        hook->mode = DO_STUB;
        hook->ret = r->ret;
    } else if (r && r->mode == RULE_EXIT) {
        hook->mode = DO_EXIT;
    } else if (hook->native >= 0) {
        hook->mode = DO_NATIVE;
    } else if (!target) {
        hook->mode = DO_STUB;
        hook->unimplemented = 1;
    }
    return hook;
}

/* El código de un hook para una importación. target es nuestra implementación, o NULL. */
void *hook_with_target(const char *name, void *target) { return hook_entry(name, target)->thunk; }
/* El código de un hook para una función del propio .so. */
void *hook_without_target(const char *name) { return hook_entry(name, 0)->thunk; }

/* Lo que importa el .so: el dato real, o un hook si es una función. */
void *resolve_import(const char *name) {
    int is_data;
    const void *implementation = bionic_find(name, &is_data);
    return is_data ? (void *)implementation : hook_with_target(name, (void *)implementation);
}
/* Ejecuta una función nativa, esté donde esté. r contiene r0 r1 r2 r3 y recibe el resultado. */
void run_native(int index, uint32_t *r, uint32_t sp) {
    if (has_host) {
        /* x86_64: la función está en el otro proceso. */
        uint32_t call[7] = {TO_HOST_NATIVE_CALL, (uint32_t)index, r[0], r[1], r[2], r[3], sp};
        write_all(FD_TO_HOST, call, sizeof call);
        serve_host_requests(1, r); /* atiende peticiones hasta que termine */
    } else {
        /* ARM: la función está aquí mismo. */
        Regs regs = {{r[0], r[1], r[2], r[3]}, sp};
        native_table[index].fn(&regs);
        debug_after_native(native_table[index].name);
        r[0] = regs.r[0];
        r[1] = regs.r[1];
    }
}
/* Escribe los argumentos de una llamada: con sus nombres, si están en android_emu/known_args.c, o los
   cuatro registers, si no. Los que no caben en r0-r3 están en el stack, justo encima de lo
   guardado: regs[4] y regs[5] son r4 y lr, así que el quinto argumento es regs[6]. */
static void format_args(const Hook *hook, const uint32_t *regs, char *text, size_t room) {
    const char *const *names = known_args(hook->name);
    if (!names) {
        snprintf(text, room, "0x%x, 0x%x, 0x%x, 0x%x", regs[0], regs[1], regs[2], regs[3]);
        return;
    }
    size_t used = 0;
    for (int i = 0; names[i] && used < room; i++) {
        const char *separator = i ? ", " : "";
        if (!strcmp(names[i], "..."))
            used += (size_t)snprintf(text + used, room - used, "%s...", separator);
        else
            used += (size_t)snprintf(text + used, room - used, "%s%s=0x%x", separator, names[i],
                                     i < 4 ? regs[i] : regs[i + 2]);
    }
}
/* La llama hook_common. regs = r0 r1 r2 r3 r4 lr, guardados en el stack. Devuelve adónde hay que
   seguir, o 0 si la llamada ya está resuelta. */
void *dispatch(Hook *hook, uint32_t *regs) {
    hook->calls++;
    if (trace_enabled() || hook->mode == DO_EXIT || (hook->mode == DO_STUB && hook->calls == 1)) {
        char text[160], from[160];
        format_args(hook, regs, text, sizeof text);
        elf_format_address(regs[5], from, sizeof from);
        log_msg("[%s] %s(%s) from %s\n",
                hook->unimplemented       ? "UNIMPLEMENTED"
                : hook->mode == DO_EXIT   ? "exit"
                : hook->mode == DO_NATIVE ? "native"
                : hook->mode == DO_STUB   ? "stub"
                                       : "trace",
                hook->name, text, from);
    }
    uint32_t sp = (uint32_t)regs + 24; /* justo encima de lo guardado: los argumentos del stack */
    switch (hook->mode) {
    case DO_NATIVE: run_native(hook->native, regs, sp); return 0;
    case DO_STUB:
        regs[0] = hook->ret;
        regs[1] = 0;
        return 0;
    case DO_EXIT:
        hooks_report();
        exit(0);
    default: return hook->target;
    }
}
/* El resumen que se imprime al salir. Es tu lista de tareas. */
void hooks_report(void) {
    log_msg("[report] hooked functions: %d\n", hook_count);
    for (int i = 0; i < hook_count; i++)
        if (hooks[i].calls) {
            Hook *hook = &hooks[i];
            log_msg("[report] %u  %s%s\n", hook->calls, hook->name,
                    hook->unimplemented       ? "  (unimplemented)"
                    : hook->mode == DO_NATIVE ? "  (native)"
                    : hook->mode == DO_STUB   ? "  (stub)"
                                           : "");
        }
}
