/* backend/sdl/backend.c: ventana, contexto OpenGL y eventos con SDL2. Es el
   único fichero que incluye
   SDL. El contexto es GLES 1.1 con make GLES=1 (Android, PortMaster) u
   OpenGL 2.1 de escritorio, cuya función fija se parece mucho a GLES1
   (glcompat.h). Lo que dibuja el backend usa solo lo que tienen los dos:
   arrays de vértices, sin glBegin ni glPushAttrib. Se crea en el thread
   principal, que es el mismo que ejecuta las funciones nativas. */
#include <SDL.h>
#include <limits.h>
#include "glcompat.h"
#include "common.h"
#include "platform.h"
#include "backend.h"
/* stb_easy_font trae un aviso en una inicialización suya */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-braces"
#include "stb_easy_font.h"
#pragma GCC diagnostic pop

static SDL_Window *window;
static SDL_GLContext context;
static int should_close;
static int resized;
/* 0: aún no se ha visto; >0: frames que faltan; -1: ya es redimensionable */
#define RESIZABLE_DELAY 30
static int frames_until_resizable;
static int init_w, init_h; /* el tamaño con que se abrió */
/* donde va el juego dentro de la ventana, en píxeles reales */
static int game_x, game_y, game_w, game_h;
/* la textura de las bandas y lo que mide cada repetición, en coordenadas del
   juego; 0 sin textura */
static int bands_texture, bands_w, bands_h;

static void die_sdl(const char *message) {
    log_msg("[sdl] %s: %s\n", message, SDL_GetError());
    die("cannot start SDL");
}
/* El rectángulo más grande con la proporción del tamaño inicial que cabe en
   w x h, centrado: lo que sobra queda como bandas negras. */
static void fit(int w, int h, int *x, int *y, int *fit_w, int *fit_h) {
    /* sobra ancho: bandas a los lados */
    if ((long)w * init_h > (long)h * init_w) {
        *fit_h = h;
        *fit_w = (int)((long)h * init_w / init_h);
    } else { /* sobra alto: bandas arriba y abajo */
        *fit_w = w;
        *fit_h = (int)((long)w * init_h / init_w);
    }
    *x = (w - *fit_w) / 2;
    *y = (h - *fit_h) / 2;
}

/* El juego en la ventana, sin deformarse. En los píxeles reales de la
   ventana: con el escalado de pantalla de Wayland pueden ser más que su
   tamaño. */
static void fill_window(void) {
    int pixels_w, pixels_h;
    SDL_GL_GetDrawableSize(window, &pixels_w, &pixels_h);
    fit(pixels_w, pixels_h, &game_x, &game_y, &game_w, &game_h);
    glViewport(game_x, game_y, game_w, game_h);
}

/* keyboard.map, las teclas del mapeador (mapper.c). */
void keymap_load(void);
int keymap_input(SDL_Keycode key);

/* Los mandos. SDL ya conoce muchos, y lee además SDL_GAMECONTROLLERCONFIG,
   que exporta PortMaster. controller.map, junto al programa, es lo que ha
   configurado el usuario (mapper.c): una línea de SDL_GAMECONTROLLERCONFIG
   por mando. Tiene que mandar sobre todo lo demás (muchas consolas traen los
   botones mal), pero SDL da más prioridad a SDL_GAMECONTROLLERCONFIG que a
   los mapeos que añade el programa. Así que sus líneas se añaden al final de
   esa variable antes de iniciar los mandos: misma prioridad, y la última de
   cada mando sustituye a las anteriores. Los mandos se abren al conectarse:
   SDL avisa también de los que ya estaban (SDL_CONTROLLERDEVICEADDED). */
static void user_mappings(void) {
    static char *original;  /* la variable tal como llegó */
    if (!original) {
        const char *value = getenv("SDL_GAMECONTROLLERCONFIG");
        original = strdup(value ? value : "");
    }
    char path[PATH_MAX];
    snprintf(path, sizeof path, "%s/controller.map", base_dir());
    char *user = read_file(path);
    if (!user) return;
    size_t size = strlen(original) + strlen(user) + 2;
    char *joined = malloc(size);
    snprintf(joined, size, "%s\n%s", original, user);
    setenv("SDL_GAMECONTROLLERCONFIG", joined, 1);
    log_msg("[input] %s: controller mappings\n", path);
    free(joined);
    free(user);
}

static void controllers_init(void) {
    user_mappings();
    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) < 0) log_msg("[input] no controllers: %s\n", SDL_GetError());
}

/* Tras guardar controller.map: los mandos otra vez, con el mapeo nuevo. */
void backend_reload_controllers(void) {
    SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
    controllers_init();
}

void backend_init(int width, int height) {
    init_w = width;
    init_h = height;
    if (window) {
        /* ya abierta por el menú de --menu: solo cambia de tamaño */
        SDL_SetWindowSize(window, width, height);
        fill_window();
        return;
    }
    if (SDL_Init(SDL_INIT_VIDEO) < 0) die_sdl("SDL_Init");
    controllers_init();
    keymap_load();
#ifdef ARENA_GLES
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
#else
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
#endif
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
    window = SDL_CreateWindow("Arena", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height,
                              SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
    if (!window) die_sdl("SDL_CreateWindow");
    context = SDL_GL_CreateContext(window);
    if (!context) die_sdl("SDL_GL_CreateContext");
    SDL_GL_SetSwapInterval(1);
    fill_window();
}

void backend_set_bands(int gl_name, int width, int height) {
    bands_texture = gl_name;
    bands_w = width;
    bands_h = height;
}

/* glIsEnabled para lo que no es un array (texturas, mezcla...).
   glGetBooleanv dice lo mismo, y el GLES 1 del emulador de Android tiene mal
   glIsEnabled: lo trata todo como un array y lee fuera de su tabla. */
static GLboolean is_enabled(GLenum cap) {
    GLboolean on = GL_FALSE;
    glGetBooleanv(cap, &on);
    return on;
}

/* Dibuja count vértices (x, y), con coordenadas de textura si uv no es NULL,
   y deja los arrays del juego como estaban: el .so no siempre vuelve a dar
   sus pointers antes de dibujar. */
typedef struct {
    GLboolean on;
    GLint size, type, stride;
    void *pointer;
} ArrayState;

static void save_array(GLenum array, GLenum size, GLenum type, GLenum stride, GLenum pointer, ArrayState *a) {
    a->on = glIsEnabled(array);
    glGetIntegerv(size, &a->size);
    glGetIntegerv(type, &a->type);
    glGetIntegerv(stride, &a->stride);
    glGetPointerv(pointer, &a->pointer);
}

static void draw_2d(GLenum mode, const float *xy, const float *uv, int count) {
    ArrayState vertex, color, texture;
    save_array(GL_VERTEX_ARRAY, GL_VERTEX_ARRAY_SIZE, GL_VERTEX_ARRAY_TYPE, GL_VERTEX_ARRAY_STRIDE,
               GL_VERTEX_ARRAY_POINTER, &vertex);
    save_array(GL_COLOR_ARRAY, GL_COLOR_ARRAY_SIZE, GL_COLOR_ARRAY_TYPE, GL_COLOR_ARRAY_STRIDE,
               GL_COLOR_ARRAY_POINTER, &color);
    save_array(GL_TEXTURE_COORD_ARRAY, GL_TEXTURE_COORD_ARRAY_SIZE, GL_TEXTURE_COORD_ARRAY_TYPE,
               GL_TEXTURE_COORD_ARRAY_STRIDE, GL_TEXTURE_COORD_ARRAY_POINTER, &texture);
    glEnableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
    if (uv) glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    else glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glVertexPointer(2, GL_FLOAT, 0, xy);
    if (uv) glTexCoordPointer(2, GL_FLOAT, 0, uv);
    glDrawArrays(mode, 0, count);
    glVertexPointer(vertex.size, (GLenum)vertex.type, vertex.stride, vertex.pointer);
    glColorPointer(color.size, (GLenum)color.type, color.stride, color.pointer);
    glTexCoordPointer(texture.size, (GLenum)texture.type, texture.stride, texture.pointer);
    if (!vertex.on) glDisableClientState(GL_VERTEX_ARRAY);
    if (color.on) glEnableClientState(GL_COLOR_ARRAY);
    if (texture.on) glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    else glDisableClientState(GL_TEXTURE_COORD_ARRAY);
}

/* La textura de las bandas, repetida por toda la ventana. El mosaico empieza
   en la esquina de arriba a la izquierda del juego, y cada repetición mide lo
   mismo que mediría dentro del juego. Deja la textura y su enlace como los
   tenía el .so. */
static void draw_bands(void) {
    int pixels_w, pixels_h;
    SDL_GL_GetDrawableSize(window, &pixels_w, &pixels_h);
    float scale = (float)game_w / 320.0f; /* píxeles por unidad del juego */
    float tile_w = bands_w * scale, tile_h = bands_h * scale;
    float left = (float)game_x;
    float top = (float)(pixels_h - game_y - game_h); /* GL cuenta desde abajo */
    GLboolean had_texture = is_enabled(GL_TEXTURE_2D);
    GLint bound = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &bound);
    glViewport(0, 0, pixels_w, pixels_h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gl_ortho(0.0f, (float)pixels_w, (float)pixels_h, 0.0f, -1.0f, 1.0f);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, (GLuint)bands_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glEnable(GL_BLEND); /* sobre el blanco, por si tiene transparencias */
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    const float xy[8] = {0, 0, (float)pixels_w, 0, (float)pixels_w, (float)pixels_h, 0, (float)pixels_h};
    float uv[8];
    for (int i = 0; i < 4; i++) {
        uv[i * 2] = (xy[i * 2] - left) / tile_w;
        uv[i * 2 + 1] = (xy[i * 2 + 1] - top) / tile_h;
    }
    draw_2d(GL_TRIANGLE_FAN, xy, uv, 4);
    glBindTexture(GL_TEXTURE_2D, (GLuint)bound);
    if (!had_texture) glDisable(GL_TEXTURE_2D);
    glViewport(game_x, game_y, game_w, game_h);
}

/* Las bandas que sobran alrededor del juego van en blanco, que pega con el
   juego (o en el color de backend_set_background), o con su textura si la
   hay. Después, lo que espera el .so al empezar
   cada imagen:
     fondo negro -> glClear, solo en la zona del juego
     proyección: glOrthof(0, 320, 480, 0, -1, 1), píxeles con el 0 arriba
     modelview: identidad
     mezcla: GL_ONE, GL_ONE_MINUS_SRC_ALPHA (color multiplicado por el alfa) */
static unsigned background = 0xffffff;
void backend_set_background(unsigned rgb) { background = rgb; }

void backend_begin_frame(void) {
    /* glClear borra la ventana entera */
    glClearColor((background >> 16 & 255) / 255.0f, (background >> 8 & 255) / 255.0f,
                 (background & 255) / 255.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    if (bands_texture > 0) draw_bands();
    glEnable(GL_SCISSOR_TEST);
    glScissor(game_x, game_y, game_w, game_h);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gl_ortho(0.0f, 320.0f, 480.0f, 0.0f, -1.0f, 1.0f);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
}
/* Las pulsaciones pendientes, para backend_input_next. */
#define MAX_INPUTS 16
static int inputs[MAX_INPUTS];
static int input_first, input_count;

static void input_push(int input) {
    if (input == INPUT_NONE || input_count == MAX_INPUTS) return;
    inputs[(input_first + input_count++) % MAX_INPUTS] = input;
}

int backend_input_next(void) {
    if (!input_count) return INPUT_NONE;
    int input = inputs[input_first];
    input_first = (input_first + 1) % MAX_INPUTS;
    input_count--;
    return input;
}

/* Qué acción es cada tecla: las flechas, Enter o espacio para aceptar, Z, X y C para 1, 2 y 3, y
   Escape o retroceso para volver. Las de keyboard.map, si las hay, van
   antes (mapper.c). */
static int key_input(SDL_Keycode key) {
    /* Las teclas asignadas en el mapeador van antes que las de siempre. */
    int mapped = keymap_input(key);
    if (mapped != INPUT_NONE) return mapped;
    switch (key) {
    case SDLK_LEFT: return INPUT_LEFT;
    case SDLK_RIGHT: return INPUT_RIGHT;
    case SDLK_UP: return INPUT_UP;
    case SDLK_DOWN: return INPUT_DOWN;
    case SDLK_RETURN:
    case SDLK_KP_ENTER:
    case SDLK_SPACE: return INPUT_ACCEPT;
    case SDLK_z: return INPUT_1;
    case SDLK_x: return INPUT_2;
    case SDLK_c: return INPUT_3;
    case SDLK_ESCAPE:
    case SDLK_BACKSPACE:
    case SDLK_AC_BACK: return INPUT_BACK; /* el botón Atrás de Android */
    default: return INPUT_NONE;
    }
}

/* Y cada botón del mando: la cruceta, A para aceptar, Y, X y B para 1, 2
   y 3, y Back (Select) para salir. El mapeador (mapper.c) guarda cada
   acción en este mismo botón, así que vale también para los mandos que SDL
   no conocía. */
static int button_input(Uint8 button) {
    switch (button) {
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return INPUT_LEFT;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return INPUT_RIGHT;
    case SDL_CONTROLLER_BUTTON_DPAD_UP: return INPUT_UP;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return INPUT_DOWN;
    /* Los botones A, B y C del aparato, de izquierda a derecha: Y, B y A en
       una consola (X también es C). A acepta además: INPUT_ACCEPT3. */
    case SDL_CONTROLLER_BUTTON_A: return INPUT_ACCEPT3;
    case SDL_CONTROLLER_BUTTON_Y: return INPUT_1;
    case SDL_CONTROLLER_BUTTON_B: return INPUT_2;
    case SDL_CONTROLLER_BUTTON_X: return INPUT_3;
    case SDL_CONTROLLER_BUTTON_BACK: return INPUT_BACK;
    case SDL_CONTROLLER_BUTTON_START: return INPUT_ACCEPT;
    default: return INPUT_NONE;
    }
}

/* Una pulsación que aún no ha visto backend_pointer, con su posición en la
   ventana: un toque corto puede bajar y subir entre dos imágenes. */
static int press_pending, press_x, press_y;

void backend_process_events(void) {
    SDL_Event event;
    /* Cerrar es el botón de la ventana. Escape es volver (key_input): qué
       hace lo decide quien lee la entrada, el menú o app_main. */
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) should_close = 1;
        if (event.type == SDL_KEYDOWN && !event.key.repeat) input_push(key_input(event.key.keysym.sym));
        if (event.type == SDL_CONTROLLERBUTTONDOWN) {
            input_push(button_input(event.cbutton.button));
        }
        /* Un mando que SDL no reconoce (sin mapeo): en crudo, cualquier botón
           acepta y el hat mueve. Basta para el menú y para configurarlo. */
        if (event.type == SDL_JOYDEVICEADDED && !SDL_IsGameController(event.jdevice.which)) {
            SDL_Joystick *raw = SDL_JoystickOpen(event.jdevice.which);
            if (raw) log_msg("[input] controller without mapping: %s\n", SDL_JoystickName(raw));
        }
        if (event.type == SDL_JOYBUTTONDOWN && !SDL_GameControllerFromInstanceID(event.jbutton.which))
            input_push(INPUT_ACCEPT);
        if (event.type == SDL_JOYHATMOTION && !SDL_GameControllerFromInstanceID(event.jhat.which)) {
            Uint8 hat = event.jhat.value;
            input_push(hat & SDL_HAT_UP ? INPUT_UP : hat & SDL_HAT_DOWN ? INPUT_DOWN
                       : hat & SDL_HAT_LEFT ? INPUT_LEFT : hat & SDL_HAT_RIGHT ? INPUT_RIGHT : INPUT_NONE);
        }
        if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
            press_pending = 1;
            press_x = event.button.x;
            press_y = event.button.y;
        }
        if (event.type == SDL_CONTROLLERDEVICEADDED) {
            SDL_GameController *pad = SDL_GameControllerOpen(event.cdevice.which);
            if (pad) log_msg("[input] controller: %s\n", SDL_GameControllerName(pad));
        }
        if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_EXPOSED &&
            frames_until_resizable == 0)
            frames_until_resizable = RESIZABLE_DELAY;
        /* otro tamaño: la imagen se ajusta a la ventana nueva */
        if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
            fill_window();
            resized = 1;
        }
    }
}
int backend_resized(void) {
    int was = resized;
    resized = 0;
    return was;
}
void backend_swap_buffers(void) {
    SDL_GL_SwapWindow(window);
    /* Redimensionable solo cuando ya se ve: los gestores en mosaico, como
       sway, dejan flotando y con su tamaño original las ventanas que aparecen
       con tamaño fijo, y siguen así aunque luego se puedan redimensionar.
       Justo tras el primer frame es pronto: a veces el gestor aún no ha
       decidido. Así que se espera a que la ventana esté en pantalla (exposed)
       y RESIZABLE_DELAY frames más. */
    if (frames_until_resizable > 0 && --frames_until_resizable == 0) {
        SDL_SetWindowResizable(window, SDL_TRUE);
        frames_until_resizable = -1;
    }
}
int backend_should_close(void) { return should_close; }
int backend_pointer(int *x, int *y) {
    int window_x, window_y, window_w, window_h;
    int down = (SDL_GetMouseState(&window_x, &window_y) & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
    /* ya suelto, pero pulsado desde la última vez: cuenta como pulsado, en
       su sitio, y la vez siguiente se suelta */
    if (press_pending && !down) {
        down = 1;
        window_x = press_x;
        window_y = press_y;
    }
    press_pending = 0;
    /* de la ventana actual al tamaño con que se abrió, sin las bandas */
    SDL_GetWindowSize(window, &window_w, &window_h);
    int left, top, fit_w, fit_h;
    fit(window_w, window_h, &left, &top, &fit_w, &fit_h);
    *x = fit_w > 0 ? (window_x - left) * init_w / fit_w : window_x;
    *y = fit_h > 0 ? (window_y - top) * init_h / fit_h : window_y;
    /* en una banda: el borde más cercano del juego */
    if (*x < 0) *x = 0;
    if (*x >= init_w) *x = init_w - 1;
    if (*y < 0) *y = 0;
    if (*y >= init_h) *y = init_h - 1;
    return down;
}
void backend_cleanup(void) {
    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

/* Con el estado que espera el .so: filtro lineal, sin repetir en los bordes y
   GL_REPLACE. La textura se queda enlazada. */
int backend_texture_create(const unsigned char *rgba, int width, int height) {
    GLuint name = 0;
    glGenTextures(1, &name);
    glBindTexture(GL_TEXTURE_2D, name);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    return (int)name;
}

void backend_texture_delete(int gl_name) {
    GLuint name = (GLuint)gl_name;
    glDeleteTextures(1, &name);
}

/* ---------- Dibujar para el menú de --menu ---------- */

static void set_color(unsigned rgb) {
    glColor4f((rgb >> 16 & 0xff) / 255.0f, (rgb >> 8 & 0xff) / 255.0f, (rgb & 0xff) / 255.0f, 1.0f);
}

void backend_fill_rect(int x, int y, int width, int height, unsigned rgb) {
    glDisable(GL_TEXTURE_2D);
    set_color(rgb);
    const float xy[8] = {(float)x, (float)y, (float)(x + width), (float)y, (float)(x + width), (float)(y + height),
                         (float)x, (float)(y + height)};
    draw_2d(GL_TRIANGLE_FAN, xy, NULL, 4);
}

/* stb_easy_font (third_party/stb) convierte el texto en cuadrados: cada
   vértice son x, y, z en float y 4 bytes de color, que aquí no se usan.
   GLES no tiene GL_QUADS: cada cuadrado son dos triángulos. */
#define TEXT_QUADS 1000
void backend_draw_text(int x, int y, const char *text, int scale, unsigned rgb) {
    static float quads[TEXT_QUADS * 4 * 4], triangles[TEXT_QUADS * 6 * 2];
    static const int corner[6] = {0, 1, 2, 0, 2, 3};
    unsigned char unused[4] = {255, 255, 255, 255};
    int count = stb_easy_font_print(0, 0, (char *)text, unused, quads, sizeof quads);
    for (int q = 0; q < count; q++)
        for (int k = 0; k < 6; k++) {
            const float *v = &quads[(q * 4 + corner[k]) * 4];
            triangles[(q * 6 + k) * 2] = (float)x + v[0] * (float)scale;
            triangles[(q * 6 + k) * 2 + 1] = (float)y + v[1] * (float)scale;
        }
    glDisable(GL_TEXTURE_2D);
    set_color(rgb);
    draw_2d(GL_TRIANGLES, triangles, NULL, count * 6);
}

/* Lo que cambia el dibujo encima del juego y luego se deja como estaba (GLES
   no tiene glPushAttrib): los arrays los guarda draw_2d. */
static struct {
    GLboolean texture, cull, depth, blend;
    GLfloat color[4];
    GLint blend_src, blend_dst, matrix_mode;
} saved;

void backend_overlay_begin(void) {
    saved.texture = is_enabled(GL_TEXTURE_2D);
    saved.cull = is_enabled(GL_CULL_FACE);
    saved.depth = is_enabled(GL_DEPTH_TEST);
    saved.blend = is_enabled(GL_BLEND);
    glGetFloatv(GL_CURRENT_COLOR, saved.color);
    glGetIntegerv(GL_BLEND_SRC, &saved.blend_src);
    glGetIntegerv(GL_BLEND_DST, &saved.blend_dst);
    glGetIntegerv(GL_MATRIX_MODE, &saved.matrix_mode);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gl_ortho(0.0f, 320.0f, 480.0f, 0.0f, -1.0f, 1.0f);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
}

void backend_overlay_end(void) {
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode((GLenum)saved.matrix_mode);
    if (saved.texture) glEnable(GL_TEXTURE_2D);
    if (saved.cull) glEnable(GL_CULL_FACE);
    if (saved.depth) glEnable(GL_DEPTH_TEST);
    if (saved.blend) glEnable(GL_BLEND);
    else glDisable(GL_BLEND);
    glBlendFunc((GLenum)saved.blend_src, (GLenum)saved.blend_dst);
    glColor4f(saved.color[0], saved.color[1], saved.color[2], saved.color[3]);
}

/* El vibrador que no es de un mando: en Android, el del teléfono (SDL lo
   llama VIBRATOR_SERVICE). Se abre la primera vez; NULL si no hay. */
static SDL_Haptic *phone_haptic(void) {
    static SDL_Haptic *haptic;
    static int tried;
    if (tried) return haptic;
    tried = 1;
    if (SDL_InitSubSystem(SDL_INIT_HAPTIC) < 0) return NULL;
    for (int i = 0; i < SDL_NumHaptics() && !haptic; i++) {
        SDL_Haptic *candidate = SDL_HapticOpen(i);
        if (!candidate) continue;
        if (SDL_HapticRumbleInit(candidate) == 0) {
            haptic = candidate;
            log_msg("[device] vibrator: %s\n", SDL_HapticName(i));
        } else {
            SDL_HapticClose(candidate);
        }
    }
    return haptic;
}

/* Vibran los mandos que puedan; si ninguno, el teléfono: el de la
   plataforma (platform_vibrate) o, si no, el de SDL.
   Cada mando abierto, a toda fuerza en los dos motores. SDL no guarda la
   lista de los abiertos: se recorren los conectados y se busca el abierto
   por su id. */
void backend_vibrate(int ms) {
    int done = 0;
    for (int i = 0; i < SDL_NumJoysticks(); i++) {
        if (!SDL_IsGameController(i)) continue;
        SDL_GameController *pad = SDL_GameControllerFromInstanceID(SDL_JoystickGetDeviceInstanceID(i));
        if (pad && SDL_GameControllerRumble(pad, 0xffff, 0xffff, (Uint32)ms) == 0) done = 1;
    }
    if (done || platform_vibrate(ms)) return;
    SDL_Haptic *haptic = phone_haptic();
    if (haptic && SDL_HapticRumblePlay(haptic, 1.0f, (Uint32)ms) < 0)
        log_msg("[device] cannot vibrate: %s\n", SDL_GetError());
}

unsigned char *backend_read_pixels(int *width, int *height) {
    SDL_GL_GetDrawableSize(window, width, height);
    size_t pixels = (size_t)*width * (size_t)*height;
    unsigned char *rgb = malloc(pixels * 4);
    if (!rgb) return NULL;
    /* GLES solo garantiza GL_RGBA: se lee así y se quita el alfa */
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, *width, *height, GL_RGBA, GL_UNSIGNED_BYTE, rgb);
    for (size_t i = 0; i < pixels; i++) memmove(rgb + i * 3, rgb + i * 4, 3);
    return rgb;
}

void backend_fill_triangle(float x0, float y0, float x1, float y1, float x2, float y2, unsigned rgb) {
    glDisable(GL_TEXTURE_2D);
    set_color(rgb);
    const float xy[6] = {x0, y0, x1, y1, x2, y2};
    draw_2d(GL_TRIANGLES, xy, NULL, 3);
}

int backend_text_width(const char *text, int scale) {
    return stb_easy_font_width((char *)text) * scale;
}
