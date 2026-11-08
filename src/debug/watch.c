/* debug/watch.c: sigue variables de tu juego y muestra cada cambio. No toca el .so: la memoria del
   juego está en la arena, así que aquí se lee directamente.

   En el fichero de reglas, o en la consola de debug:
     watch NOMBRE LUGAR [TIPO]

   LUGAR es dónde empieza:
     g_score       un símbolo que el .so exporta, o uno tuyo definido con symbol
     @0x4AD0       una posición dentro del .so, como las de readelf
     #0x60805498   una dirección absoluta

   Detrás se puede seguir el camino, sin espacios y como en C:
     +0x18         suma, sin leer nada: @0xC2D4C+0x18 es 0xC2D64 dentro del .so
     ->0x10        lee un pointer en la dirección actual y le suma 0x10

   Por ejemplo, para un World *g_world con Entity *player en 0x10 e int health en 4:
     watch vida g_world->0x10->4 i32
       1. Lee g_world: un pointer al World.
       2. Suma 0x10 y lee ese pointer: el Entity.
       3. Suma 4: la dirección de health.

   TIPO es i8, u8, i16, u16, i32, u32, f32, ptr o str. Si no se indica, i32.

   Se comprueba al volver de cada call_method, y también cuando tu código llama a watch_poll. */
#include "memory.h"

#define MAX_WATCHES 64
#define MAX_TEXT 64

typedef struct {
    const char *name;
    const char *place;     /* el LUGAR, tal como se escribió */
    arm_ptr place_address; /* dónde empieza; 0 hasta poder calcularlo */
    ValueType type;
    int has_value;
    char last[MAX_TEXT]; /* el último valor mostrado */
} Watch;

static Watch watches[MAX_WATCHES];
static int watch_count;
static uint32_t poll_number;

/* Añade una variable. spec es lo que va detrás del nombre, por ejemplo "g_world->0x10->4 i32".
   Si está mal escrita, lo avisa y no la añade. */
void watch_add(const char *name, const char *spec) {
    if (watch_count == MAX_WATCHES) {
        debug_report("[watch] too many watches\n");
        return;
    }
    Watch w = {0};
    size_t name_bytes = strlen(name) + 1, spec_bytes = strlen(spec) + 1;
    char *copy = malloc(name_bytes + spec_bytes);
    memcpy(copy, name, name_bytes);
    w.name = copy;
    char *word = copy + name_bytes;
    memcpy(word, spec, spec_bytes);
    w.type = TYPE_I32;
    w.place = word;
    word = next_word(word);
    while (*word && *word != '#') {
        char *next = next_word(word);
        if (!value_type_parse(word, &w.type)) {
            debug_report("[watch] %s: unknown type %s\n", name, word);
            return;
        }
        word = next;
    }
    watches[watch_count++] = w;
}
/* Deja de seguir una variable. Devuelve 1 si existía. */
int watch_remove(const char *name) {
    for (int i = 0; i < watch_count; i++)
        if (!strcmp(watches[i].name, name)) {
            watches[i] = watches[--watch_count];
            return 1;
        }
    return 0;
}
/* Sigue los pointers y devuelve la dirección final. 0 si algún paso no se puede leer. */
static arm_ptr final_address(Watch *w) {
    /* Un símbolo se pregunta aquí y no al leer las reglas: entonces el .so aún no está cargado. */
    if (!w->place_address) w->place_address = place_start(w->place);
    arm_ptr address = place_follow(w->place_address, w->place);
    return arm_readable(address) ? address : 0;
}
/* Convierte el valor actual en texto. */
static void read_value(Watch *w, char *text) {
    arm_ptr address = final_address(w);
    if (address) value_format(w->type, address, text, MAX_TEXT);
    else snprintf(text, MAX_TEXT, "<invalid>");
}
/* Muestra todas las variables con su valor actual, solo a la consola. */
void watch_list(void) {
    for (int i = 0; i < watch_count; i++) {
        char text[MAX_TEXT];
        read_value(&watches[i], text);
        debug_send("%s = %s\n", watches[i].name, text);
    }
}
/* Compara cada variable con su último valor y muestra las que cambiaron. where dice en qué punto
   estamos: el nombre del método, o el texto que quieras si la llamas desde tu código. */
void watch_poll(const char *where) {
    if (!watch_count) return;
    poll_number++;
    const char *tail = strrchr(where, '_');
    const char *label = tail && tail[1] ? tail + 1 : where;
    for (int i = 0; i < watch_count; i++) {
        Watch *w = &watches[i];
        char text[MAX_TEXT];
        read_value(w, text);
        if (!w->has_value) debug_report("[watch] #%u %s: %s = %s\n", poll_number, label, w->name, text);
        else if (strcmp(text, w->last))
            debug_report("[watch] #%u %s: %s %s -> %s\n", poll_number, label, w->name, w->last, text);
        else continue;
        memcpy(w->last, text, MAX_TEXT);
        w->has_value = 1;
    }
}
