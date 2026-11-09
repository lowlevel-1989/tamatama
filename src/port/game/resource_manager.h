/* port/game/resource_manager.h: 
 * abrir, leer y cerrar los ficheros del juego. No sabe nada de Java ni de JNI:
 * cada fichero abierto es un número, y -1 significa error. */
#pragma once

int resource_open_local(const char *name);
/* un asset del .apk, por su nombre dentro de assets/ */
int resource_open_asset(const char *name);
/* devuelve los bytes leídos */
int resource_read(int id, void *buffer, int size);
/* devuelve los bytes saltados: menos si se llega al final */
int resource_skip(int id, int count);
int resource_close(int id);
int resource_create_local(const char *name);
/* un fichero entero por su nombre, sin abrirlo con un id: la partida guardada
   o, si no existe, el asset. -1 si no está */
int resource_file_size(const char *name);
int resource_read_file(const char *name, int offset, void *buffer, int size);
/* devuelve los bytes escritos */
int resource_write(int id, const void *data, int size);
