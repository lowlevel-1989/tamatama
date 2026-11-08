/* backend/dummy/backend.c: sin ventana y sin dibujo. Sirve para probar la lógica del juego, y es lo
   que lleva loader-arm cuando el backend de loader necesita libraries de x86_64. Las funciones GL
   son las de backend/gl_defaults.c. Termina tras BACKEND_FRAMES imágenes, 600 si no se indica. */
#include "common.h"
#include "backend.h"

static int frames, max_frames = 600;

void backend_init(int width, int height) {
    (void)width;
    (void)height;
    const char *text = getenv("BACKEND_FRAMES");
    if (text) max_frames = (int)parse_number(text);
}
void backend_process_events(void) {}
void backend_swap_buffers(void) { frames++; }
int backend_should_close(void) { return frames >= max_frames; }
int backend_pointer(int *x, int *y) {
    *x = *y = 0;
    return 0;
}
void backend_cleanup(void) {}
