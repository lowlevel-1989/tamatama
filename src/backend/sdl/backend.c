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

static void die_sdl(const char *message) {
    log_msg("[sdl] %s: %s\n", message, SDL_GetError());
    die("cannot start SDL");
}
void backend_init(int width, int height) {
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
    /* Toda la ventana, en sus píxeles reales: con el escalado de pantalla de
       Wayland pueden ser más que su tamaño. */
    int pixels_w, pixels_h;
    SDL_GL_GetDrawableSize(window, &pixels_w, &pixels_h);
    glViewport(0, 0, pixels_w, pixels_h);
}

/* Lo que espera el .so al empezar cada imagen:
     fondo negro -> glClear
     proyección: glOrthof(0, 320, 480, 0, -1, 1), píxeles con el 0 arriba
     modelview: identidad
     mezcla: GL_ONE, GL_ONE_MINUS_SRC_ALPHA (color multiplicado por el alfa) */
void backend_begin_frame(void) {
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
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
    while (SDL_PollEvent(&event))
        if (event.type == SDL_QUIT ||
            (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE))
            should_close = 1;
}
void backend_swap_buffers(void) { SDL_GL_SwapWindow(window); }
int backend_should_close(void) { return should_close; }
int backend_pointer(int *x, int *y) {
    return (SDL_GetMouseState(x, y) & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
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
