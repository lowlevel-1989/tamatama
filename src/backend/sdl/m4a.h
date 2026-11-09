/* backend/sdl/m4a.h: un M4A (AAC dentro de un MP4), convertido a WAV. */
#pragma once
#include <stddef.h>
#include <stdint.h>

/* 1 si los datos son un fichero MP4 o M4A. */
int m4a_detect(const uint8_t *data, size_t size);
/* Un WAV PCM de 16 bits en memoria de malloc, con su tamaño en wav_size; NULL
   si no se pudo decodificar. */
uint8_t *m4a_to_wav(const uint8_t *data, size_t size, size_t *wav_size);
