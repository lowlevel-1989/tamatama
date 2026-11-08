/* shared/app_default.c: el programa que se usa mientras src/port no tenga el
   suyo. Es una definición débil: si escribes app_main en src/port, el linker
   elige la tuya sin que haya que tocar nada aquí. Con el port vacío ya se carga
   el .so, se ejecutan sus constructores y
   JNI_OnLoad, y el informe final muestra a qué ha llamado. */
#include "common.h"

__attribute__((weak)) int app_main(void) {
    log_msg("[loader] src/port has no app_main: nothing to run\n");
    return 0;
}
