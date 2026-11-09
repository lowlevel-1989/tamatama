#include "common.h"
#include "backend.h"
#include "methods.h"
#include "touch.h"
#include "texture.h"

// La pantalla del juego es de 320x480, pero la ventana se abre a SCALE veces
// ese tamaño. init recibe el tamaño real, como en Android, y el .so escala
// sus coordenadas; el backend dibuja siempre en 320x480 y GL lo amplía.
#define WIDTH 320
#define HEIGHT 480
#define SCALE 1

extern int reload_counter;

int app_main(void) {
    arm_ptr env = arm_jni_env();
    backend_init(WIDTH * SCALE, HEIGHT * SCALE);

    // Las bandas que sobran si la ventana no tiene la proporción del juego:
    // uno de sus fondos, repetido. La imagen está a la escala alta del juego
    // (640x960), así que cada repetición mide la mitad en coordenadas del
    // juego.
    int bands_w, bands_h, bands = texture_upload("ima_bg_00001.png", &bands_w, &bands_h);
    if (bands > 0) backend_set_bands(bands, bands_w / 2, bands_h / 2);

    // Cuenta los cambios de tamaño de la pantalla, como onSurfaceChanged en
    // Android. Va antes de init: backend_init ya ha creado la ventana, así que
    // init ve 1. En el loop, cada cambio de tamaño suma uno.
    reload_counter = 1;

    uint32_t init_args[4] = {env, FAKE_JCLASS, WIDTH * SCALE, HEIGHT * SCALE};

    // Ejecuta el gameloop original
    call_method(METHOD("init"), init_args, 4);

    // El loop del juego, run() (doc/code/run.c), no corre dentro de init:
    // corre en un thread que solo avanza cuando el principal le cede el turno.
    //   init -> OnThreadCreate -> thread creado, sin correr
    //   vuelve init -> primer turno
    //     -> run() hasta su primer usleep -> devuelve el turno
    //   usleep(200 ms) del lado ARM -> el principal duerme y cede el turno
    //     -> termina el sleep del thread -> otra vuelta de run()
    //     -> usleep -> devuelve el turno
    //     -> ... hasta que pasan los 200 ms -> app_main sigue
    // Cada vuelta de run() dura unos 62 ms (controlSleep(62)): unas 16 por
    // segundo, así que 200 ms dan al menos tres.
    // uint32_t wait[1] = {30000000};
    // call_method("usleep", wait, 1);

    uint32_t no_args[2] = {env, FAKE_JCLASS};

    while (!backend_should_close()) {
        backend_process_events();
        if (backend_resized()) reload_counter++;
        touch_update(env); /* el ratón como un dedo */
        backend_begin_frame();
        call_method(METHOD("step"), no_args, 2);
        backend_swap_buffers();
    }

    call_method(METHOD("GameTerm"), no_args, 2);
    call_method(METHOD("stop"), no_args, 2);

    backend_cleanup();
    return 0;
}
