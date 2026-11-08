/* platform/platform.h: lo que el programa necesita de la plataforma de destino. Cada carpeta de
   platform/ define estas funciones, y el Makefile compila una sola (PLATFORM):
     linux  PC y Raspberry Pi
   Un lanzador para Android o para una consola aporta su propia carpeta, y apk.c, asset_open y
   el gestor de resources siguen igual. */
#pragma once

const char *base_dir(void);                   /* la carpeta del programa, sin barra final */
int apk_each(int (*use)(const char *path)); /* prueba cada .apk aportado hasta que use da 1 */
/* Qué tiene que hacer el usuario para aportar el .apk, en una línea en inglés: el sitio donde lo
   busca apk_each. */
const char *apk_hint(void);
/* Donde el programa puede escribir, sin barra final: lo extraído del .apk y las partidas. En
   Android o en una consola no es la carpeta del programa, que es de solo lectura. */
const char *data_dir(void);
/* Un mensaje ya formateado, de log_msg o de die: en Linux va a stderr, en Android iría a logcat. */
void platform_log(const char *text);
