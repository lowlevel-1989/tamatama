/* port/natives/screen.c: la pantalla y su tamaño, desde Java. */
#include "common.h"

/* La lleva app_main: 1 al crear la ventana y uno más cada vez que cambia su
   tamaño. */
int reload_counter = 0;

/* En Android cuenta las llamadas a onSurfaceChanged: la primera es al crearse
   la pantalla, y las siguientes, cuando cambia su tamaño. */
void native_OnGetReloadCounter(Regs *r) {
    r->r[0] = (uint32_t)reload_counter;
}
