/* arm/arm_main.c: el programa ARM. Corre directamente en una Raspberry Pi y
   bajo QEMU en x86_64. Contiene el ELF loader, los hooks y, cuando no hay
   loader, también tus funciones nativas. */
#include "arm.h"
#include "bridge.h"
#include "debug.h"

/* Dónde quedó cargado el .so. */
static uint32_t so_base;
arm_ptr arena_address(void);

/* Atiende las peticiones del loader. Es reentrante: una función nativa puede
   volver a llamar a código ARM mientras otra llamada sigue en curso. Con
   until_native_done = 1 vuelve al terminar la función nativa en curso, y deja
   su resultado en regs. */

// Se llama desde dos sitios:
//   arm/arm_main.c  run()         serve_host_requests(0, 0)
//                                 el loop principal: no vuelve nunca
//   arm/hooks.c     run_native()  serve_host_requests(1, r)
//                                 espera a una nativa: vuelve con el return
void serve_host_requests(int until_native_done, uint32_t *regs) {
    for (;;) {
        uint32_t header[2];
        read_all(FD_FROM_HOST, header, 8);
        uint32_t reply[3] = {TO_HOST_RETURN, 0, 0};
        if (header[0] == TO_ARM_EXIT) {
            hooks_report();
            exit(0);
        }

        // Es para esta opción
        // arm/hooks.c  run_native()  serve_host_requests(1, r)
        //                            espera a una nativa: vuelve con el return
        if (header[0] == TO_ARM_NATIVE_DONE) { /* la función nativa ha
                                                  terminado */
            uint32_t result[2];
            read_all(FD_FROM_HOST, result, 8);
            if (!until_native_done) die("unexpected end of native call");
            regs[0] = result[0];
            regs[1] = result[1];
            return;
        }
        uint8_t *payload = malloc(header[1] + 1);
        read_all(FD_FROM_HOST, payload, header[1]);
        payload[header[1]] = 0;
        uint64_t answer = 0;
        if (header[0] == TO_ARM_CALL_ADDRESS) { /* [dirección, palabras...] */
            uint32_t *fields = (uint32_t *)payload;
            answer = call_arm_at(fields[0], fields + 1, (int)(header[1] / 4 - 1));
        } else if (header[0] == TO_ARM_CALL_NAME) {
            /* [nombre\0, palabras aligned a 4] */
            uint32_t name_bytes = (strlen((char *)payload) + 4) & ~3u;
            answer = call_arm_by_name((char *)payload, (uint32_t *)(payload + name_bytes), (int)((header[1] - name_bytes) / 4));
        } else if (header[0] == TO_ARM_SYMBOL) {
            answer = arm_symbol((char *)payload);
        } else if (header[0] == TO_ARM_ALLOC) {
            answer = (arm_ptr)arena_malloc(*(uint32_t *)payload);
        }
        free(payload);
        reply[1] = (uint32_t)answer;
        reply[2] = (uint32_t)(answer >> 32);
        write_all(FD_TO_HOST, reply, sizeof reply);
    }
}
/* Las llamadas de app_main al código ARM pasan por aquí, en el PC y en la
   Raspberry Pi. Al terminar, los threads del juego tienen su turno. */
static uint64_t call_arm(void *target, const uint32_t *words, int word_count) {
    thread_call_begin();
    uint64_t result = call_words(target, words, (uint32_t)word_count);
    thread_call_end();
    return result;
}
/* La API compartida, versión ARM: todo está en este proceso. */
uint64_t call_arm_at(arm_ptr target, const uint32_t *words, int word_count) {
    return call_arm((void *)target, words, word_count);
}
/* Busca el método entre los registrados con RegisterNatives, después entre los
   símbolos del .so y por último entre lo que el loader le da al .so (bionic), y
   lo ejecuta. Lo último sirve, por ejemplo, para que app_main llame a usleep y
   deje correr los threads del juego. */
uint64_t call_arm_by_name(const char *name, const uint32_t *words, int count) {
    void *target = jni_registered(name);
    if (!target) target = so_find(name);
    if (!target) {
        int is_data;
        const void *own = bionic_find(name, &is_data);
        if (own && !is_data) target = (void *)own;
    }
    if (!target) {
        log_msg("[loader] %s does not exist\n", name);
        die("method not found");
    }
    return call_arm(target, words, count);
}
/* Si el símbolo tiene hook, las vtables contienen el código del hook: esa es la
   dirección que hay que devolver, para que las comparaciones con el contenido
   de una vtable funcionen. */
arm_ptr arm_symbol(const char *name) {
    return is_hooked(name) ? (arm_ptr)hook_without_target(name) : (arm_ptr)so_find(name);
}
/* El JNIEnv que hay que pasar a los métodos. */
arm_ptr arm_jni_env(void) { return (arm_ptr)jni_env(); }
/* Dónde quedó cargado el .so. */
arm_ptr arm_so_base(void) { return so_base; }
/* En este proceso se ve toda la memoria, así que solo se descartan las
   direcciones bajas, que nunca están mapeadas. Un pointer roto más alto haría
   fallar el programa al leerlo. */
int arm_readable(arm_ptr address) { return address >= 0x10000u; }
/* Dónde tiene sentido buscar: desde el .so hasta lo último reservado en la
   arena. */
void arm_memory_range(arm_ptr *start, uint32_t *bytes) {
    arm_ptr arena = arena_address();
    *start = so_base ? so_base : arena;
    *bytes = arena + *(uint32_t *)to_local(arena) - *start;
}
/* Rutas del .so y del fichero de reglas, tomadas de la línea de órdenes. */
static const char *so_path, *rules_path;
static int debug_mode;
/* --trace: registra cada llamada a una función con hook. */
static int trace;
static int load_only; /* --load-only: se detiene tras JNI_OnLoad e imprime el
                         informe */

/* El cuerpo del programa: lee las reglas, carga el .so, crea la tabla JNI,
   avisa al loader, ejecuta los constructores y JNI_OnLoad, y después atiende
   órdenes o ejecuta tu programa. No vuelve. */
static void run(void *arg) {
    (void)arg;
    if (rules_path) rules_load(rules_path);
    if (trace) trace_enable(); /* después de las reglas, para que la opción
                                  mande */
    int with_so = !LOADER_NATIVE; /* con NATIVE=1 no se carga el .so */
    if (with_so) so_base = elf_load(so_path);
    arm_ptr env = (arm_ptr)jni_env(); /* la tabla JNI debe existir antes de
                                         JNI_OnLoad */
    if (has_host) {
        /* Primer mensaje: dónde está cada cosa. Debe ir antes de los
           constructores, porque estos ya pueden llamar a funciones nativas. */
        uint32_t hello[4] = {TO_HOST_HELLO, arena_address(), so_base, env};
        write_all(FD_TO_HOST, hello, sizeof hello);
    }
    if (with_so) {
        elf_run_constructors();
        int (*on_load)(void *, void *) = so_find("JNI_OnLoad");
        if (on_load) log_msg("[loader] JNI_OnLoad -> 0x%x\n", on_load(jni_vm(), 0));
    }
    if (load_only) {
        hooks_report();
        exit(0);
    }

    // Ejecuta modo serve
    if (has_host) {
        uint32_t ready[3] = {TO_HOST_RETURN, 0, 0};
        write_all(FD_TO_HOST, ready, sizeof ready);

        // se queda a la escucha por siempre y nunca vuelve
        // loop infinito
        // procesa los requests enviados desde el HOST x86_64
        // El proceso finaliza dentro del mismo serve
        serve_host_requests(0, 0);
    }

    // Solo lo ejecuta cuando se ejecuta sin qemu, es decir sin host.
    // Totalmente nativo, el loader es reemplazado por loader-arm
    //
    // loader -> loader-arm -> app_main
    // pasa a ser
    // loader-arm -> app_main
    if (debug_mode) debug_start("loader.sock", debug_mode == 2);
    int rc = app_main();
    hooks_report();
    exit(rc);
}
/* Punto de entrada: interpreta las opciones, reserva la arena y arranca run.
   Opciones: --probe termina con el código 42, para la detección del entorno.
   --serve indica que hay un loader. --load-only, --trace, --callers, --debug y
   --help son las del programa. */
int main(int argc, char **argv) {
    const char *game = NULL;
    /* Las opciones pueden ir en cualquier posición. El primer argumento que no
       es una opción es el juego; los demás se ignoran. */
    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--", 2)) {
            if (!game) game = argv[i];
            continue;
        }
        const char *rules = option_rules(argc, argv, &i);
        int debug_option = rules ? 0 : option_debug(argc, argv, &i);
        if (rules) rules_path = rules;
        else if (debug_option) debug_mode = debug_option;
        else if (!strcmp(argv[i], "--probe")) _exit(42);
        else if (!strcmp(argv[i], "--serve")) has_host = 1;
        else if (!strcmp(argv[i], "--load-only")) load_only = 1;
        else if (!strcmp(argv[i], "--trace")) trace = 1;
        else if (!strcmp(argv[i], "--callers")) callers_enable();
        else if (!strcmp(argv[i], "--help")) {
            usage("loader-arm", "  --load-only    stop after JNI_OnLoad and print the report\n"
                                "  --trace        log every call to a hooked function\n"
                                "  --callers      show where each Java method is called from\n"
                                "  --debug        open the debug console (connect with tools/console.py)\n"
                                "  --debug pause  the same, and wait for the console before the first method\n"
                                "                 (also --debug=pause)\n"
                                "  --probe, --serve  used by loader, not by hand\n",
                  "");
            return 0;
        }
    }
    /* Sin game.apk|game.so, apk_library busca el juego. Sin --rules, las de la
       versión. Con --serve, loader ya pasa el .so y las reglas resueltos, y la
       versión solo la necesita él. */
    so_path = has_host ? game : apk_library(game);
    if (!rules_path && !has_host) rules_path = apk_rules();
    if (!has_host) log_msg("[env] arm: game code on this CPU, native code in this process\n");
    arena_init(has_host);
    bionic_init();
    /* Los primeros 8 MiB de la arena se reservan para el stack, de 0x60000000 a
       0x60800000, y así el .so empieza justo después: en 0x60800000 si la arena
       está donde se pidió. Solo el loader necesita ese stack, porque debe leer
       los argumentos pasados por él. Sin loader, el programa se queda en el
       stack que preparó libc. */
    uint8_t *stack = arena_take(0x800000, 4096);
    if (has_host) switch_stack(stack + 0x800000, run, 0);
    run(0);
    return 0;
}
