/* debug/console_default.c: lo que el programa usa de la consola de debug, para cuando se
   compila sin ella (make DEBUG_CONSOLE=0), por ejemplo en una plataforma sin sockets Unix. Son
   definiciones débiles: si debug_server.c está compilado, el linker elige las suyas.

   Sin consola, el seguimiento de variables (watch.c) sigue funcionando y sus cambios salen por
   el log. --debug y la regla pause avisan y no hacen nada. */
#include "debug.h"

/* Avisa una vez de que este build no tiene consola. */
__attribute__((weak)) void debug_start(const char *name, int start_paused) {
    (void)name;
    (void)start_paused;
    log_msg("[debug] this build has no debug console (DEBUG_CONSOLE=0): --debug is ignored\n");
}
/* Solo comprueba las variables vigiladas. */
__attribute__((weak)) void debug_after_call(const char *method) { watch_poll(method); }
/* Sin consola no hay pausa, que es lo único que hace aquí. */
__attribute__((weak)) void debug_after_native(const char *method) { (void)method; }
/* Sin consola no se puede parar: avisa la primera vez. */
__attribute__((weak)) void debug_pause_at(const char *method) {
    static int warned;
    if (!warned) log_msg("[debug] pause %s: the pause rule needs the debug console\n", method);
    warned = 1;
}
/* Lo que solo iba a la consola: no hay nadie a quien enviarlo. */
__attribute__((weak)) void debug_send(const char *format, ...) { (void)format; }
/* Lo que iba a la consola y a la terminal: solo a la terminal. */
__attribute__((weak)) void debug_report(const char *format, ...) {
    char text[512];
    va_list args;
    va_start(args, format);
    int n = vsnprintf(text, sizeof text, format, args);
    va_end(args);
    if (n > 0) log_msg("%s", text);
}
