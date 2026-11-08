/* backend/backend.h: lo que el programa necesita de la plataforma. Tu código
   solo conoce esto. Cada carpeta de backend define estas funciones, y el
   Makefile compila una sola:
     sdl    ventana y OpenGL reales
     dummy  sin ventana y sin dibujo: no necesita ninguna library
     soft   sin ventana, dibuja en memoria: lo usa la demo para comparar
            imágenes */
#pragma once

/* Abre la ventana y prepara la imagen. Va antes de llamar al juego. */
void backend_init(int width, int height);
/* Atiende los eventos pendientes. Va una vez por imagen. */
void backend_process_events(void);
/* Termina la imagen actual y la muestra. */
void backend_swap_buffers(void);
/* 1 cuando el programa debe terminar. */
int backend_should_close(void);
/* Escribe la posición del pointer y devuelve 1 si está pulsado. */
int backend_pointer(int *x, int *y);
/* Cierra la ventana. */
void backend_cleanup(void);
