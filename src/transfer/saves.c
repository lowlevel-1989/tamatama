/* transfer/saves.c: leer, resumir, guardar y restaurar partidas. Nada se
   queda a medias: se escribe aparte y se cambia con rename. */
#include "saves.h"
#include "common.h"
#include "platform.h"
#include <sys/stat.h>
#include <unistd.h>

static const char *const FILES[] = {"Tama2Movie_gamestatus_0.dat", "gamestatus_0.dat", NULL};

int save_known_file(const char *file) {
    for (int i = 0; FILES[i]; i++)
        if (!strcmp(file, FILES[i])) return 1;
    return 0;
}

static uint32_t read_be32(const uint8_t *p) {
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}

/* El resumen de una partida de 768 bytes (doc/code/saveRecord.c): m_para[i]
   en 0x118 + i * 4, en big endian. Solo los días, AGE_01: el personaje
   (ID_00) no, porque cada juego tiene su propia tabla de ids. */
static void summarize(const uint8_t *data, long size, const char *file, char *out, size_t room) {
    out[0] = 0;
    if (size != 768 || strcmp(file, FILES[0])) return;
    int32_t age = (int32_t)read_be32(data + 0x11c);
    if (age >= 0 && age < 1000) snprintf(out, room, "Day %d", (int)age);
}

/* La carpeta de partidas de una versión. */
static void folder_of(const char *md5, char *out, size_t room) { snprintf(out, room, "%s/%s/files", data_dir(), md5); }

uint8_t *save_read(const char *path, long *size) {
    FILE *file = fopen(path, "rb");
    if (!file) return NULL;
    uint8_t *data = NULL;
    if (!fseek(file, 0, SEEK_END)) {
        long n = ftell(file);
        if (n > 0 && n <= SAVE_MAX && !fseek(file, 0, SEEK_SET) && (data = malloc((size_t)n))) {
            if (fread(data, 1, (size_t)n, file) == (size_t)n) {
                *size = n;
            } else {
                free(data);
                data = NULL;
            }
        }
    }
    fclose(file);
    return data;
}

int save_find(const char *md5, int backup, Save *save) {
    char folder[PATH_MAX];
    folder_of(md5, folder, sizeof folder);
    for (int i = 0; FILES[i]; i++) {
        if (snprintf(save->path, sizeof save->path, "%s/%s%s", folder, FILES[i], backup ? ".backup" : "") >=
            (int)sizeof save->path)
            continue;
        struct stat info;
        if (stat(save->path, &info) || info.st_size <= 0 || info.st_size > SAVE_MAX) continue;
        save->file = FILES[i];
        save->size = (long)info.st_size;
        save->date = info.st_mtime;
        save->summary[0] = 0;
        long size = 0;
        uint8_t *data = save_read(save->path, &size);
        if (data) summarize(data, size, save->file, save->summary, sizeof save->summary);
        free(data);
        return 1;
    }
    return 0;
}

/* La ruta de file en la carpeta de md5, y la misma con suffix detrás. */
static int paths_of(const char *md5, const char *file, const char *suffix, char *path, char *other) {
    char folder[PATH_MAX];
    folder_of(md5, folder, sizeof folder);
    return snprintf(path, PATH_MAX, "%s/%s", folder, file) < PATH_MAX &&
           snprintf(other, PATH_MAX, "%s/%s%s", folder, file, suffix) < PATH_MAX;
}

int save_store(const char *md5, const char *file, const uint8_t *data, long size) {
    char folder[PATH_MAX], path[PATH_MAX], backup[PATH_MAX], temporary[PATH_MAX];
    /* la versión puede no haberse jugado nunca aquí: ni resources/{md5} */
    char version[PATH_MAX];
    snprintf(version, sizeof version, "%s/%s", data_dir(), md5);
    mkdir(data_dir(), 0755);
    mkdir(version, 0755);
    folder_of(md5, folder, sizeof folder);
    mkdir(folder, 0755);
    if (!paths_of(md5, file, ".backup", path, backup) || !paths_of(md5, file, ".tmp", path, temporary)) return 0;
    FILE *out = fopen(temporary, "wb");
    if (!out) return 0;
    int ok = fwrite(data, 1, (size_t)size, out) == (size_t)size;
    if (fclose(out) || !ok) {
        unlink(temporary);
        return 0;
    }
    if (!access(path, F_OK) && rename(path, backup)) {
        unlink(temporary);
        return 0;
    }
    if (rename(temporary, path)) {
        rename(backup, path); /* deja la de antes como estaba */
        return 0;
    }
    return 1;
}

int save_restore_backup(const char *md5) {
    Save backup;
    char path[PATH_MAX], swap[PATH_MAX];
    if (!save_find(md5, 1, &backup) || !paths_of(md5, backup.file, ".swap", path, swap)) return 0;
    int had = !access(path, F_OK);
    if (had && rename(path, swap)) return 0;
    if (rename(backup.path, path)) {
        if (had) rename(swap, path);
        return 0;
    }
    if (had) rename(swap, backup.path);
    return 1;
}

uint32_t save_crc32(const uint8_t *data, size_t n) {
    uint32_t crc = 0xffffffffu;
    while (n--) {
        crc ^= *data++;
        for (int k = 0; k < 8; k++) crc = crc >> 1 ^ (0xedb88320u & (0u - (crc & 1)));
    }
    return ~crc;
}

void save_date_text(time_t date, char *out, size_t room) {
    struct tm parts;
    if (!localtime_r(&date, &parts) || !strftime(out, room, "%Y-%m-%d %H:%M", &parts)) snprintf(out, room, "?");
}
