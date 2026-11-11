/* port/touch.c: el ratón como un touch, con id 0. El .so espera los toques por
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
#include <time.h>

/* El toque pendiente del mando: un touch que va de (from) a (to) en steps
   imágenes, se queda quieto hold imágenes y se suelta en la siguiente. Un
   toque es el que no se mueve, en una imagen. state cuenta las imágenes que
   quedan, la del soltar incluida. */
#define DRAG_HOLD 8 /* quieto antes de soltar: sin velocidad, no hay inercia */
static int tap_state, tap_steps, tap_hold;
static float tap_from_x, tap_from_y, tap_to_x, tap_to_y;

void touch_drag(float from_x, float from_y, float to_x, float to_y, int steps) {
    tap_from_x = from_x;
    tap_from_y = from_y;
    tap_to_x = to_x;
    tap_to_y = to_y;
    tap_steps = steps < 1 ? 1 : steps;
    tap_hold = steps > 1 ? DRAG_HOLD : 0;
    tap_state = tap_steps + tap_hold + 1;
}

void touch_tap(float x, float y) { touch_drag(x, y, x, y, 1); }

/* La lógica del Tamagotchi (Tama2MovieMain) ocupada en una
   transformación: la sección 5 (m_gameSec) con la animación 7 (m_animNum,
   doc/code/doControlIvent/transform.c). Dura unos 12 s; la del huevo al
   bebé es el nacimiento. Hasta que acaba no atiende, y un toque que cambia
   de pantalla (Home) deja la interfaz en blanco esperándola. Así que,
   mientras tanto, un toque nuevo no llega al juego: se guarda (held) y se da
   al acabar, y app.c avisa con "hatching..." mientras nace o con
   "waiting..." si es otra transformación. Las demás animaciones (comer,
   jugar...) no esperan: el juego las atiende como siempre. Angel no tiene
   esta clase: nunca está ocupado. Devuelve 7 o 0; en *hatching, 1 si la
   transformación empezó con el huevo (m_para[0], el personaje: 0 es el
   huevo). */
#define HATCH_ANIM 7
static int game_busy(int *hatching) {
    static arm_ptr section, anim, para;
    static int looked, id_before = -1, in_transform, from_egg;
    if (!looked) {
        looked = 1;
        section = arm_symbol("_ZN24Tama2MovieMain_Implement9m_gameSecE");
        anim = arm_symbol("_ZN24Tama2MovieMain_Implement9m_animNumE");
        para = arm_symbol("_ZN24Tama2MovieMain_Implement6m_paraE");
    }
    *hatching = 0;
    if (!section || !anim) return 0;
    int busy = *(int32_t *)to_local(section) == 5 ? *(int32_t *)to_local(anim) : 0;
    /* El personaje cambia al empezar la animación (initCharacterData), así
       que vale el de antes: el último visto sin ella. */
    if (busy != HATCH_ANIM) {
        in_transform = 0;
        if (para) id_before = *(int32_t *)to_local(para);
    } else if (!in_transform) {
        in_transform = 1;
        from_egg = id_before == 0;
    }
    *hatching = busy == HATCH_ANIM && from_egg;
    return busy == HATCH_ANIM ? busy : 0;
}

/* Al acabar la animación, el toque guardado espera un poco más: así se ve
   un momento cómo queda (el bebé recién nacido) antes de cambiar de
   pantalla. */
#define HELD_AFTER_MS 3000

static long long now_ms(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1000LL + t.tv_nsec / 1000000;
}

/* El toque guardado mientras el juego está ocupado: dónde empezó, si el
   touch se ha movido (entonces era un arrastre y no se da) y si es del mando
   (tap_state, que espera tal cual). */
#define HELD_SLOP 8
static int held, held_x, held_y, held_moved;
static int held_anim;        /* la animación a la que esperó, o 0 */
static long long held_until; /* 0 mientras la animación sigue */
static const char *waiting;
const char *touch_waiting(void) { return waiting; }

void touch_update(arm_ptr env) {
    static int was_down, last_x, last_y;
    int x, y, down = backend_pointer(&x, &y), from_pad = 0;
    /* Con el juego ocupado, lo que empieza ahora espera (ver game_busy); lo
       que ya estaba pulsado sigue. */
    int hatching, anim = game_busy(&hatching);
    if (was_down) anim = 0;
    if (anim && (down || tap_state) && !held_anim) /* esperó a esta */
        log_msg("[touch] waiting for animation %d%s\n", anim, hatching ? " (hatching)" : "");
    if (anim && (down || tap_state)) held_anim = anim;
    int pending = held_anim && (held || tap_state);
    if (!pending) held_anim = 0;
    if (anim || !pending) held_until = 0;
    else if (!held_until) held_until = now_ms() + HELD_AFTER_MS;
    int busy = anim || (pending && now_ms() < held_until);
    /* "hatching..." solo mientras nace; después (la pausa para ver al bebé)
       o con otra transformación, "waiting..." */
    waiting = !busy || !(pending || down) ? NULL : anim && hatching ? "hatching..." : "waiting...";
    if (busy) {
        if (down && !held) {
            held = 1;
            held_x = x;
            held_y = y;
            held_moved = 0;
        } else if (down && (abs(x - held_x) > HELD_SLOP || abs(y - held_y) > HELD_SLOP)) {
            held_moved = 1;
        }
        return; /* ni el touch ni el toque del mando llegan todavía */
    }
    if (held) { /* ya no está ocupado: el toque guardado, si lo era */
        held = 0;
        if (!held_moved) touch_tap((float)held_x, (float)held_y);
    }
    /* Un toque del mando manda sobre el ratón: pulsado mientras se mueve,
       suelto en la imagen siguiente. */
    if (tap_state && !down) {
        int done = tap_steps + tap_hold + 1 - tap_state; /* ya pulsadas */
        float t = tap_steps > 1 ? (float)(done < tap_steps ? done : tap_steps - 1) / (tap_steps - 1) : 0;
        t = t * t * (3 - 2 * t); /* acelera y frena: sin golpes */
        x = (int)(tap_from_x + (tap_to_x - tap_from_x) * t + 0.5f);
        y = (int)(tap_from_y + (tap_to_y - tap_from_y) * t + 0.5f);
        down = tap_state > 1;
        tap_state--;
        from_pad = 1;
    }
    /* Los del mando van en cada imagen, también quietos: así el juego ve que
       el touch se ha parado. */
    if (down && (!was_down || x != last_x || y != last_y || from_pad)) {
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
