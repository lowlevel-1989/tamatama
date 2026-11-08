/* shared/usage.c: las opciones que comparten loader y loader-arm, y el texto de
   --help. La parte común (el juego y las reglas) está aquí; cada programa añade
   sus opciones y, si las tiene, sus variables de entorno. Los mensajes van en
   inglés, como el resto de la consola. */
#include "common.h"
#include "platform.h"

void usage(const char *program, const char *options, const char *environment) {
    log_msg("usage: %s [options] [game.apk|game.so]\n"
            "\n"
            "  (no game)  use the version already extracted, or find the original .apk:\n"
            "             %s\n"
            "  game.apk   check its md5 against the supported versions, extract it and run it\n"
            "  game.so    run this .so. If it was extracted from a supported .apk, its version\n"
            "             is known from resources/registry.txt\n"
            "\n"
            "Options:\n"
            "  --rules FILE   rules file, relative to the current folder (also --rules=FILE).\n"
            "                 Without it, and when the version is known, rules/{md5}.txt next to\n"
            "                 the program is loaded if it exists\n"
            "%s"
            "  --help         show this help\n"
            "%s%s",
            program, apk_hint(), options, *environment ? "\nEnvironment:\n" : "", environment);
}

/* --rules FICHERO o --rules=FICHERO. Si argv[*i] es esta opción, devuelve el
   fichero y deja *i en el último argumento que ha usado; si no, devuelve
   NULL. */
const char *option_rules(int argc, char **argv, int *i) {
    if (!strncmp(argv[*i], "--rules=", 8)) return argv[*i] + 8;
    if (strcmp(argv[*i], "--rules")) return NULL;
    if (*i + 1 >= argc) die("--rules needs a file: --rules FILE");
    return argv[++*i];
}

/* --debug, --debug pause o --debug=pause. Devuelve 0 si argv[*i] no es esta
   opción, 1 sin pausa y 2 con pausa. Con espacio, el argumento siguiente solo
   se toma si es pause: si no, es el juego. */
int option_debug(int argc, char **argv, int *i) {
    if (!strcmp(argv[*i], "--debug=pause")) return 2;
    if (strcmp(argv[*i], "--debug")) return 0;
    if (*i + 1 < argc && !strcmp(argv[*i + 1], "pause")) {
        ++*i;
        return 2;
    }
    return 1;
}
