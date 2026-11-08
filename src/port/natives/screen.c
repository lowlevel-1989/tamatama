/* port/natives/screen.c: la pantalla y su tamaño, desde Java. */
#include "common.h"

/* La pone app_main: debería actualizarse cada vez que cambia el tamaño de la
   pantalla. */
int reload_counter = 0;

/* En Android cuenta las llamadas a onSurfaceChanged. Aquí la ventana solo la
   "cambia" una vez, al crearse. */
void native_OnGetReloadCounter(Regs *r) {
    r->r[0] = (uint32_t)reload_counter;
}
