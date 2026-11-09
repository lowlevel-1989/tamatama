/* backend/backend.h: lo que el programa necesita de la plataforma. Tu código
   solo conoce esto. Cada carpeta de backend define estas funciones, y el
   Makefile compila una sola:
     sdl    ventana y OpenGL reales
     dummy  sin ventana y sin dibujo: no necesita ninguna library
     soft   sin ventana, dibuja en memoria: lo usa la demo para comparar
            imágenes */
#pragma once
#include <stddef.h>

/* Abre la ventana y prepara la imagen. Va antes de llamar al juego. */
void backend_init(int width, int height);
/* Prepara la imagen como la espera el .so antes de dibujar: fondo negro,
   coordenadas en píxeles de 320x480 con el 0 arriba y mezcla con el color
   multiplicado por el alfa. Va una vez por imagen, antes de que el juego
   dibuje. */
void backend_begin_frame(void);
/* Atiende los eventos pendientes. Va una vez por imagen. */
void backend_process_events(void);
/* Termina la imagen actual y la muestra. */
void backend_swap_buffers(void);
/* 1 cuando el programa debe terminar. */
int backend_should_close(void);
/* 1 una sola vez después de cada cambio de tamaño de la ventana. */
int backend_resized(void);
/* Escribe la posición del pointer y devuelve 1 si está pulsado. La posición va
   en píxeles del tamaño con que se abrió la ventana, aunque luego cambie: es
   el que conoce el juego. */
int backend_pointer(int *x, int *y);
/* Cierra la ventana. */
void backend_cleanup(void);

/* Sube los píxeles RGBA a una textura nueva y devuelve su nombre en GL (0 sin
   GL). Cargar la imagen es cosa del juego: src/port/game/texture.c. */
int backend_texture_create(const unsigned char *rgba, int width, int height);
/* Borra la textura de GL con ese nombre. */
void backend_texture_delete(int gl_name);
/* La textura que se repite en las bandas que sobran alrededor del juego
   cuando la ventana no tiene su proporción. width y height: lo que mide cada
   repetición, en coordenadas del juego (320x480). Sin llamarla, las bandas son
   blancas. */
void backend_set_bands(int gl_name, int width, int height);

/* El sonido. Cargar el fichero es cosa del juego (src/port/game/sound.c): el
   backend solo lo decodifica y lo hace sonar. music = 1 para la música, que
   suena de una en una; 0 para un efecto, que puede sonar con otros. */
/* Devuelve un handle desde 1, o 0 si no se pudo. data se puede liberar
   después. */
int backend_sound_load(const void *data, size_t size, int music);
void backend_sound_play(int handle, int loop);
void backend_sound_stop(int handle);
/* 1 mientras suena */
int backend_sound_playing(int handle);
/* el volumen de toda la música o de todos los efectos, de 0 a 1 */
void backend_sound_volume(int music, float volume);
