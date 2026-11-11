/* platform/linux/platform.c: la plataforma Linux, para el PC y la Raspberry Pi.
   Implementa platform/platform.h:

     base_dir()      la carpeta de la que cuelgan resources/, apk/ y loader-arm
     apk_each(use)   los .apk que puede haber aportado el usuario
     apk_hint()      cómo aportarlo: copiarlo en {base_dir}/apk/
     data_dir()      {base_dir}/resources: aquí la carpeta del programa se puede
                     escribir
     platform_log()  los mensajes, por stderr */
/* Offsets de 64 bits en readdir y fopen también en ARM de 32 bits: si no, bajo
   QEMU readdir falla con las carpetas de ext4 y no se abren ficheros de más de
   2 GiB. */
#define _FILE_OFFSET_BITS 64
#include "common.h"
#include "platform.h"
#include <dirent.h>
#include <limits.h>
#include <unistd.h>

/* La carpeta del ejecutable, sin la barra final. Se calcula una vez:
   /proc/self/exe es el programa en ejecución (bajo QEMU, qemu-arm responde con
   la ruta de loader-arm). ARENA_BASE la fija: en Android loader-arm está entre
   las librerías de la app, y sus datos en otra carpeta. */
const char *base_dir(void) {
    static char dir[PATH_MAX];
    const char *forced = getenv("ARENA_BASE");
    if (!dir[0] && forced && *forced) snprintf(dir, sizeof dir, "%s", forced);
    if (!dir[0]) {
        ssize_t n = readlink("/proc/self/exe", dir, sizeof dir - 1);
        if (n <= 0) die("cannot find the program folder: /proc/self/exe is not readable");
        dir[n] = 0;
        char *slash = strrchr(dir, '/');
        if (slash == dir) slash[1] = 0; /* el programa está en / */
        else if (slash) *slash = 0;
    }
    return dir;
}

/* Solo los ficheros que acaban en .apk, para scandir. */
static int is_apk(const struct dirent *e) {
    size_t n = strlen(e->d_name);
    return n > 4 && !strcmp(e->d_name + n - 4, ".apk");
}

/* Prueba los *.apk de {base}/apk/, en orden alfabético para que el resultado no
   dependa del disco, hasta que use devuelva 1. Devuelve 1 si alguno valió. */
int apk_each(int (*use)(const char *path)) {
    char dir[PATH_MAX], path[PATH_MAX];
    if (snprintf(dir, sizeof dir, "%s/apk", base_dir()) >= (int)sizeof dir) return 0;
    struct dirent **list;
    int count = scandir(dir, &list, is_apk, alphasort);
    if (count < 0) return 0; /* no hay carpeta apk/ */
    int found = 0;
    for (int k = 0; k < count; k++) {
        int fits = snprintf(path, sizeof path, "%s/%s", dir, list[k]->d_name) < (int)sizeof path;
        if (!found && fits && use(path)) found = 1;
        free(list[k]);
    }
    free(list);
    return found;
}

/* El sitio de apk_each, dicho al usuario. */
const char *apk_hint(void) {
    static char text[PATH_MAX + 64];
    snprintf(text, sizeof text, "Copy the original .apk of the game into %s/apk/", base_dir());
    return text;
}

/* {base_dir}/resources. Se calcula una vez. */
const char *data_dir(void) {
    static char dir[PATH_MAX];
    if (!dir[0] && snprintf(dir, sizeof dir, "%s/resources", base_dir()) >= (int)sizeof dir)
        die("the program folder path is too long");
    return dir;
}

/* Sin buffer: stderr ya escribe cada mensaje en cuanto llega. */
void platform_log(const char *text) { fputs(text, stderr); }

/* En Linux la captura ya está donde el usuario la busca. */
void platform_screenshot_saved(const char *path) { (void)path; }

/* xdg-open en un proceso aparte, sin esperarlo: el juego sigue. */
int platform_open_url(const char *url) {
    pid_t child = fork();
    if (child < 0) return 0;
    if (child == 0) {
        setsid();
        execlp("xdg-open", "xdg-open", url, (char *)NULL);
        _exit(127);
    }
    return 1;
}

int platform_vibrate(int ms) {
    (void)ms;
    return 0;
}
