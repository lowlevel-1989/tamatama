/* port/game/resource_manager.c: los ficheros locales del juego (lo que guarda)
   van en data_dir(), donde la plataforma deja escribir: en Linux, resources/
   junto al programa. Si el juego abre uno que no ha guardado, se busca entre
   los assets del .apk con asset_open. */
#include "services.h"
#include "platform.h"
#include "resource_manager.h"
#include <limits.h>
#include <sys/stat.h>

#define MAX_RESOURCES 32
#define MAX_NAME 256

typedef struct {
    FILE *file;          /* NULL: hueco libre */
    char name[MAX_NAME]; /* solo para los mensajes */
} Resource;

static Resource resources[MAX_RESOURCES];

/* Guarda el fichero en el primer hueco libre y devuelve su posición. */
static int resource_add(const char *name, FILE *file) {
    for (int id = 0; id < MAX_RESOURCES; id++) {
        if (!resources[id].file) {
            resources[id].file = file;
            snprintf(resources[id].name, MAX_NAME, "%s", name);
            return id;
        }
    }
    fclose(file);
    log_msg("[resource] %s: too many open files, the limit is %d\n", name, MAX_RESOURCES);
    return -1;
}

/* data_dir(), con name detrás si no es NULL. Si no cabe, una ruta vacía, que no
   abre nada. */
static const char *local_path(const char *name) {
    static char path[PATH_MAX];
    if (snprintf(path, sizeof path, "%s%s%s", data_dir(), name ? "/" : "", name ? name : "") >=
        (int)sizeof path)
        path[0] = 0;
    return path;
}

static FILE *resource_get(int id) {
    return id >= 0 && id < MAX_RESOURCES ? resources[id].file : NULL;
}

int resource_open_local(const char *name) {
    FILE *file = fopen(local_path(name), "rb");
    int asset = !file; /* no está guardado: se busca entre los assets */
    if (asset) file = asset_open(name);
    if (!file) {
        log_msg("[resource] open %s for reading: not found\n", name);
        return -1;
    }
    int id = resource_add(name, file);
    log_msg("[resource] open %s for reading%s: id %d\n", name,
            asset ? " (asset)" : "", id);
    return id;
}

int resource_open_asset(const char *name) {
    FILE *file = asset_open(name);
    if (!file) {
        log_msg("[resource] open %s for reading (asset): not found\n", name);
        return -1;
    }
    int id = resource_add(name, file);
    log_msg("[resource] open %s for reading (asset): id %d\n", name, id);
    return id;
}

int resource_create_local(const char *name) {
    mkdir(local_path(NULL), 0755);   /* en Android esta carpeta ya existe */
    FILE *file = fopen(local_path(name), "wb");
    if (!file) {
        log_msg("[resource] open %s for writing: failed\n", name);
        return -1;
    }
    int id = resource_add(name, file);
    log_msg("[resource] open %s for writing: id %d\n", name, id);
    return id;
}

int resource_read(int id, void *buffer, int size) {
    FILE *file = resource_get(id);
    if (!file) {
        log_msg("[resource] read: invalid id %d\n", id);
        return -1;
    }
    int got = (int)fread(buffer, 1, (size_t)size, file);
    // log_msg("[resource] read %s: id %d, %d of %d bytes\n",
    // resources[id].name, id, got, size);
    return got;
}

int resource_write(int id, const void *data, int size) {
    FILE *file = resource_get(id);
    if (!file) {
        log_msg("[resource] write: invalid id %d\n", id);
        return -1;
    }
    int put = (int)fwrite(data, 1, (size_t)size, file);
    // log_msg("[resource] write %s: id %d, %d of %d bytes\n",
    // resources[id].name, id, put, size);
    return put;
}

int resource_close(int id) {
    FILE *file = resource_get(id);
    if (!file) {
        log_msg("[resource] close: invalid id %d\n", id);
        return -1;
    }
    log_msg("[resource] close %s: id %d\n", resources[id].name, id);
    fclose(file);
    resources[id].file = NULL;
    return 0;
}
