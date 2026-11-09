/* backend/sdl/backend.c: ventana, contexto OpenGL y eventos con SDL2. Es el
   único fichero que incluye
   SDL. El contexto es OpenGL 2.1 de escritorio, porque GLES1 se parece mucho a
   su función fija. Se crea en el thread principal, que es el mismo que ejecuta
   las funciones nativas. */
#include <SDL.h>
#define GL_GLEXT_PROTOTYPES 1
#include <GL/gl.h>
#include <GL/glext.h>
#include "common.h"
#include "backend.h"

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

void backend_init(int width, int height) {
    init_w = width;
    init_h = height;
    if (SDL_Init(SDL_INIT_VIDEO) < 0) die_sdl("SDL_Init");
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
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
    GLboolean had_texture = glIsEnabled(GL_TEXTURE_2D);
    GLint bound = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &bound);
    glViewport(0, 0, pixels_w, pixels_h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, pixels_w, pixels_h, 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, (GLuint)bands_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glEnable(GL_BLEND); /* sobre el blanco, por si tiene transparencias */
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    const float corners[4][2] = {{0, 0}, {(float)pixels_w, 0},
                                 {(float)pixels_w, (float)pixels_h}, {0, (float)pixels_h}};
    glBegin(GL_QUADS);
    for (int i = 0; i < 4; i++) {
        glTexCoord2f((corners[i][0] - left) / tile_w, (corners[i][1] - top) / tile_h);
        glVertex2f(corners[i][0], corners[i][1]);
    }
    glEnd();
    glBindTexture(GL_TEXTURE_2D, (GLuint)bound);
    if (!had_texture) glDisable(GL_TEXTURE_2D);
    glViewport(game_x, game_y, game_w, game_h);
}

/* Las bandas que sobran alrededor del juego van en blanco, que pega con el
   juego, o con su textura si la hay. Después, lo que espera el .so al empezar
   cada imagen:
     fondo negro -> glClear, solo en la zona del juego
     proyección: glOrthof(0, 320, 480, 0, -1, 1), píxeles con el 0 arriba
     modelview: identidad
     mezcla: GL_ONE, GL_ONE_MINUS_SRC_ALPHA (color multiplicado por el alfa) */
void backend_begin_frame(void) {
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f); /* glClear borra la ventana entera */
    glClear(GL_COLOR_BUFFER_BIT);
    if (bands_texture > 0) draw_bands();
    glEnable(GL_SCISSOR_TEST);
    glScissor(game_x, game_y, game_w, game_h);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, 320.0, 480.0, 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
}
void backend_process_events(void) {
    SDL_Event event;
    /* Escape cierra igual que el botón de la ventana: en Wayland, según el
       gestor de ventanas, la ventana puede no tener marco ni botón. */
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT ||
            (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE))
            should_close = 1;
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
