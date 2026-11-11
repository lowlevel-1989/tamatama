/* platform/platform.h: lo que el programa necesita de la plataforma de destino.
   Cada carpeta de platform/ define estas funciones, y el Makefile compila una
   sola (PLATFORM):
     linux  PC y Raspberry Pi
   Un lanzador para Android o para una consola aporta su propia carpeta, y
   apk.c, asset_open y el gestor de resources siguen igual. */
#pragma once

const char *base_dir(void);                   /* la carpeta del programa, sin
                                                 barra final */
int apk_each(int (*use)(const char *path)); /* prueba cada .apk aportado hasta
                                               que use da 1 */
/* Qué tiene que hacer el usuario para aportar el .apk, en una línea en inglés:
   el sitio donde lo busca apk_each. */
const char *apk_hint(void);
/* Donde el programa puede escribir, sin barra final: lo extraído del .apk y las
   partidas. En
   Android o en una consola no es la carpeta del programa, que es de solo
   lectura. */
const char *data_dir(void);
/* Un mensaje ya formateado, de log_msg o de die: en Linux va a stderr, en
   Android iría a logcat. */
void platform_log(const char *text);
/* Una captura de pantalla ya guardada en path (PNG). En Linux no hace nada:
   ya está en screenshots/. En Android la copia a la galería. */
void platform_screenshot_saved(const char *path);
/* Vibra el aparato ms milisegundos, si la plataforma sabe hacerlo mejor
   que SDL: 1 si ha vibrado. En Linux, 0 (SDL lo hace). En Android vibra el
   teléfono como vibración de un juego: la de SDL cuenta como respuesta al
   touch, y muchos teléfonos la tienen apagada. */
int platform_vibrate(int ms);
/* Abre url en el navegador del sistema: 1 si lo ha intentado. En Linux,
   xdg-open (en una consola sin navegador no hace nada); en Android, un
   Intent. */
int platform_open_url(const char *url);
