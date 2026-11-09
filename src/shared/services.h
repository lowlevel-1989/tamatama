/* shared/services.h: los servicios que necesita el código del juego, sin nada
   de ARM. Es lo único que incluye src/port/game/ (junto con platform.h): así
   ese código no puede depender de Regs, arm_ptr ni to_local, y servirá tal cual
   cuando el juego sea nativo del todo. common.h lo incluye, así que el resto
   del programa también los tiene. */
#pragma once
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

void log_msg(const char *fmt, ...); /* mensaje de diagnóstico: la plataforma
                                       decide adónde va */
void die(const char *msg);          /* mensaje y salida del programa */
uint64_t clock_ns(void);            /* reloj monótono, en nanosegundos */
char *read_file(const char *path);  /* contenido terminado en 0, o NULL */
/* ruta de assets/name; -1 si no vale */
int asset_path(char *out, size_t room, const char *name);
/* assets/name para leer, o NULL */
FILE *asset_open(const char *name);
/* Un PNG en RGBA, 4 bytes por píxel, fila a fila, en memoria de malloc que se
   libera con free. NULL si no es un PNG que se sepa leer (shared/png.c). */
uint8_t *png_decode(const uint8_t *data, size_t size, int *width, int *height);

#ifdef __cplusplus
}
#endif
