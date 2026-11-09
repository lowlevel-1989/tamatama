/* backend/dummy/backend.c: sin ventana y sin dibujo. Sirve para probar la
   lógica del juego, y es lo que lleva loader-arm cuando el backend de loader
   necesita libraries de x86_64. Las funciones GL son las de
   backend/gl_defaults.c. Termina tras BACKEND_FRAMES imágenes, 600 si no se
   indica. */
#include "common.h"
#include "backend.h"

static int frames, max_frames = 600;

void backend_init(int width, int height) {
    (void)width;
    (void)height;
    const char *text = getenv("BACKEND_FRAMES");
    if (text) max_frames = (int)parse_number(text);
}
void backend_begin_frame(void) {}
void backend_process_events(void) {}
void backend_swap_buffers(void) { frames++; }
int backend_should_close(void) { return frames >= max_frames; }
int backend_pointer(int *x, int *y) {
    *x = *y = 0;
    return 0;
}
void backend_cleanup(void) {}
/* Sin GL: la textura se carga y se decodifica, pero no se sube a ningún sitio.
   0 es la textura vacía. */
int backend_texture_create(const unsigned char *rgba, int width, int height) {
    (void)rgba;
    (void)width;
    (void)height;
    return 0;
}
/* Sin GL no hay nada que borrar. */
void backend_texture_delete(int gl_name) { (void)gl_name; }
/* Sin audio: los sonidos no se cargan y nunca están sonando. */
int backend_sound_load(const void *data, size_t size, int music) {
    (void)data;
    (void)size;
    (void)music;
    return 0;
}
void backend_sound_play(int handle, int loop) {
    (void)handle;
    (void)loop;
}
void backend_sound_stop(int handle) { (void)handle; }
int backend_sound_playing(int handle) {
    (void)handle;
    return 0;
}
void backend_sound_volume(int music, float volume) {
    (void)music;
    (void)volume;
}
