/* port/game/resource_manager.h: 
 * abrir, leer y cerrar los ficheros del juego. No sabe nada de Java ni de JNI:
 * cada fichero abierto es un número, y -1 significa error. */
#pragma once

int resource_open_local(const char *name);
/* un asset del .apk, por su nombre dentro de assets/ */
int resource_open_asset(const char *name);
/* devuelve los bytes leídos */
int resource_read(int id, void *buffer, int size);
int resource_close(int id);
int resource_create_local(const char *name);
/* devuelve los bytes escritos */
int resource_write(int id, const void *data, int size);
