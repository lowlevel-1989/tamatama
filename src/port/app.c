#include "common.h"
#include "backend.h"
#include "methods.h"
#include "touch.h"
#include "controls.h"
#include "texture.h"

// La pantalla del juego es de 320x480, pero la ventana se abre a SCALE veces
// ese tamaño. init recibe el tamaño real, como en Android, y el .so escala
// sus coordenadas; el backend dibuja siempre en 320x480 y GL lo amplía.
#define WIDTH 320
#define HEIGHT 480
#define SCALE 1

extern int reload_counter;
extern int capture_requested; /* natives/screen.c */

#define EXIT_CONFIRM_NS 2000000000ull /* 2 s para pulsar salir otra vez */

/* El aviso de salir: una franja con el texto, abajo, encima del juego y por
   encima del logo de Tamagotchi del pie de L.i.f.e. y Angel. */
static void exit_notice(void) {
    const char *text = "press again to exit";
    backend_overlay_begin();
    backend_fill_rect(0, 410, 320, 28, 0xeb008b);
    backend_draw_text((320 - backend_text_width(text, 2)) / 2, 417, text, 2, 0xffffff);
    backend_overlay_end();
}
void capture_save(void);

/* Un toque espera a que el juego acabe una animación (touch_waiting): el
   aviso, como el de salir. */
static void waiting_notice(const char *text) {
    backend_overlay_begin();
    backend_fill_rect(0, 410, 320, 28, 0xeb008b);
    backend_draw_text((320 - backend_text_width(text, 2)) / 2, 417, text, 2, 0xffffff);
    backend_overlay_end();
}

/* Mientras el juego carga no dibuja nada: unos segundos en negro que
   parecen un cuelgue. Hasta su primera imagen, un aviso con los colores del
   menú (shared/menu.c). */
static void loading_screen(void) {
    const char *text = "loading...";
    backend_overlay_begin();
    backend_fill_rect(0, 0, 320, 480, 0xfffddc);
    backend_draw_text((320 - backend_text_width("ARENA", 5)) / 2, 180, "ARENA", 5, 0xeb008b);
    backend_draw_text((320 - backend_text_width(text, 2)) / 2, 250, text, 2, 0x60c668);
    backend_overlay_end();
}

/* Al cambiar de pantalla, el juego funde a blanco y carga la siguiente en
   un solo step, que puede tardar segundos: la pantalla se queda en blanco
   y parece colgada. Las cargas llaman a app_loading_tick: si el step lleva
   LOADING_AFTER_NS sin dibujar nada de su imagen, se enseña una imagen en
   blanco con el aviso, como mucho cada LOADING_EVERY_NS. Todo corre en el
   thread de GL (los threads del juego van por turnos), así que se puede
   dibujar desde aquí. */
#define LOADING_AFTER_NS 300000000ull
#define LOADING_EVERY_NS 100000000ull
static uint64_t step_start, loading_shown;
static int step_draws;

void app_loading_tick(void) {
    if (!step_start || !backend_game_drew()) return; /* antes de la primera
                                                        imagen: loading_screen */
    uint64_t now = clock_ns();
    if (now - step_start < LOADING_AFTER_NS || now - loading_shown < LOADING_EVERY_NS) return;
    if (backend_draw_count() != step_draws) return; /* ya ha dibujado parte */
    loading_shown = now;
    backend_overlay_begin();
    backend_fill_rect(0, 0, 320, 480, 0xffffff);
    backend_overlay_end();
    waiting_notice("loading...");
    backend_swap_buffers();
    backend_begin_frame();
}

int app_main(void) {
    arm_ptr env = arm_jni_env();
    backend_init(WIDTH * SCALE, HEIGHT * SCALE);

    // Las bandas que sobran si la ventana no tiene la proporción del juego:
    // uno de sus fondos, repetido. La imagen está a la escala alta del juego
    // (640x960), así que cada repetición mide la mitad en coordenadas del
    // juego. Classic 2 solo trae la versión _N; sin ninguna, las bandas
    // quedan en blanco.
    int bands_w, bands_h, bands = texture_upload("ima_bg_00001.png", &bands_w, &bands_h);
    if (bands <= 0) bands = texture_upload("ima_bg_00001_N.png", &bands_w, &bands_h);
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

    int running = 1;
    uint64_t exit_asked = 0; /* cuándo se pulsó salir la primera vez */
    while (running && !backend_should_close()) {
        backend_process_events();
        // Salir (Escape, o Back en el mando) cierra el juego si se pulsa dos
        // veces seguidas: la primera solo lo avisa. El resto de acciones
        // mueven el puntero y tocan los botones del juego (controls.c).
        for (int input; (input = backend_input_next()) != INPUT_NONE;) {
            if (input != INPUT_BACK) controls_input(input);
            else if (exit_asked && clock_ns() - exit_asked < EXIT_CONFIRM_NS) running = 0;
            else exit_asked = clock_ns();
        }
        if (backend_resized()) reload_counter++;
        touch_update(env); /* el ratón como un touch */
        backend_begin_frame();
        step_start = clock_ns();
        step_draws = backend_draw_count();
        call_method(METHOD("step"), no_args, 2);
        step_start = 0;
        if (capture_requested) { /* la cámara del juego, antes del puntero */
            capture_requested = 0;
            capture_save();
        }
        if (!backend_game_drew()) loading_screen();
        controls_frame(); /* el puntero, encima del juego */
        if (exit_asked && clock_ns() - exit_asked < EXIT_CONFIRM_NS) exit_notice();
        else if (touch_waiting()) waiting_notice(touch_waiting());
        backend_swap_buffers();
    }

    call_method(METHOD("GameTerm"), no_args, 2);
    call_method(METHOD("stop"), no_args, 2);

    backend_cleanup();
    return 0;
}
