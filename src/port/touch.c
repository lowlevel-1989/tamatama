/* port/touch.c: el ratón como un dedo, con id 0. El .so espera los toques por
   dos métodos nativos, con la posición en píxeles de la ventana, como los da
   Android: él mismo los pasa a su escala, con el tamaño que recibió init.
     pulsar el botón izquierdo     -> GameInputSetTouches(0, x, y, x, y)
     moverlo con el botón pulsado  -> GameInputSetTouches(0, x, y, x0, y0)
     soltarlo                      -> GameInputReleaseTouches(0, x, y)
   x0 e y0 son la posición anterior. Los float van como bits en los registers:
   el .so es softfp. */
#include "backend.h"
#include "methods.h"
#include "touch.h"

void touch_update(arm_ptr env) {
    static int was_down, last_x, last_y;
    int x, y, down = backend_pointer(&x, &y);
    if (down && (!was_down || x != last_x || y != last_y)) {
        int from_x = was_down ? last_x : x, from_y = was_down ? last_y : y;
        uint32_t args[7] = {env, FAKE_JCLASS, 0, float_to_bits((float)x), float_to_bits((float)y),
                            float_to_bits((float)from_x), float_to_bits((float)from_y)};
        call_method(METHOD("GameInputSetTouches"), args, 7);
    } else if (!down && was_down) {
        uint32_t args[5] = {env, FAKE_JCLASS, 0, float_to_bits((float)x), float_to_bits((float)y)};
        call_method(METHOD("GameInputReleaseTouches"), args, 5);
    }
    was_down = down;
    last_x = x;
    last_y = y;
}
