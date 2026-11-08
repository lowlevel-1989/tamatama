/* arm/callers.c: la opción --callers. Muestra desde dónde se llama cada método
   de Java, como un árbol y con los nombres de las funciones. No hay stack
   frames que recorrer, así que busca en el stack valores que parezcan
   direcciones de retorno: alguno puede ser un resto de una llamada anterior. */
#include "arm.h"

#define MAX_CALLERS 64
#define WORDS_TO_SCAN 128

static int enabled;
/* El árbol anterior: lo que coincide con él no se vuelve a mostrar. */
static uint32_t previous[MAX_CALLERS];
static int previous_count;

void callers_enable(void) { enabled = 1; }
int callers_enabled(void) { return enabled; }

/* Copia un nombre C++ sin sus tipos: _ZN15DeviceImplement5WriteEi pasa a
   DeviceImplement::Write. Un nombre que no empiece por _Z se copia tal cual. */
static void demangle(const char *name, char *text, size_t room) {
    const char *p = name + 2;
    size_t used = 0;
    text[0] = 0;
    if (!strncmp(name, "_Z", 2)) {
        int nested = *p == 'N';
        if (nested) p++;
        while (*p == 'K') p++; /* const */
        while (*p >= '0' && *p <= '9') {
            size_t length = 0;
            while (*p >= '0' && *p <= '9') length = length * 10 + (size_t)(*p++ - '0');
            if (used + length + 3 >= room) break;
            if (used) {
                memcpy(text + used, "::", 2);
                used += 2;
            }
            memcpy(text + used, p, length);
            used += length;
            text[used] = 0;
            p += length;
            if (!nested) break;
        }
    }
    if (!used) snprintf(text, room, "%s", name);
}
/* Escribe la función que contiene una posición del .so: "Clase::metodo+0x7a", o
   "?" si no hay símbolo. */
void describe_position(uint32_t position, char *text, size_t room) {
    uint32_t start;
    const char *name = elf_function_at(position, &start);
    if (!name) {
        snprintf(text, room, "?");
        return;
    }
    char plain[96];
    demangle(name, plain, sizeof plain);
    snprintf(text, room, "%s+0x%x", plain, position - start);
}
/* 1 si justo antes de esta dirección hay una instrucción de llamada. Una
   dirección de retorno impar es de código Thumb, y una par, de código ARM. */
static int follows_a_call(uint32_t address) {
    if (address & 1) {
        const uint16_t *next = (const uint16_t *)(address - 1);
        uint16_t low = next[-1], high = next[-2];
        /* blx rN */
        if ((low & 0xFF80) == 0x4780) return 1;
        /* bl o blx */
        return (high & 0xF800) == 0xF000 && (low & 0xC000) == 0xC000;
    }
    if (address & 3) return 0;
    uint32_t word = ((const uint32_t *)address)[-1];
    return (word & 0x0F000000) == 0x0B000000      /* bl */
           || (word & 0xFE000000) == 0xFA000000   /* blx */
           || (word & 0x0FFFFFF0) == 0x012FFF30;  /* blx rN */
}
/* Muestra el árbol de llamadas hasta este método de Java. stack son sus
   argumentos, en el stack. Las 16 palabras anteriores cubren la función
   auxiliar del jni.h, que guarda ahí su dirección de retorno. */
void print_callers(const uint32_t *stack, const char *method) {
    uint32_t found[MAX_CALLERS], chain[MAX_CALLERS];
    int count = 0;
    const uint32_t *word = stack - 16;
    for (int i = 0; i < WORDS_TO_SCAN && count < MAX_CALLERS; i++) {
        uint32_t position;
        if (!elf_position(word[i], &position) || position < 4 || !follows_a_call(word[i])) continue;
        position &= ~1u;
        if (!count || found[count - 1] != position) found[count++] = position;
    }
    /* de fuera hacia dentro */
    for (int i = 0; i < count; i++) chain[i] = found[count - 1 - i];
    int shared = 0;
    while (shared < count && shared < previous_count && chain[shared] == previous[shared]) shared++;
    char indent[2 * MAX_CALLERS + 1], name[128];
    memset(indent, ' ', sizeof indent);
    for (int depth = shared; depth < count; depth++) {
        indent[2 * depth] = 0;
        describe_position(chain[depth], name, sizeof name);
        log_msg("%s@0x%x  %s\n", indent, chain[depth], name);
        indent[2 * depth] = ' ';
    }
    indent[2 * count] = 0;
    log_msg("%s[java] %s\n", indent, method);
    memcpy(previous, chain, (size_t)count * sizeof chain[0]);
    previous_count = count;
}
