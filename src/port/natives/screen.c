/* port/natives/screen.c: la pantalla, su tamaño y sus capturas, desde Java. */
#include <limits.h>
#include <sys/stat.h>
#include <time.h>
#include "common.h"
#include "platform.h"
#include "backend.h"

/* La lleva app_main: 1 al crear la ventana y uno más cada vez que cambia su
   tamaño. */
int reload_counter = 0;

/* En Android cuenta las llamadas a onSurfaceChanged: la primera es al crearse
   la pantalla, y las siguientes, cuando cambia su tamaño. */
void native_OnGetReloadCounter(Regs *r) {
    r->r[0] = (uint32_t)reload_counter;
}

/* La captura de pantalla: el icono de la cámara del juego.
   CTcGameMainPart::processCapture la lleva por estados: al pulsarlo llama a
   OnStartCapture(1), cuenta unas 93 imágenes y luego llama a OnSelectSNS
   para compartirla. Aquí OnStartCapture pide guardar la siguiente imagen
   (app_main la guarda después de step, sin el puntero del mando) y
   OnSelectSNS dice que no se comparte. */
int capture_requested;

void native_OnStartCapture(Regs *r) {
    (void)r;
    capture_requested = 1;
}

void native_OnSelectSNS(Regs *r) { r->r[0] = 0; }

/* Guarda lo dibujado en {base_dir}/screenshots/, con la fecha y la hora. */
void capture_save(void) {
    char dir[PATH_MAX], path[PATH_MAX + 80], name[64];
    snprintf(dir, sizeof dir, "%s/screenshots", base_dir());
    mkdir(dir, 0755);
    time_t now = time(NULL);
    strftime(name, sizeof name, "%Y-%m-%d_%H-%M-%S", localtime(&now));
    snprintf(path, sizeof path, "%s/%s.png", dir, name);
    int width, height;
    uint8_t *rgb = backend_read_pixels(&width, &height);
    if (rgb && png_save_rgb(path, rgb, width, height)) {
        log_msg("[capture] saved %s\n", path);
        platform_screenshot_saved(path);
    }
    else log_msg("[capture] cannot save %s\n", path);
    free(rgb);
}
