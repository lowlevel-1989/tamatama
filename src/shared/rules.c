/* shared/rules.c: 
 * la decisión de quién ejecuta cada función. 
 * Decide la tabla: una función con versión nativa va a ella, 
 * y lo demás sigue siendo ARM. 
 *
 * Un fichero opcional añade excepciones:
     trace on|off                  registra cada llamada
     arm NOMBRE                    mantiene el ARM, aunque exista versión nativa
     stub NOMBRE [@0xDIR] [valor]  no ejecuta nada y devuelve valor
     exit NOMBRE [@0xDIR]          termina el programa en cuanto la llaman, con el informe final
     watch NOMBRE LUGAR [TIPO]     sigue una variable: lo explica debug/watch.c
     symbol NOMBRE LUGAR           un nombre para un lugar que el .so no exporta
   @0xDIR es la posición dentro del .so, para funciones sin símbolo. */

#include "debug.h"
#define MAX_RULES 512

/* Las reglas leídas, y cuántas hay. */
static Rule rules[MAX_RULES];
static int count, trace_on;

/* Los nombres que defines tú: symbol g_volumen @0xC2D4C. */
#define MAX_SYMBOLS 256
static struct {
    const char *name, *place;
} symbols[MAX_SYMBOLS];
static int symbol_count;

/* Interpreta un número, en decimal o como 0x... */
uint32_t parse_number(const char *c) {
    uint32_t v = 0, base = 10;
    if (c[0] == '0' && c[1] == 'x') {
        base = 16;
        c += 2;
    }
    for (;; c++) {
        uint32_t d = *c >= '0' && *c <= '9'   ? (uint32_t)(*c - '0')
                     : *c >= 'a' && *c <= 'f' ? (uint32_t)(*c - 'a' + 10)
                     : *c >= 'A' && *c <= 'F' ? (uint32_t)(*c - 'A' + 10)
                                              : 99;
        if (d >= base) return v;
        v = v * base + d;
    }
}
/* Termina la palabra en p con un 0 y devuelve el inicio de la siguiente. */
char *next_word(char *p) {
    while (*p && *p != ' ') p++;
    if (!*p) return p;
    *p++ = 0;
    while (*p == ' ') p++;
    return p;
}
/* Añade un nombre. El lugar tiene que ser una posición fija: con -> su valor cambiaría durante el
   juego, y con otro nombre podrían apuntarse el uno al otro para siempre. */
static void symbol_add(const char *name, char *place) {
    next_word(place);
    int fixed = place[0] == '@' || place[0] == '#' || (place[0] == '0' && place[1] == 'x');
    if (!*name || !fixed || strstr(place, "->") || name[strcspn(name, "+-")])
        die("symbol NAME PLACE: PLACE must be @0x..., #0x... or 0x..., with no ->");
    if (symbol_count == MAX_SYMBOLS) die("too many symbols");
    symbols[symbol_count].name = name;
    symbols[symbol_count++].place = place;
}
/* El lugar de un nombre definido con symbol, o NULL si no existe. */
const char *symbol_place(const char *name) {
    for (int i = 0; i < symbol_count; i++)
        if (!strcmp(symbols[i].name, name)) return symbols[i].place;
    return 0;
}
/* Lee el fichero línea a línea. Los nombres apuntan dentro del buffer, que nunca se libera. */
void rules_load(const char *path) {
    char *b = read_file(path);
    if (!b) die("cannot read the rules file");
    for (char *line = b; line && *line;) {
        char *end = strchr(line, '\n');
        if (end) *end = 0;
        if (*line && *line != '#') {
            char *name = next_word(line);
            char *option = next_word(name);
            if (!strcmp(line, "trace")) trace_on = !strcmp(name, "on");
            else if (!strcmp(line, "watch")) watch_add(name, option);
            else if (!strcmp(line, "symbol")) symbol_add(name, option);
            else if (!strcmp(line, "pause")) debug_pause_at(name);
            else if (count == MAX_RULES) die("too many rules");
            else {
                Rule *r = &rules[count++];
                r->name = name;
                r->mode = !strcmp(line, "arm") ? RULE_ARM : !strcmp(line, "exit") ? RULE_EXIT : RULE_STUB;
                while (*option && *option != '#') {
                    char *next = next_word(option);
                    if (*option == '@') r->address = parse_number(option + 1);
                    else r->ret = parse_number(option);
                    option = next;
                }
            }
        }
        line = end ? end + 1 : 0;
    }
}
/* Busca la regla de un nombre. */
const Rule *rule_find(const char *n) {
    for (int i = 0; i < count; i++)
        if (!strcmp(rules[i].name, n)) return &rules[i];
    return 0;
}
/* La regla número i, o 0 si no existe. */
const Rule *rule_at(int i) { return i < count ? &rules[i] : 0; }
/* Cuántas reglas hay. */
int rule_count(void) { return count; }
/* 1 si se pidió con --trace o con trace on en el fichero. */
int trace_enabled(void) { return trace_on; }
/* La opción --trace. */
void trace_enable(void) { trace_on = 1; }
/* Busca un nombre en native_table. */
int native_index(const char *n) {
    for (int i = 0; native_table[i].name; i++)
        if (!strcmp(native_table[i].name, n)) return i;
    return -1;
}
/* Toda la decisión: implementada en nativo y no forzada a otra cosa por una regla. */
int native_active(const char *n) { return rule_find(n) ? -1 : native_index(n); }

/* 1 si el juego no debe ejecutar su propio código: con una regla stub, o con versión nativa y
   sin una regla que lo impida. */
int is_hooked(const char *n) {
    const Rule *r = rule_find(n);
    return r ? r->mode != RULE_ARM : native_index(n) >= 0;
}

/* Seno y coseno con una serie de Taylor. Usar el mismo código en los dos lados garantiza los
   mismos bits, lo que permite comparar imágenes enteras. Cámbialas por las de libm cuando ya no
   compares con el original. reduce lleva x al intervalo de -pi a pi. */
static double reduce(double x) {
    const double two_pi = 6.283185307179586;
    int k = (int)(x / two_pi);
    x -= (double)k * two_pi;
    if (x > 3.141592653589793) x -= two_pi;
    if (x < -3.141592653589793) x += two_pi;
    return x;
}

/* Serie de Taylor con 10 términos. Cada término sale del anterior, sin calcular potencias. */
double math_sin(double x) {
    x = reduce(x);
    double t = x, r = x;
    for (int i = 1; i < 10; i++) {
        t *= -x * x / (double)((2 * i) * (2 * i + 1));
        r += t;
    }
    return r;
}
/* cos x = sen(x + pi / 2). */
double math_cos(double x) { return math_sin(x + 1.5707963267948966); }
