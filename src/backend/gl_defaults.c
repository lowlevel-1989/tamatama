/* backend/gl_defaults.c: una versión de cada función de gl_table.h, para las
   que el backend no sabe hacer. Son definiciones débiles: si el backend define
   la misma función, el linker elige esa. Así todos los backends tienen la lista
   completa y nada falla al hacer el link. La primera vez que se llama a una de
   estas, lo avisa. */
#include "common.h"

#define NATIVE(name, fn)                                                  \
    __attribute__((weak)) void fn(Regs *r) {                              \
        static int warned;                                                \
        if (!warned) {                                                    \
            warned = 1;                                                   \
            log_msg("[gl] %s: not implemented in this backend\n", name);  \
        }                                                                 \
        r->r[0] = 0;                                                      \
    }
#include "gl_table.h"
