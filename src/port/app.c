#include "common.h"
#include "backend.h"

#define METHOD(name) "Java_com_namcobandaigames_tamagotchisp_SampleGameNatives_" name

// El juego no debería mirar dentro de jclass.
#define FAKE_JCLASS 0x1000u
#define WIDTH 320
#define HEIGHT 480

extern int reload_counter;

int app_main(void) {
    arm_ptr env = arm_jni_env();
    backend_init(WIDTH, HEIGHT);

    // deberia actualizar cada que cambia el tamaño de la pantalla. Va antes de
    // init: backend_init ya ha creado la ventana, así que init ve 1.
    reload_counter = 1;

    uint32_t init_args[4] = {env, FAKE_JCLASS, WIDTH, HEIGHT};

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
        backend_begin_frame();
        call_method(METHOD("step"), no_args, 2);
        backend_swap_buffers();
    }

    /*
    call_method(METHOD("GameTerm"), no_args, 2);
    call_method(METHOD("stop"), no_args, 2);
    */
    backend_cleanup();
    return 0;
}
