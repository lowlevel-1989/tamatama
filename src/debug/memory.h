/* debug/memory.h: leer la memoria del juego por tipo y por lugar. Lo usan
   watch.c y debug_server.c. */
#pragma once
#include "debug.h"

typedef enum { TYPE_I8, TYPE_U8, TYPE_I16, TYPE_U16, TYPE_I32, TYPE_U32, TYPE_F32, TYPE_PTR, TYPE_STR } ValueType;

/* Convierte "i32" en su tipo. Devuelve 0 si no existe. */
int value_type_parse(const char *text, ValueType *type);
/* Cuántos bytes ocupa un valor de ese tipo. Un texto cuenta como 1. */
int value_bytes(ValueType type);
/* Escribe en text el valor que hay en esa dirección. */
void value_format(ValueType type, arm_ptr address, char *text, size_t room);
/* La dirección de un lugar: @0x4AD0 es una posición en el .so, #0x60805498 o
   0x60805498 es una dirección absoluta, y cualquier otra cosa es un símbolo.
   Devuelve 0 si no existe. */
arm_ptr place_start(const char *place);
arm_ptr place_follow(arm_ptr address, const char *place);
arm_ptr place_resolve(const char *place);
