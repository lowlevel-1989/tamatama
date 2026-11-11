/* transfer/transfer.h: lo que el resto del programa usa de Transfer Save.
   Todo lo demás está en esta carpeta (ver README.md). */
#pragma once
#include "common.h"

/* La pantalla de Transfer Save, con la ventana ya abierta: la abre el menú
   de --menu. Vuelve con Back. */
void transfer_screen(void);
/* --transfer: abre la ventana, enseña transfer_screen y termina. Así la
   abre Android. */
int transfer_main(void);
