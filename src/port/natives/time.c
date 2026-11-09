/* port/natives/time.c: la hora, desde Java. */
#include "common.h"
#include <time.h>

/* Como (int) System.currentTimeMillis() en Java: la hora del sistema en
   milisegundos, recortada a 32 bits. Da la vuelta cada ~50 días, pero la
   diferencia entre dos llamadas sigue valiendo. */
void native_OnGetSystemTime(Regs *r) {
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    uint64_t ms = (uint64_t)now.tv_sec * 1000u + (uint64_t)now.tv_nsec / 1000000u;
    r->r[0] = (uint32_t)ms;
}
