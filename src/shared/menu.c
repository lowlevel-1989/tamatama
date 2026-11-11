/* shared/menu.c: el menú de entrada de --menu. Antes de cargar nada, abre la
   ventana y deja elegir:
     cada versión que se puede jugar (apk_choices: los .apk de apk/ y las ya
     extraídas)
     Transfer Save          pasar la partida a otro dispositivo
                            (src/transfer)
     Configure controls     el mapeador (backend_configure_controller)
     Exit
   La pantalla y cómo se elige son las de shared/screen.c: con más versiones
   de las que caben, esa parte se desliza. Para salir está Exit. Lo elegido
   vuelve como si se hubiera pasado en la línea de órdenes, así que después
   todo sigue igual que sin --menu. */
#include "common.h"
#include "platform.h"
#include "backend.h"
#include "screen.h"
#include "transfer.h"
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>

#define MAX_CHOICES 16
#define WIDTH 320
#define HEIGHT 480

/* Las opciones fijas, debajo de las versiones. */
#define FIXED 3
static const char *const FIXED_ITEMS[FIXED] = {"Transfer Save", "Configure controls", "Exit"};

/* Lo elegido tarda en arrancar (y la primera vez se extrae el .apk): el
   aviso queda en pantalla hasta que el juego dibuja (app.c). */
static void loading(const char *name) {
    backend_begin_frame();
    backend_fill_rect(0, 0, WIDTH, HEIGHT, CREAM);
    screen_centered(180, "ARENA", 5, PINK);
    screen_centered(250, "loading...", 2, GREEN);
    screen_centered(290, name, 1, BLACK);
    backend_swap_buffers();
}

const char *menu_choose_game(void) {
    static ApkChoice choices[MAX_CHOICES];
    int count = apk_choices(choices, MAX_CHOICES);
    for (int i = 0; i < count; i++) log_msg("[menu] %d: %s\n", i + 1, choices[i].name);

    static Screen s;
    screen_reset(&s, "choose a game");
    s.logo = "ARENA";
    s.logo_scale = 5;
    s.logo_y = 50;
    s.title_y = 100;
    s.list_top = 150;
    s.can_back = 0; /* para salir está Exit */
    for (int i = 0; i < count; i++) screen_item(&s, choices[i].name);
    for (int i = 0; i < FIXED; i++) screen_item(&s, FIXED_ITEMS[i]);
    s.fixed = FIXED;
    if (!count) {
        log_msg("[menu] no supported game found. %s\n", apk_hint());
        s.notice = "no supported .apk found: copy it into apk/";
    }

    backend_init(WIDTH, HEIGHT);
    backend_set_background(CREAM); /* las bandas, como el fondo del menú */
    for (;;) {
        int chosen = screen_run(&s, NULL, NULL);
        if (chosen >= 0 && chosen < count) {
            log_msg("[menu] %s: %s\n", choices[chosen].name, choices[chosen].game);
            loading(choices[chosen].name);
            backend_set_background(0xffffff);
            return choices[chosen].game;
        }
        if (chosen == count) {
            transfer_screen();
            backend_set_background(CREAM);
            s.notice = NULL;
        } else if (chosen == count + 1) {
            int saved = backend_configure_controller();
            s.notice = saved == 1   ? "controller saved"
                       : saved == 2 ? "keyboard saved"
                       : saved < 0  ? "cannot configure controls"
                                    : "controls not changed";
        } else {
            backend_cleanup();
            exit(0);
        }
    }
}

/* El .so no se puede cargar dos veces en el mismo proceso, así que volver al
   menú es empezar de nuevo: el mismo programa (/proc/self/exe), con las
   mismas opciones. Antes se cierran la ventana y todo lo abierto (los pipes y
   la arena de loader-arm): execv solo conserva los descriptores. */
void menu_restart(char **argv) {
    if (backend_should_close()) return;
    char self[PATH_MAX];
    ssize_t n = readlink("/proc/self/exe", self, sizeof self - 1);
    if (n <= 0) return;
    self[n] = 0;
    log_msg("[menu] back to the menu\n");
    backend_cleanup();
    fflush(NULL);
    for (int fd = 3; fd < 1024; fd++) close(fd);
    execv(self, argv);
}
