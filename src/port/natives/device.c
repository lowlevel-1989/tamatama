/* port/natives/device.c: Device_PrintMessage es un printf del juego, así que
   cada argumento se lee según lo que pida el formato. Un printf normal no
   serviría: los argumentos están en los registers y el stack de ARM, no en los
   de este programa. */
#include "common.h"

/* La palabra número i de los argumentos: r0-r3 y después el stack. */
static uint32_t arg_word(const Regs *r, int i) { return i < 4 ? r->r[i] : stack_arg(r, i - 4); }

/* Un valor de 64 bits va en dos palabras, y siempre empieza en una posición
   par. */
static uint64_t next_wide(const Regs *r, int *next) {
    *next += *next & 1;
    uint64_t value = arg_word(r, *next) | ((uint64_t)arg_word(r, *next + 1) << 32);
    *next += 2;
    return value;
}
void native_Device_PrintMessage(Regs *r) {
    const char *p = to_local(r->r[0]);
    char out[1024];
    size_t used = 0;
    int next = 1; /* r0 es el formato */
    while (*p && used < sizeof out - 1) {
        if (*p != '%') {
            out[used++] = *p++;
            continue;
        }
        /* Copia las opciones y el ancho, sin l, h ni z: aquí cada tamaño tiene
           su propio tipo. */
        char spec[32] = "%";
        size_t n = 1;
        int longs = 0;
        for (p++; *p && strchr("-+ #0123456789.lhzjt", *p); p++) {
            if (*p == 'l') longs++;
            if (!strchr("lhzjt", *p) && n < sizeof spec - 3) spec[n++] = *p;
        }
        char kind = *p ? *p++ : 0;
        size_t room = sizeof out - used;
        int wrote = 0;
        switch (kind) {
        case '%': out[used++] = '%'; continue;
        case 'd':
        case 'i':
            if (longs == 2) {
                strcpy(spec + n, "lld");
                wrote = snprintf(out + used, room, spec, (long long)next_wide(r, &next));
            } else {
                strcpy(spec + n, "d");
                wrote = snprintf(out + used, room, spec, (int32_t)arg_word(r, next++));
            }
            break;
        case 'u':
        case 'x':
        case 'X':
        case 'o':
            if (longs == 2) {
                snprintf(spec + n, 8, "ll%c", kind);
                wrote = snprintf(out + used, room, spec, (unsigned long long)next_wide(r, &next));
            } else {
                snprintf(spec + n, 8, "%c", kind);
                wrote = snprintf(out + used, room, spec, arg_word(r, next++));
            }
            break;
        case 'c':
            strcpy(spec + n, "c");
            wrote = snprintf(out + used, room, spec, (int)arg_word(r, next++));
            break;
        case 'p':
            wrote = snprintf(out + used, room, "0x%x", arg_word(r, next++));
            break;
        case 's': {
            uint32_t text = arg_word(r, next++);
            strcpy(spec + n, "s");
            wrote = snprintf(out + used, room, spec, text ? (const char *)to_local(text) : "(null)");
            break;
        }
        case 'f':
        case 'g':
        case 'e': {
            uint64_t bits = next_wide(r, &next); /* en ... un float siempre
                                                    llega como double */
            double value;
            memcpy(&value, &bits, sizeof value);
            snprintf(spec + n, 8, "%c", kind);
            wrote = snprintf(out + used, room, spec, value);
            break;
        }
        default:
            wrote = snprintf(out + used, room, "%%%c", kind);
        }
        if (wrote > 0) used += (size_t)wrote < room ? (size_t)wrote : room - 1;
    }
    out[used] = 0;
    log_msg("[game] %s\n", out);
    r->r[0] = 0;
}
