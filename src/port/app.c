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

    uint32_t init_args[4] = {env, FAKE_JCLASS, WIDTH, HEIGHT};
    call_method(METHOD("init"), init_args, 4);

    // deberia actualizar cada que cambia el tamaño de la pantalla
    reload_counter = 1;

    /*
    uint32_t no_args[2] = {env, FAKE_JCLASS};

    while (!backend_should_close()) {
        backend_process_events();
        call_method(METHOD("step"), no_args, 2);
        backend_swap_buffers();
    }

    call_method(METHOD("GameTerm"), no_args, 2);
    call_method(METHOD("stop"), no_args, 2);
    */
    backend_cleanup();
    return 0;
}
