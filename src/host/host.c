/* host/host.c: el programa loader, de 64 bits. No carga el .so ni ejecuta ARM:
   decide quién lo hace y ejecuta las funciones nativas.

   En x86_64 hay dos procesos a la vez:
     loader      este programa: tu código y las funciones nativas, en x86_64
     loader-arm  el .so y lo que él espera de Android, en ARM bajo QEMU

   Ninguno puede llamar directamente a una función del otro, así que se
   comunican de dos formas:
     - Dos pipes, para las órdenes y los resultados: solo viajan números, como
       registers y direcciones. recv_from_arm y send_to_arm las usan.
       Esta técnica se llama RPC.
     - Una arena de shared memory, para los datos: los objetos, los vértices y
       el stack del juego se leen allí directamente con to_local, sin copiarlos.

   Los mensajes están definidos en shared/common.h: TO_ARM_* van de aquí a
   loader-arm y TO_HOST_* vienen de loader-arm hacia aquí. Todo va en palabras
   de 32 bits. */
#include <limits.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include "common.h"
#include "bridge.h"
#include "environment.h"
#include "platform.h"
#include "debug.h"

/* La misma arena tiene dos direcciones: dónde la ve este proceso (la elige
   Linux) y dónde la ve loader-arm (0x60000000 si estaba libre). to_local y
   to_arm convierten entre las dos. */
uint8_t *arena_local;
arm_ptr arena_arm;
/* Los pipes con loader-arm. Valen -1 si no hay .so, y entonces no hay nadie con
   quien hablar. */
static int pipe_to_arm = -1, pipe_from_arm = -1;
/* El proceso de QEMU. */
static pid_t qemu_pid;
/* El JNIEnv del juego y la base del .so, tal como los ve loader-arm. Llegan en
   su primer mensaje. */
static arm_ptr jni_env_arm;
static arm_ptr so_base_arm;
/* Cuánto se ha entregado de la arena cuando no hay .so. */
static uint32_t arena_used;

/* loader-arm está junto a este programa, no en el directorio actual. */
static const char *loader_arm_path(void) {
    static char path[PATH_MAX];
    if (!path[0] && snprintf(path, sizeof path, "%s/loader-arm", base_dir()) >= (int)sizeof path)
        die("the program folder path is too long");
    return path;
}

/* ¿Puede este sistema ejecutar programas ARM de 32 bits sin ayuda? Lo averigua
   probando: lanza loader-arm --probe, que termina con el código 42. */
static int can_run_arm32(void) {
    pid_t child = fork();
    if (child == 0) {
        fclose(stderr); /* si no puede, no queremos ver el error */
        execl(loader_arm_path(), loader_arm_path(), "--probe", (char *)NULL);
        _exit(127);
    }
    int status = 0;
    waitpid(child, &status, 0);
    return WIFEXITED(status) && WEXITSTATUS(status) == 42;
}
/* Decide quién ejecuta el ARM. Combina lo fijado al compilar con lo que dice el
   kernel, y la variable LOADER_ARM_RUNNER permite forzar la decisión. */
static ArmRunner choose_arm_runner(void) {
    Environment environment = build_environment();
    struct utsname system_info;
    uname(&system_info);
    log_msg("[env] built for %s, kernel %s\n", environment_name(environment), system_info.machine);
    const char *forced = getenv("LOADER_ARM_RUNNER");
    if (forced) return !strcmp(forced, "cpu") ? ARM_ON_CPU : ARM_ON_QEMU;
    /* una CPU x86_64 no puede ejecutar ARM */
    if (environment == ENV_X86_64) return ARM_ON_QEMU;
    if (environment == ENV_ARM64 && can_run_arm32()) return ARM_ON_CPU;
    return ARM_ON_QEMU; /* aarch64 sin soporte de 32 bits */
}

static void die_errno(const char *message) {
    perror(message);
    exit(2);
}

/* Recibe exactamente n bytes de loader-arm por el pipe. 
 * Se bloquea hasta que lleguen. 
 * Si loader-arm ha terminado o ha fallado, read devuelve 0: 
 * el motivo ya estará en sus mensajes. */
static void recv_from_arm(void *buffer, size_t n) {
    char *cursor = buffer;
    while (n) {
        ssize_t got = read(pipe_from_arm, cursor, n);
        if (got == 0) {
            int status;
            if (waitpid(qemu_pid, &status, 0) == qemu_pid && WIFEXITED(status) && !WEXITSTATUS(status))
                exit(0);
            die("loader-arm has exited: look at the messages above");
        }
        if (got < 0) die_errno("reading from loader-arm");
        cursor += (size_t)got;
        n -= (size_t)got;
    }
}

/* Envía exactamente n bytes a loader-arm por el pipe. */
static void send_to_arm(const void *buffer, size_t n) {
    const char *cursor = buffer;
    while (n) {
        ssize_t sent = write(pipe_to_arm, cursor, n);
        if (sent <= 0) die_errno("writing to loader-arm");
        cursor += (size_t)sent;
        n -= (size_t)sent;
    }
}

/* 
 * Espera el resultado de lo que se le pidió a loader-arm. 
 * Es un loop porque, mientras tanto, el código ARM puede llamar 
 * a funciones nativas. Cada una llega como TO_HOST_NATIVE_CALL, 
 * se ejecuta aquí y su resultado vuelve con TO_ARM_NATIVE_DONE. 
 *
 * Y esa función nativa puede volver a pedir algo a loader-arm: 
 * entonces esta función se llama a sí misma a través de ella. 
 *
 * Por eso las llamadas pueden anidarse a cualquier profundidad. 
 * El loop acaba al llegar TO_HOST_RETURN. */
static uint64_t wait_for_arm_result(void) {
    for (;;) {
        uint32_t tag;
        recv_from_arm(&tag, 4);
        if (tag == TO_HOST_RETURN) {
            uint32_t result[2];
            recv_from_arm(result, 8);
            /* En ARM de 32 bits el resultado de una función siempre vuelve
             * en los registers R0 y R1. 
             *
             * Lo fija AAPCS, la convención de llamadas de ARM.
             */
            return result[0] | ((uint64_t)result[1] << 32);
        }

        /* Punto seguro al final del turno de un thread: watch, pausas y
           consola, como al volver un call_method. Mientras tanto loader-arm
           espera, y puede atender lo que pida la consola. */
        if (tag == TO_HOST_SAFE_POINT) {
            debug_after_call("thread");
            uint32_t done[4] = {TO_ARM_NATIVE_DONE, 8, 0, 0};
            send_to_arm(done, sizeof done);
            continue;
        }

        // llama a una funcion nativa
        uint32_t call[6]; /* posición en native_table, r0, r1, r2, r3, sp */
        recv_from_arm(call, sizeof call);

        Regs regs = {
          {
          //    R0       R1       R2       R3
            call[1], call[2], call[3], call[4]
          }, 
          //  SP
          call[5]
        };

        native_table[call[0]].fn(&regs);
        debug_after_native(native_table[call[0]].name);

        // Enviamos el resultado
        uint32_t done[4] = {
          /* En ARM de 32 bits el resultado de una función siempre vuelve
           * en los registers R0 y R1. 
           *
           * Lo fija AAPCS, la convención de llamadas de ARM.
           */
          TO_ARM_NATIVE_DONE, 8, regs.r[0], regs.r[1]
        };
        send_to_arm(done, sizeof done);
    }
}

/* ---------- La API compartida, versión de 64 bits: 
 *
 * lo que pide el código nativo viaja por los pipe. 
 *
 * La versión ARM de estas funciones está en arm/arm_main.c 
 * y las ejecuta directamente. 
 */

/* Ejecuta código ARM en una dirección, como una entrada de vtable.
   Mensaje: TO_ARM_CALL_ADDRESS, bytes, dirección, argumentos. */
uint64_t call_arm_at(arm_ptr target, const uint32_t *words, int n) {
    uint32_t header[3] = {TO_ARM_CALL_ADDRESS, ((uint32_t)n + 1) * 4, target};
    send_to_arm(header, sizeof header);
    send_to_arm(words, (size_t)n * 4);
    return wait_for_arm_result();
}

/* Ejecuta en ARM un método, por su nombre. Sin .so no hay nadie que lo ejecute,
 * así que es un método que aún hay que migrar.
 * 
 * Mensaje: TO_ARM_CALL_NAME, bytes, nombre con su 0 y relleno hasta un 
 * múltiplo de 4, argumentos. */
uint64_t call_arm_by_name(const char *name, const uint32_t *words, int n) {
    if (pipe_to_arm < 0) {
        log_msg("[loader] %s has no native version\n", name);
        die("this function still needs to be migrated");
    }
    uint32_t name_bytes = ((uint32_t)strlen(name) + 4) & ~3u;
    char buffer[256] = {0};
    strcpy(buffer, name);
    uint32_t header[2] = {TO_ARM_CALL_NAME, name_bytes + (uint32_t)n * 4};
    send_to_arm(header, 8);
    send_to_arm(buffer, name_bytes);
    send_to_arm(words, (size_t)n * 4);
    return wait_for_arm_result();
}

/* Hace a loader-arm una pregunta cuyo argumento es un nombre y 
 * cuya respuesta es una dirección.
 *
 * Mensaje: operación, longitud del nombre, nombre sin 0. */
static arm_ptr ask_arm_by_name(uint32_t operation, const char *name) {
    if (pipe_to_arm < 0) return 0;
    uint32_t header[2] = {operation, (uint32_t)strlen(name)};
    send_to_arm(header, 8);
    send_to_arm(name, header[1]);
    return (arm_ptr)wait_for_arm_result();
}

/* La dirección que usan la PLT y las vtables para ese símbolo. 0 si no
   existe. */
arm_ptr arm_symbol(const char *name) { 
  return ask_arm_by_name(TO_ARM_SYMBOL, name); 
}

/* Memoria visible desde los dos programas. Con .so, la reserva loader-arm en su
   heap, y así un solo proceso administra la arena. Sin .so, la arena es toda de
   este proceso. */
arm_ptr shared_alloc(uint32_t n) {
    if (pipe_to_arm < 0) {
        arm_ptr address = arena_arm + arena_used;
        arena_used += (n + 7) & ~7u;
        if (arena_used > ARENA_BYTES) die("arena exhausted");
        return address;
    }
    uint32_t request[3] = {TO_ARM_ALLOC, 4, n};
    send_to_arm(request, sizeof request);
    return (arm_ptr)wait_for_arm_result();
}

/* El JNIEnv que hay que pasar a los métodos JNI. */
arm_ptr arm_jni_env(void) { return jni_env_arm; }
/* Dónde quedó cargado el .so. */
arm_ptr arm_so_base(void) { return so_base_arm; }
/* 1 si esta dirección del juego se puede leer desde aquí: debe estar dentro de
   la arena. */
int arm_readable(arm_ptr address) { return address - arena_arm < ARENA_BYTES; }
/* Dónde tiene sentido buscar: desde el .so hasta lo último reservado en la
   arena. loader-arm deja esa cuenta en la primera palabra de la arena. */
void arm_memory_range(arm_ptr *start, uint32_t *bytes) {
    uint32_t used = pipe_to_arm < 0 ? arena_used : *(uint32_t *)arena_local;
    *start = so_base_arm ? so_base_arm : arena_arm;
    *bytes = arena_arm + used - *start;
}


/* Crea la arena y los pipes, lanza loader-arm bajo QEMU y espera a que termine
   de inicializarse. Al volver, el .so ya está cargado y su JNI_OnLoad ya se ha
   ejecutado. */
static void start_qemu(const char *so_path, const char *rules_path, int trace, int callers) {
    int to_arm_fds[2], from_arm_fds[2];
    if (pipe(to_arm_fds) || pipe(from_arm_fds)) die_errno("pipe");

    /* 
     * Un memfd es un fichero anónimo en memoria: 
     * los dos procesos pueden proyectarlo y ver las mismas páginas. 
     * Ocupa 256 MiB de direcciones, pero solo gasta memoria lo que se toca. */
    int arena_fd = memfd_create("loader-arena", 0);
    if (arena_fd < 0 || ftruncate(arena_fd, ARENA_BYTES)) die_errno("memfd");


    /* 
     * Primer argumento NULL: Linux elige dónde ve este proceso la arena. 
     * Nada depende de esa dirección, y cambia en cada ejecución. */
    arena_local = mmap(NULL, ARENA_BYTES, PROT_READ | PROT_WRITE, MAP_SHARED, arena_fd, 0);
    if (arena_local == MAP_FAILED) die_errno("mmap");

    // Ejecutamos el proceso de qemu
    qemu_pid = fork();
    if (qemu_pid == 0) {
        /* 
         * El hijo pone cada descriptor en el número que espera loader-arm: 3, 4
         * y 5. 
         * Antes los copia con dup a números más altos, 
         * para no pisar ninguno de los que aún necesita. 
         * Después cierra el resto. Así, cuando este proceso cierre 
         * su extremo de los pipes, loader-arm verá el final de los pipes. */
        int from_host = dup(to_arm_fds[0]), 
            to_host   = dup(from_arm_fds[1]), 
            arena     = dup(arena_fd);


        // Apuntamos a los pipes originales y asi no repetimos
        // dup2(viejo, nuevo)
        dup2(from_host, FD_FROM_HOST);
        dup2(to_host,   FD_TO_HOST);
        dup2(arena,     FD_ARENA);

        // 0, 1, 2   stdin, stdout, stderr
        // 3         FD_FROM_HOST
        // 4         FD_TO_HOST
        // 5         FD_ARENA
        //
        // Se cierran todos los demas. 64 es solo por si abrimos
        // mas en un futuro y no queremos basura.
        for (int fd = 6; fd < 64; fd++) close(fd);

        const char *qemu = getenv("QEMU_ARM");
        if (!qemu) qemu = "qemu-arm";

       /* Modo --serve
        *
        * - Toma la arena del descriptor 5, 
        *   en lugar de reservarla para sí mismo.
        * - Usa un stack dentro de la arena, 
        *   para que loader pueda leer los argumentos.
        * - Envía los mensajes de arranque por el descriptor 4.
        * - Espera órdenes por el descriptor 3, 
        *   en lugar de ejecutar app_main.
        * - Envía las funciones nativas a loader,
        *   en lugar de ejecutarlas él.
        */

        const char *args[8];
        int count = 0;
        args[count++] = qemu;
        args[count++] = loader_arm_path();
        args[count++] = "--serve";
        if (trace) args[count++] = "--trace";
        if (callers) args[count++] = "--callers";
        /* Las reglas, ya resueltas, van como opción: antes del .so, que es el
           primer argumento que no empieza por --. */
        char rules_option[PATH_MAX + 16];
        if (rules_path) {
            snprintf(rules_option, sizeof rules_option, "--rules=%s", rules_path);
            args[count++] = rules_option;
        }
        args[count++] = so_path;
        args[count] = NULL;
        execvp(qemu, (char *const *)args);
        perror("cannot run qemu-arm");
        _exit(127);
    }

    /* El padre cierra los extremos del hijo y se queda con los suyos. */
    close(to_arm_fds[0]);
    close(from_arm_fds[1]);
    close(arena_fd);

    pipe_to_arm = to_arm_fds[1];
    pipe_from_arm = from_arm_fds[0];

    /* Primer mensaje: TO_HOST_HELLO, arena, base del .so, JNIEnv. */
    uint32_t hello[4];
    recv_from_arm(hello, sizeof hello);
    if (hello[0] != TO_HOST_HELLO) die("invalid hello");

    arena_arm   = hello[1];
    so_base_arm = hello[2];
    jni_env_arm = hello[3];
    log_msg("[bridge] arena: arm=0x%x, local=%p; .so=0x%x; JNIEnv=0x%x\n",
        hello[1],            // arm
        (void *)arena_local, // local
        hello[2],            // .s0
        hello[3]);           // JNIEnv

    /* Mientras loader-arm ejecuta los constructores y JNI_OnLoad, 
     * ya puede llamar a funciones nativas. Esta espera las atiende, 
     * hasta recibir su aviso de que está listo. */
    wait_for_arm_result();
}

/* Pide a loader-arm que imprima su informe y termine, y espera a que acabe. */
static void stop_qemu(void) {
    uint32_t request[2] = {TO_ARM_EXIT, 0};
    send_to_arm(request, 8);
    close(pipe_to_arm);
    int status;
    waitpid(qemu_pid, &status, 0);
}


/* Punto de entrada: 
 * interpreta las opciones, elige quién ejecuta el ARM y lanza el programa.
 *
 * Con --debug abre la consola de debug, y con --debug=pause 
 * el juego espera en el primer método a que le digas que siga.
 */
int main(int argc, char **argv) {
    int debug = 0, trace = 0, callers = 0;
    const char *rules_option = NULL, *game = NULL;
    /* Las opciones pueden ir en cualquier posición. El primer argumento que no
       es una opción es el juego; los demás se ignoran. */
    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--", 2)) {
            if (!game) game = argv[i];
            continue;
        }
        const char *rules = option_rules(argc, argv, &i);
        int debug_option = rules ? 0 : option_debug(argc, argv, &i);
        if (rules) rules_option = rules;
        else if (debug_option) debug = debug_option;
        else if (!strcmp(argv[i], "--trace")) trace = 1;
        else if (!strcmp(argv[i], "--callers")) callers = 1;
        else if (!strcmp(argv[i], "--help")) {
            usage("loader", "  --trace        log every call to a hooked function\n"
                            "  --callers      show where each Java method is called from\n"
                            "  --debug        open the debug console (connect with tools/console.py)\n"
                            "  --debug pause  the same, and wait for the console before the first method\n"
                            "                 (also --debug=pause)\n",
                  "  LOADER_ARM_RUNNER=cpu|qemu  who runs the ARM code, instead of detecting it\n"
                  "  QEMU_ARM=path               the qemu-arm to use\n");
            return 0;
        }
    }
    /* Sin game.apk|game.so, apk_library busca el juego. También con NATIVE=1:
       el .so no se carga, pero la versión dice dónde están los recursos y las
       reglas. */
    const char *so_path = apk_library(game);

    /* La CPU ejecuta ARM, así que loader-arm lo contiene todo: juego y código
       nativo. execv reemplaza este proceso por él, con los mismos argumentos, y
       no vuelve. Le llega el .apk (o nada), no el .so, para que él también sepa
       qué resources/{md5} usar. */
    if (!LOADER_NATIVE && choose_arm_runner() == ARM_ON_CPU) {
        log_msg("[env] game code on this CPU: handing over to loader-arm\n");
        argv[0] = (char *)loader_arm_path();
        execv(argv[0], argv);
        die_errno("cannot run loader-arm");
    }

    /* Este proceso también necesita las reglas: call_method las consulta para
       decidir si un método va a tu versión nativa o a ARM. Sin rules.txt, las
       de la versión, si las hay. Se buscan aquí, después del execv: en ese
       camino las busca loader-arm. */
    const char *rules_path = rules_option ? rules_option : apk_rules();
    if (rules_path) rules_load(rules_path);

    /* Migración terminada (NATIVE=1): no queda código ARM. La arena pasa a ser
       memoria normal, pero conserva las mismas direcciones del juego, así que
       el código nativo no tiene que cambiar. 0x60000000 ya es solo un número:
       nada se mapea ahí. */
    int exit_code;
    if (LOADER_NATIVE) {
        log_msg("[env] built with NATIVE=1: no .so, everything is native\n");
        arena_local = mmap(NULL, ARENA_BYTES, PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        arena_arm = ARENA_ADDRESS;
        arena_used = 8; /* para que ningún objeto reciba la dirección 0 de la
                           arena */
        if (debug) debug_start("loader.sock", debug == 2);
        exit_code = app_main();
    } else {
        log_msg("[env] game code under QEMU, native code here, sharing the arena\n");
        start_qemu(so_path, rules_path, trace, callers);
        if (debug) debug_start("loader.sock", debug == 2);
        exit_code = app_main();
        stop_qemu();
    }
    return exit_code;
}
