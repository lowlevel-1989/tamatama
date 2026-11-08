/* shared/support.c: pequeños servicios que comparten los dos programas, sobre
   libc. */
#include "common.h"
#include "platform.h"
#include <time.h>

/* Mensaje de diagnóstico: se formatea aquí y la plataforma decide adónde va.
   Casi todos caben en el buffer del stack; uno más largo se formatea otra vez
   en memoria pedida. */
void log_msg(const char *f, ...) {
    char text[1024];
    va_list a;
    va_start(a, f);
    int n = vsnprintf(text, sizeof text, f, a);
    va_end(a);
    if (n < 0) return;
    if ((size_t)n < sizeof text) {
        platform_log(text);
        return;
    }
    char *long_text = malloc((size_t)n + 1);
    if (!long_text) return;
    va_start(a, f);
    vsnprintf(long_text, (size_t)n + 1, f, a);
    va_end(a);
    platform_log(long_text);
    free(long_text);
}
/* Muestra el error y termina. */
void die(const char *m) {
    log_msg("[loader] ERROR: %s\n", m);
    exit(1);
}
/* Reloj monótono, en nanosegundos. */
uint64_t clock_ns(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec * 1000000000u + (uint64_t)t.tv_nsec;
}
/* Lee un fichero entero y añade un 0 al final. Devuelve NULL si no existe. */
char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);
    char *b = malloc((size_t)size + 1);
    size_t n = fread(b, 1, (size_t)size, f);
    b[n] = 0;
    fclose(f);
    return b;
}
