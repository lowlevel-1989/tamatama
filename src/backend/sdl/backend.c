/* backend/sdl/backend.c: ventana, contexto OpenGL y eventos con SDL2. Es el
   único fichero que incluye
   SDL. El contexto es OpenGL 2.1 de escritorio, porque GLES1 se parece mucho a
   su función fija. Se crea en el thread principal, que es el mismo que ejecuta
   las funciones nativas. */
#include <SDL.h>
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
    window = SDL_CreateWindow("Game", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height,
                              SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
    if (!window) die_sdl("SDL_CreateWindow");
    context = SDL_GL_CreateContext(window);
    if (!context) die_sdl("SDL_GL_CreateContext");
    SDL_GL_SetSwapInterval(1);
}
void backend_process_events(void) {
    SDL_Event event;
    while (SDL_PollEvent(&event))
        if (event.type == SDL_QUIT) should_close = 1;
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
