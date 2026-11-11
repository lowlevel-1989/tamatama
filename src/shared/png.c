/* shared/png.c: decodificar un PNG con stb_image (src/third_party/stb), con
   solo su decodificador de PNG y desde memoria, y guardar uno con miniz (las
   capturas de pantalla). Se declara en services.h, para que también lo use
   src/port/game. */
#include "services.h"
#include "miniz.h"

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

int png_save_rgb(const char *path, const uint8_t *rgb, int width, int height) {
    size_t size;
    /* 6: la compresión por defecto de zlib. MZ_TRUE: dar la vuelta a las
       filas, que vienen de abajo arriba. */
    void *png = tdefl_write_image_to_png_file_in_memory_ex(rgb, width, height, 3, &size, 6, MZ_TRUE);
    if (!png) return 0;
    FILE *out = fopen(path, "wb");
    int ok = out && fwrite(png, 1, size, out) == size;
    if (out && fclose(out)) ok = 0;
    mz_free(png);
    return ok;
}
