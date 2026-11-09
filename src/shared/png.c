/* shared/png.c: decodificar un PNG con stb_image (src/third_party/stb), con
   solo su decodificador de PNG y desde memoria. Se declara en services.h, para
   que también lo use src/port/game. */
#include "services.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STBI_NO_LINEAR
#include "stb_image.h"

uint8_t *png_decode(const uint8_t *data, size_t size, int *width, int *height) {
    int channels;
    /* 4: siempre RGBA, sea cual sea el tipo del PNG. stb_image reserva con
       malloc, así que se libera con free. */
    uint8_t *rgba = stbi_load_from_memory(data, (int)size, width, height, &channels, 4);
    if (!rgba) log_msg("[png] cannot decode it: %s\n", stbi_failure_reason());
    return rgba;
}
