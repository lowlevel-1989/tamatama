/* port/game/texture.c: las texturas del juego, tal como las espera el .so.
   El .so pide un nombre como "title_bg_00000.png" y espera que se cargue así:
     name -> decodifica (png_decode) -> el backend lo sube a GL
       -> un id desde 1 (el hueco + 1), con su nombre en GL, ancho y alto
   Con un id no válido, las consultas devuelven -1. Al quitar una textura, su
   hueco queda libre y la siguiente que se cargue lo usa. En el .apk vienen
   como .tgd, que son PNG con otro nombre: al extraerlo ya se guardan como .png
   (asset_table.h), así que aquí basta con el nombre que pide el .so. */
#include "services.h"
#include "backend.h"
#include "texture.h"

#define MAX_TEXTURES 256

static struct {
    int used;
    int gl_name, width, height;
} textures[MAX_TEXTURES];
/* Cuántos huecos se han usado alguna vez: los de después están sin estrenar. */
static int texture_count;

/* El fichero entero, en memoria de malloc. NULL si no existe. */
static uint8_t *read_asset(const char *name, size_t *size) {
    FILE *file = asset_open(name);
    if (!file) return NULL;
    uint8_t *data = NULL;
    long length = fseek(file, 0, SEEK_END) ? -1 : ftell(file);
    if (length > 0 && !fseek(file, 0, SEEK_SET) && (data = malloc((size_t)length)))
        if (fread(data, 1, (size_t)length, file) != (size_t)length) free(data), data = NULL;
    fclose(file);
    *size = length > 0 ? (size_t)length : 0;
    return data;
}

int texture_upload(const char *name, int *width_out, int *height_out) {
    size_t size = 0;
    uint8_t *data = read_asset(name, &size);
    if (!data) {
        log_msg("[texture] %s: not found\n", name);
        return -1;
    }
    int width = 0, height = 0;
    uint8_t *rgba = png_decode(data, size, &width, &height);
    free(data);
    if (!rgba) {
        log_msg("[texture] %s: cannot decode it\n", name);
        return -1;
    }
    /* El .so dibuja con glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA): espera el
       color ya multiplicado por el alfa. */
    for (size_t i = 0, n = (size_t)width * height * 4; i < n; i += 4)
        for (int k = 0; k < 3; k++) rgba[i + k] = (uint8_t)(rgba[i + k] * rgba[i + 3] / 255);
    int gl_name = backend_texture_create(rgba, width, height);
    free(rgba);
    *width_out = width;
    *height_out = height;
    return gl_name;
}

int texture_load(const char *name) {
    /* el primer hueco libre: uno que se quitó, o uno sin estrenar */
    int slot = 0;
    while (slot < texture_count && textures[slot].used) slot++;
    if (slot == MAX_TEXTURES) {
        log_msg("[texture] %s: too many textures, the limit is %d\n", name, MAX_TEXTURES);
        return -1;
    }
    int width, height, gl_name = texture_upload(name, &width, &height);
    if (gl_name < 0) return -1;
    if (slot == texture_count) texture_count++;
    textures[slot].used = 1;
    textures[slot].gl_name = gl_name;
    textures[slot].width = width;
    textures[slot].height = height;
    log_msg("[texture] %s: id %d, %dx%d, gl %d\n", name, slot + 1, width, height, gl_name);
    return slot + 1; /* el .so cuenta desde 1 */
}

/* El hueco de un id, o -1 si no es una textura cargada. */
static int slot_of(int id) {
    return id >= 1 && id <= texture_count && textures[id - 1].used ? id - 1 : -1;
}

int texture_remove(int id) {
    int slot = slot_of(id);
    if (slot < 0) return 0;
    backend_texture_delete(textures[slot].gl_name);
    textures[slot].used = 0;
    log_msg("[texture] id %d removed\n", id);
    return 1;
}

int texture_gl_name(int id) { return slot_of(id) < 0 ? -1 : textures[slot_of(id)].gl_name; }
int texture_width(int id) { return slot_of(id) < 0 ? -1 : textures[slot_of(id)].width; }
int texture_height(int id) { return slot_of(id) < 0 ? -1 : textures[slot_of(id)].height; }
