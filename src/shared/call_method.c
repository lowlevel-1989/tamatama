/* shared/call_method.c: llamar al juego por nombre, igual en las dos plataformas. Aquí un método
   se entrega a tu función nativa o al código del .so. */
#include "debug.h"

/* Llama a un método por nombre. words son sus argumentos, como palabras de 32 bits. Si tiene
   versión nativa corre aquí mismo, sin pasar por ARM. Al volver, comprueba las variables que se
   estén siguiendo. */
uint64_t call_method(const char *name, const uint32_t *words, int n) {
    int index = native_active(name);
    uint64_t result;
    if (index >= 0) {
        static arm_ptr stack; /* para los argumentos que no caben en r0-r3 */
        Regs r = {{0, 0, 0, 0}, 0};
        for (int i = 0; i < n && i < 4; i++) r.r[i] = words[i];
        if (n > 4) {
            if (!stack) stack = shared_alloc(256);
            for (int i = 4; i < n; i++) ((uint32_t *)to_local(stack))[i - 4] = words[i];
            r.sp = stack;
        }
        native_table[index].fn(&r);
        result = r.r[0] | ((uint64_t)r.r[1] << 32);
    } else {
        result = call_arm_by_name(name, words, n);
    }
    debug_after_call(name);
    return result;
}
/* Crea un jstring con el formato de la JVM falsa. */
arm_ptr new_jstring(const char *t) {
    uint32_t n = (uint32_t)strlen(t);
    arm_ptr g = shared_alloc((uint32_t)sizeof(JniObject) + n + 1);
    JniObject *o = to_local(g);
    o->len = n;
    o->dirty = 0;
    memcpy(o->data, t, n + 1);
    return g;
}
