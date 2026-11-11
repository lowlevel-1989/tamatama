/* port/touch.h: el ratón como un touch en la pantalla. */
#pragma once
#include "common.h"

/* Una vez por imagen: si el botón izquierdo cambia o el ratón se mueve con él
   pulsado, se lo dice al juego como un toque. */
void touch_update(arm_ptr env);
/* Un toque corto en x, y (coordenadas del juego): se pulsa en el siguiente
   touch_update y se suelta en el de después. Para los botones del mando. */
void touch_tap(float x, float y);
/* Un arrastre del mando: el touch baja en from, llega a to en steps imágenes,
   se queda quieto un momento (para que no haya inercia) y se suelta. Para
   pasar páginas. */
void touch_drag(float from_x, float from_y, float to_x, float to_y, int steps);
/* Mientras un toque espera a que el juego deje de estar ocupado (una
   animación de la lógica, como el nacimiento), el aviso que app.c muestra:
   "hatching..." o "waiting...". NULL si no espera nada. */
const char *touch_waiting(void);
