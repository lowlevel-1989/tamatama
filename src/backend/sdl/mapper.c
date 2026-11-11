/* backend/sdl/mapper.c: el mapeador de botones, la opción "Configure
   controls" del menú de --menu. Pide las acciones que usa el juego (las
   cuatro direcciones, aceptar, 1, 2 y 3, y salir solo en el mando: en el
   teclado es Escape), en el mando o en el teclado: lo decide la primera
   entrada (sin mando, directamente el teclado).

   El mando. Uno que SDL reconoce no lo necesita: sus botones ya dan las
   acciones (backend.c, button_input). Para otro, las guarda como una línea
   de SDL_GAMECONTROLLERCONFIG en controller.map, junto al programa: el mismo
   formato que exporta PortMaster, y backend_init lo carga al arrancar. Cada
   acción se guarda como el botón estándar de SDL que le corresponde
   (izquierda dpleft, aceptar a, 1 y, 2 x, 3 b, salir back), así que un
   mando mapeado funciona igual que uno reconocido.

     abre el primer mando (SDL_Joystick: lo crudo, sin mapeo)
     por cada acción:
       espera una entrada nueva: un botón (b3), un hat (h0.1) o un eje (-a1)
       espera a que se suelte
     guarda la línea en controller.map (sustituye la de ese mando)
     y la aplica ya: backend_reload_controllers

   El teclado. Las teclas van a keyboard.map, junto al programa, una línea
   por acción con el nombre de SDL de la tecla:
     left=Left
     1=Z
   keymap_load lo lee al arrancar y keymap_input lo consulta antes que las
   teclas de siempre (backend.c, key_input).

   Esc o retroceso cancelan sin guardar y vuelven al menú: son volver, así
   que no se pueden asignar. */
#include <SDL.h>
#include <limits.h>
#include "common.h"
#include "platform.h"
#include "backend.h"

void backend_reload_controllers(void); /* backend.c */

#define AXIS_PRESSED 16000  /* de 32767: más allá, el eje cuenta como pulsado */
#define AXIS_RELEASED 8000

/* Las acciones del juego, en orden: el botón estándar de SDL que les toca,
   lo que se enseña, su nombre en keyboard.map y la acción. El botón a es
   aceptar y 3 a la vez (INPUT_ACCEPT3, backend.c): BUTTON 3 puede ser el
   mismo que ACCEPT, y entonces no ocupa x. */
static const struct {
    const char *sdl, *label, *name;
    int input;
} wanted[] = {
    {"dpup", "UP", "up", INPUT_UP},             {"dpdown", "DOWN", "down", INPUT_DOWN},
    {"dpleft", "LEFT", "left", INPUT_LEFT},     {"dpright", "RIGHT", "right", INPUT_RIGHT},
    {"a", "ACCEPT", "accept", INPUT_ACCEPT},    {"y", "BUTTON 1", "1", INPUT_1},
    {"b", "BUTTON 2", "2", INPUT_2},            {"x", "BUTTON 3", "3", INPUT_3},
    {"back", "EXIT", "exit", INPUT_BACK},
};
#define WANTED (int)(sizeof wanted / sizeof wanted[0])

/* Qué se está configurando: aún no se sabe, el mando o el teclado. */
enum { MODE_ANY, MODE_PAD, MODE_KEYS };

/* Una entrada cruda: "b3", "h0.1", "-a1" o una tecla ("k:Z", y su key). */
typedef struct {
    char text[40];
    SDL_Keycode key;
} Raw;

/* Las teclas de keyboard.map, por acción (SDLK_UNKNOWN: sin asignar). */
static SDL_Keycode keys[WANTED];

static void keymap_path(char *path, size_t size) {
    snprintf(path, size, "%s/keyboard.map", base_dir());
}

void keymap_load(void) {
    char path[PATH_MAX], line[128];
    keymap_path(path, sizeof path);
    FILE *in = fopen(path, "r");
    if (!in) return;
    int count = 0;
    while (fgets(line, sizeof line, in)) {
        line[strcspn(line, "\r\n")] = 0;
        char *value = strchr(line, '=');
        if (!value) continue;
        *value++ = 0;
        for (int i = 0; i < WANTED; i++) {
            if (strcmp(line, wanted[i].name)) continue;
            keys[i] = SDL_GetKeyFromName(value);
            if (keys[i] != SDLK_UNKNOWN) count++;
        }
    }
    fclose(in);
    log_msg("[input] %s: %d keys\n", path, count);
}

int keymap_input(SDL_Keycode key) {
    int found = INPUT_NONE;
    for (int i = 0; i < WANTED; i++)
        if (keys[i] != SDLK_UNKNOWN && keys[i] == key) {
            /* aceptar y 3 en la misma tecla */
            if ((found == INPUT_ACCEPT && wanted[i].input == INPUT_3) ||
                (found == INPUT_3 && wanted[i].input == INPUT_ACCEPT))
                return INPUT_ACCEPT3;
            found = wanted[i].input;
        }
    return found;
}

/* Guarda las teclas en keyboard.map y las aplica ya. */
static int keymap_save(const Raw *raws) {
    char path[PATH_MAX], temporary[PATH_MAX + 8];
    keymap_path(path, sizeof path);
    snprintf(temporary, sizeof temporary, "%s.tmp", path);
    FILE *out = fopen(temporary, "w");
    if (!out) return 0;
    for (int i = 0; i < WANTED; i++) fprintf(out, "%s=%s\n", wanted[i].name, raws[i].text + 2);
    if (fclose(out) || rename(temporary, path)) return 0;
    for (int i = 0; i < WANTED; i++) keys[i] = raws[i].key;
    log_msg("[input] %s: saved\n", path);
    return 1;
}

static void screen(const char *title, const char *line1, const char *line2) {
    /* Los mismos colores claros que el menú (shared/menu.c). */
    backend_begin_frame();
    backend_fill_rect(0, 0, 320, 480, 0xfffddc);
    int w = backend_text_width(title, 3);
    backend_draw_text((320 - w) / 2, 120, title, 3, 0xeb008b);
    if (line1) backend_draw_text((320 - backend_text_width(line1, 2)) / 2, 200, line1, 2, 0x000000);
    if (line2) backend_draw_text((320 - backend_text_width(line2, 1)) / 2, 260, line2, 1, 0x60c668);
    backend_draw_text((320 - backend_text_width("esc: back", 1)) / 2, 440, "esc: back", 1, 0x000000);
    backend_swap_buffers();
}

/* 1 si raw ya está asignada a otro botón. BUTTON 3 puede repetir ACCEPT. */
static int taken(const Raw *raws, int count, const char *text) {
    for (int i = 0; i < count; i++)
        if (!strcmp(raws[i].text, text) &&
            !(wanted[count].input == INPUT_3 && wanted[i].input == INPUT_ACCEPT)) return 1;
    return 0;
}

/* 1 si BUTTON 3 es el mismo botón que ACCEPT. */
static int shared_three(const Raw *raws, int index) {
    if (wanted[index].input != INPUT_3) return 0;
    for (int i = 0; i < WANTED; i++)
        if (wanted[i].input == INPUT_ACCEPT && !strcmp(raws[i].text, raws[index].text)) return 1;
    return 0;
}

/* Espera una entrada nueva, del mando o del teclado según *mode, y a que se
   suelte. Con MODE_ANY, la primera decide cuál. Devuelve 1 si la ha visto y
   -1 si se cancela. */
static int capture(int index, Raw *raws, Raw *out, int *mode) {
    static const char *const from[] = {"keyboard or controller", "controller", "keyboard"};
    out->text[0] = 0;
    for (;;) {
        char prompt[32], count[48];
        snprintf(prompt, sizeof prompt, "press %s", wanted[index].label);
        snprintf(count, sizeof count, "%d of %d, %s", index + 1, WANTED, from[*mode]);
        screen("CONTROLS", prompt, count);

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                SDL_PushEvent(&e);   /* para que el menú también cierre */
                return -1;
            }
            if (e.type == SDL_KEYDOWN &&
                (e.key.keysym.sym == SDLK_ESCAPE || e.key.keysym.sym == SDLK_BACKSPACE ||
                 e.key.keysym.sym == SDLK_AC_BACK))
                return -1;
            if (e.type == SDL_JOYDEVICEREMOVED && *mode != MODE_KEYS) return -1;

            /* El teclado: una tecla, y se suelta con su SDL_KEYUP. */
            if (*mode != MODE_PAD) {
                if (out->text[0] == 'k') {
                    if (e.type == SDL_KEYUP && e.key.keysym.sym == out->key) {
                        *mode = MODE_KEYS;
                        return 1;
                    }
                    continue;
                }
                if (!out->text[0] && e.type == SDL_KEYDOWN && !e.key.repeat) {
                    char text[40];
                    snprintf(text, sizeof text, "k:%s", SDL_GetKeyName(e.key.keysym.sym));
                    if (!taken(raws, index, text)) {
                        memcpy(out->text, text, sizeof text);
                        out->key = e.key.keysym.sym;
                    }
                    continue;
                }
            }
            if (*mode == MODE_KEYS) continue;

            /* El mando. */
            char text[16] = "";
            int released = 0;
            /* Lo que se pulsa: un botón, un hat o un eje pasado de la mitad.
               Mientras no se suelta, la entrada ya vista se ignora. */
            if (e.type == SDL_JOYBUTTONDOWN) snprintf(text, sizeof text, "b%d", e.jbutton.button);
            if (e.type == SDL_JOYHATMOTION && e.jhat.value != SDL_HAT_CENTERED)
                snprintf(text, sizeof text, "h%d.%d", e.jhat.hat, e.jhat.value);
            if (e.type == SDL_JOYAXISMOTION && abs(e.jaxis.value) > AXIS_PRESSED)
                snprintf(text, sizeof text, "%ca%d", e.jaxis.value < 0 ? '-' : '+', e.jaxis.axis);
            /* Y cuándo se suelta la que se está esperando. */
            if (out->text[0]) {
                if (e.type == SDL_JOYBUTTONUP && out->text[0] == 'b' && atoi(out->text + 1) == e.jbutton.button)
                    released = 1;
                if (e.type == SDL_JOYHATMOTION && out->text[0] == 'h' && e.jhat.value == SDL_HAT_CENTERED)
                    released = 1;
                if (e.type == SDL_JOYAXISMOTION && out->text[1] == 'a' &&
                    atoi(out->text + 2) == e.jaxis.axis && abs(e.jaxis.value) < AXIS_RELEASED)
                    released = 1;
                if (released) {
                    *mode = MODE_PAD;
                    return 1;
                }
                continue;
            }
            if (text[0] && !taken(raws, index, text)) {
                memcpy(out->text, text, sizeof text);
                out->key = SDLK_UNKNOWN;
            }
        }
        SDL_Delay(10);
    }
}

/* Guarda la línea en controller.map: las de los demás mandos se conservan, y
   la de este (mismo GUID, el principio de la línea) se sustituye. */
static int save(const char *guid, const char *line) {
    char path[PATH_MAX], temporary[PATH_MAX + 8];
    snprintf(path, sizeof path, "%s/controller.map", base_dir());
    snprintf(temporary, sizeof temporary, "%s.tmp", path);
    FILE *out = fopen(temporary, "w");
    if (!out) return 0;
    FILE *in = fopen(path, "r");
    if (in) {
        char old[1024];
        while (fgets(old, sizeof old, in))
            if (strncmp(old, guid, strlen(guid))) fputs(old, out);
        fclose(in);
    }
    fprintf(out, "%s\n", line);
    if (fclose(out) || rename(temporary, path)) return 0;
    log_msg("[input] %s: %s\n", path, line);
    return 1;
}

/* Devuelve 1 si guarda el mando, 2 si guarda el teclado y 0 si se cancela o
   no puede guardar. */
int backend_configure_controller(void) {
    /* Sin mando, solo el teclado. */
    SDL_Joystick *pad = SDL_NumJoysticks() > 0 ? SDL_JoystickOpen(0) : NULL;
    if (pad) SDL_JoystickEventState(SDL_ENABLE);
    int mode = pad ? MODE_ANY : MODE_KEYS;

    Raw raws[WANTED];
    for (int i = 0; i < WANTED; i++) {
        /* En el teclado, salir es siempre Escape (y Escape cancela aquí):
           EXIT solo se pide al mando. Como es la última, ya se sabe cuál. */
        if (wanted[i].input == INPUT_BACK && mode == MODE_KEYS) {
            snprintf(raws[i].text, sizeof raws[i].text, "k:");
            raws[i].key = SDLK_UNKNOWN;
            continue;
        }
        if (capture(i, raws, &raws[i], &mode) < 0) {
            if (pad) SDL_JoystickClose(pad);
            return 0;
        }
        log_msg("[input] %s = %s\n", wanted[i].label, raws[i].text);
    }

    if (mode == MODE_KEYS) {
        if (pad) SDL_JoystickClose(pad);
        if (!keymap_save(raws)) {
            screen("CONTROLS", "nothing saved", NULL);
            SDL_Delay(1500);
            return 0;
        }
        screen("CONTROLS", "saved", "keyboard.map");
        SDL_Delay(1500);
        return 2;
    }

    /* La línea: GUID,nombre,botón:entrada,...,platform:Linux */
    char guid[33], line[1024];
    SDL_JoystickGetGUIDString(SDL_JoystickGetGUID(pad), guid, sizeof guid);
    int used = snprintf(line, sizeof line, "%s,%s", guid, SDL_JoystickName(pad));
    for (int i = 0; i < WANTED; i++)
        if (!shared_three(raws, i))
            used += snprintf(line + used, sizeof line - (size_t)used, ",%s:%s", wanted[i].sdl, raws[i].text);
    snprintf(line + used, sizeof line - (size_t)used, ",platform:%s", SDL_GetPlatform());
    SDL_JoystickClose(pad);

    if (!save(guid, line)) {
        screen("CONTROLS", "nothing saved", NULL);
        SDL_Delay(1500);
        return 0;
    }
    /* Ya, sin reiniciar el programa: los mandos se vuelven a abrir con él. */
    backend_reload_controllers();
    screen("CONTROLS", "saved", "controller.map");
    SDL_Delay(1500);
    return 1;
}
