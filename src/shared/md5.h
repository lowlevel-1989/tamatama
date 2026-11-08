/* shared/md5.h: el MD5 de un fichero, para reconocer la versión del .apk. */
#pragma once
#include <stdint.h>

/* Escribe en hex las 32 cifras y un 0. Devuelve 0, o -1 si no se pudo leer el fichero. */
int md5_file(const char *path, char hex[33]);
