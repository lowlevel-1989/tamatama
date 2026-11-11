/* transfer/saves.h: las partidas que se pasan, en resources/{md5}/files. */
#pragma once
#include <limits.h>
#include <stdint.h>
#include <time.h>

/* Una partida más grande es un error: las de verdad miden menos de 1 KB, y
   así cabe entera en un paquete UDP. */
#define SAVE_MAX 16384

typedef struct {
    char path[PATH_MAX];
    const char *file;   /* uno de los ficheros conocidos */
    long size;
    time_t date;
    char summary[32];   /* "Day 34" o nada */
} Save;

/* La partida de una versión, o su backup: 1 si la hay. */
int save_find(const char *md5, int backup, Save *save);
/* 1 si file es un fichero de partida conocido: el de Classic y L.i.f.e.
   (Tama2Movie_gamestatus_0.dat) o el de Angel (gamestatus_0.dat). */
int save_known_file(const char *file);
/* El fichero entero, en memoria de malloc, o NULL. */
uint8_t *save_read(const char *path, long *size);
/* Guarda una partida recibida: la de antes pasa a backup (el fichero con
   .backup detrás). 1 si todo va bien. */
int save_store(const char *md5, const char *file, const uint8_t *data, long size);
/* Cambia la partida por su backup, y la actual queda como backup. */
int save_restore_backup(const char *md5);
uint32_t save_crc32(const uint8_t *data, size_t n);
/* La fecha como 2026-11-11 18:30. */
void save_date_text(time_t date, char *out, size_t room);
