/* transfer/identity.h: cómo se presenta cada dispositivo, sin teclado: su
   nombre y una firma de 4 caracteres. */
#pragma once
#include <stddef.h>

/* usuario@hostname (sistema), por ejemplo root@rg35xx (Knulli). En
   Android, el modelo: Pixel 10a (Android). */
void identity_name(char *out, size_t room);
/* Una firma nueva al azar, sin los caracteres que se confunden. */
void identity_new_signature(char out[5]);
/* 1 si text es una firma válida. */
int identity_valid_signature(const char *text);
/* Deja solo ASCII imprimible (lo que dibuja el backend) y lo corta a room. */
void identity_clean(char *text, size_t room);
