/* port/natives/sound.c: el sonido, desde Java. Leen los argumentos de los
   registers y llaman a game/sound.c. Un float de Java llega como double,
   porque el .so lo pasa por varargs; y un double va en un par de registers que
   empieza en uno par, así que va en r1:r2 y r0 queda de relleno. */
#include "common.h"
#include "sound.h"

/* El float del método, desde el double de r1:r2. */
static float arg_volume(const Regs *r) {
    uint64_t bits = r->r[1] | ((uint64_t)r->r[2] << 32);
    double value;
    memcpy(&value, &bits, sizeof value);
    return (float)value;
}

/* Hace sonar el sonido de ese id. */
void native_OnPlaySound(Regs *r) { sound_play((int)r->r[0]); }

/* Lo para. */
void native_OnStopSound(Regs *r) { sound_stop((int)r->r[0]); }

/* En Android siempre devuelve false: el código Java calcula si la música de
   ese id suena, pero pisa el resultado con false antes de devolverlo. El .so
   está hecho con eso, así que se hace igual. */
void native_OnTestSound(Regs *r) { r->r[0] = 0; }

/* El volumen de la música y el de los efectos, de 0 a 1. */
void native_OnSetBgmVolume(Regs *r) { sound_volume(1, arg_volume(r)); }
void native_OnSetSeVolume(Regs *r) { sound_volume(0, arg_volume(r)); }
