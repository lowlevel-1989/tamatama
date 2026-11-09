/* port/game/sound.c: los sonidos del juego, tal como los espera el .so. El .so
   los pide por un id, y sound_table.h dice su fichero:
     id -> sound_table.h -> el fichero, la primera vez
       -> el backend lo decodifica -> un handle, que se guarda para el id
       -> el backend lo hace sonar */
#include "services.h"
#include "backend.h"
#include "sound.h"

enum { EFFECT, MUSIC };

static const struct {
    int id;
    const char *file;
    int music, loop;
} table[] = {
#define SOUND(id, file, kind, loop) {id, file, kind, loop},
#include "sound_table.h"
#undef SOUND
    {-1, NULL, 0, 0} /* fin de la tabla */
};

/* el handle de cada entrada de la tabla: 0 sin cargar, -1 si falló */
static int handles[sizeof table / sizeof table[0]];

/* La entrada del id en la tabla, o -1. */
static int entry_of(int id) {
    for (int i = 0; table[i].file; i++)
        if (table[i].id == id) return i;
    /* el aviso sale una vez por id */
    static unsigned char warned[256];
    if (id >= 0 && id < 256 && !warned[id]) {
        warned[id] = 1;
        log_msg("[sound] id %d is not in sound_table.h\n", id);
    }
    return -1;
}

/* El handle del sonido, cargándolo si hace falta; 0 si no hay. */
static int handle_of(int id, int load) {
    int i = entry_of(id);
    if (i < 0) return 0;
    if (!handles[i] && load) {
        handles[i] = -1;
        FILE *file = asset_open(table[i].file);
        if (!file) {
            log_msg("[sound] %s: not found\n", table[i].file);
            return 0;
        }
        uint8_t *data = NULL;
        long length = fseek(file, 0, SEEK_END) ? -1 : ftell(file);
        if (length > 0 && !fseek(file, 0, SEEK_SET) && (data = malloc((size_t)length)))
            if (fread(data, 1, (size_t)length, file) != (size_t)length) free(data), data = NULL;
        fclose(file);
        int handle = data ? backend_sound_load(data, (size_t)length, table[i].music) : 0;
        free(data);
        if (handle) {
            handles[i] = handle;
            log_msg("[sound] %s: id %d\n", table[i].file, id);
        }
    }
    return handles[i] > 0 ? handles[i] : 0;
}

void sound_play(int id) {
    int i = entry_of(id), handle = handle_of(id, 1);
    if (handle) backend_sound_play(handle, table[i].loop);
}

void sound_stop(int id) {
    int handle = handle_of(id, 0);
    if (handle) backend_sound_stop(handle);
}

int sound_playing(int id) {
    int handle = handle_of(id, 0);
    return handle ? backend_sound_playing(handle) : 0;
}

void sound_volume(int music, float volume) { backend_sound_volume(music, volume); }
