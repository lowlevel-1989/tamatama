/* backend/backend.h: lo que el programa necesita de la plataforma. Tu código
   solo conoce esto. Cada carpeta de backend define estas funciones, y el
   Makefile compila una sola:
     sdl    ventana y OpenGL reales
     dummy  sin ventana y sin dibujo: no necesita ninguna library
     soft   sin ventana, dibuja en memoria: lo usa la demo para comparar
            imágenes */
#pragma once
#include <stddef.h>

/* Abre la ventana y prepara la imagen. Va antes de llamar al juego. Si la
   ventana ya está abierta (el menú de --menu la abre antes), solo le pone ese
   tamaño. */
void backend_init(int width, int height);
/* Prepara la imagen como la espera el .so antes de dibujar: fondo negro,
   coordenadas en píxeles de 320x480 con el 0 arriba y mezcla con el color
   multiplicado por el alfa. Va una vez por imagen, antes de que el juego
   dibuje. */
void backend_begin_frame(void);
/* El color de las bandas que sobran alrededor de los 320x480, 0xRRGGBB:
   blanco por defecto. El menú pone el de su fondo y lo devuelve al salir. */
void backend_set_background(unsigned rgb);
/* 1 cuando el juego ya ha dibujado algo (su primer glDrawArrays). */
int backend_game_drew(void);
/* Cuántas veces ha dibujado el juego desde que empezó: si no cambia durante
   un step, el juego aún no ha dibujado nada de esa imagen. */
int backend_draw_count(void);
/* Atiende los eventos pendientes. Va una vez por imagen. */
void backend_process_events(void);
/* Termina la imagen actual y la muestra. */
void backend_swap_buffers(void);
/* 1 cuando el programa debe terminar. */
int backend_should_close(void);
/* 1 una sola vez después de cada cambio de tamaño de la ventana. */
int backend_resized(void);
/* La entrada, sin teclas ni botones concretos: las acciones del juego. Las
   cuatro direcciones mueven el puntero entre los botones de cada pantalla
   (port/controls.c) y aceptar pulsa el elegido; 1, 2 y 3 son los tres
   botones del Tamagotchi. Volver solo lo da el teclado. Cada backend decide
   qué tecla o botón es cada acción. */
enum {
    INPUT_NONE,
    INPUT_LEFT,
    INPUT_RIGHT,
    INPUT_ACCEPT,
    INPUT_1,
    INPUT_2,
    INPUT_3,
    INPUT_BACK,
    INPUT_UP,
    INPUT_DOWN,
    /* aceptar y 3 en el mismo botón (A, o lo que se configure así): toca lo
       que hay bajo el puntero, y si no hay, en el aparato es su botón C */
    INPUT_ACCEPT3,
};
/* La siguiente pulsación pendiente, o INPUT_NONE. Las recoge
   backend_process_events. */
int backend_input_next(void);
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

/* Dibujar sin el juego, para el menú de --menu. Van en coordenadas del juego
   (320x480), entre backend_begin_frame y backend_swap_buffers. rgb es el
   color: 0xRRGGBB. */
void backend_fill_rect(int x, int y, int width, int height, unsigned rgb);
/* El mapeador de botones (la opción "Configure controls"): pide las
   acciones del juego en el mando o en el teclado y las guarda. 1 si guarda
   el mando, 2 si guarda el teclado, 0 si se cancela, -1 si no se puede. */
int backend_configure_controller(void);
/* Dibujar encima del juego, después de step: entre backend_overlay_begin y
   backend_overlay_end se dibuja en coordenadas del juego (320x480) y el
   estado de GL del juego queda como estaba. */
void backend_overlay_begin(void);
void backend_overlay_end(void);
/* Hace vibrar los mandos abiertos durante ms milisegundos (los que no
   vibran no hacen nada). Para la vibración del juego, OnSetVibrator. */
void backend_vibrate(int ms);
/* Lo dibujado hasta ahora en la imagen en curso, en RGB, 3 bytes por
   píxel y con las filas de abajo arriba, en memoria de malloc. NULL si no
   se puede. Para las capturas de pantalla. */
unsigned char *backend_read_pixels(int *width, int *height);
/* Un triángulo relleno, en coordenadas del juego. */
void backend_fill_triangle(float x0, float y0, float x1, float y1, float x2, float y2, unsigned rgb);
/* Texto ASCII con una fuente de 6x7 píxeles, a scale veces su tamaño. */
void backend_draw_text(int x, int y, const char *text, int scale, unsigned rgb);
/* El ancho que ocupará ese texto. */
int backend_text_width(const char *text, int scale);
