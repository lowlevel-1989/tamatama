/* port/game/resource_manager.h: 
 * abrir, leer y cerrar los ficheros del juego. No sabe nada de Java ni de JNI: cada fichero abierto es un número, y -1 significa error. */
#pragma once

int resource_open_local(const char *name);
int resource_read(int id, void *buffer, int size);   /* devuelve los bytes leídos */
int resource_close(int id);
int resource_create_local(const char *name);
int resource_write(int id, const void *data, int size);   /* devuelve los bytes escritos */
