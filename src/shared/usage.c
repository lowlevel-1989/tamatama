/* shared/usage.c: el texto de --help. La parte común (el juego y las reglas) está aquí; cada
   programa añade sus opciones y, si las tiene, sus variables de entorno. Los mensajes van en
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
            "  --rules=FILE   rules file, relative to the current folder. Without it, and when\n"
            "                 the version is known, rules/{md5}.txt next to the program is loaded\n"
            "                 if it exists\n"
            "%s"
            "  --help         show this help\n"
            "%s%s",
            program, apk_hint(), options, *environment ? "\nEnvironment:\n" : "", environment);
}
