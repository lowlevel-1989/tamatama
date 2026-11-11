/* port/controls.h: el mando y el teclado dentro del juego (controls.c). */
#pragma once
#include "common.h"

/* Una acción del mando o del teclado (INPUT_LEFT...): las direcciones
   mueven el puntero entre los botones, aceptar pulsa el elegido y 1, 2 y 3
   pulsan los del aparato. */
void controls_input(int input);
/* Una vez por imagen, después de step: pone al día los botones y dibuja el
   puntero. */
void controls_frame(void);
/* El juego ha leído un fichero (OnReadFile): si es un CSV de botones
   (tc_*btn*.csv), es una pantalla o un diálogo nuevo. */
void controls_csv_read(const char *name, const char *text, int size);
