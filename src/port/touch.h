/* port/touch.h: el ratón como un dedo en la pantalla. */
#pragma once
#include "common.h"

/* Una vez por imagen: si el botón izquierdo cambia o el ratón se mueve con él
   pulsado, se lo dice al juego como un toque. */
void touch_update(arm_ptr env);
