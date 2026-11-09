/* shared/common.h: todo lo que comparten el build ARM y el de 64 bits. */
#pragma once
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "services.h" /* log_msg, die, clock_ns, read_file y los assets */

#ifdef __cplusplus
extern "C" {
#endif

/* Un pointer del juego mide siempre 32 bits, aunque compilemos para 64. 
 * Por eso los objetos del juego guardan arm_ptr y no pointers de C: 
 * así tienen la misma forma en memoria en las dos máquinas. */
typedef uint32_t arm_ptr;

/* Los registers que tenía el código ARM al entrar en una función hook. */
typedef struct Regs {
    uint32_t r[4]; /* de r0 a r3: los cuatro primeros argumentos */
    uint32_t sp;   /* stack pointer */
} Regs;

/* Toda función nativa lee sus argumentos de Regs y escribe el resultado en r[0]
   y, 
 * si mide 64 bits, también en r[1]. */
typedef void (*NativeFunction)(Regs *r);
typedef struct {
    const char *name;
    NativeFunction fn;
    /* solo para una función interna sin símbolo: su posición en el .so, con el
       bit 0 a 1 si es Thumb */
    uint32_t address;
} NativeEntry;

/* La tabla final: une la lista (port/port_table.h) y la de GL
   (backend/gl_table.h). La construye shared/native_table.c. Una función que
   aparece aquí 
 * se hace auto hook: desde ese momento el juego la funcion hook. */
extern const NativeEntry native_table[];

/* 1 si se compiló con make NATIVE=1: no se carga ningún .so, todo el juego es
   código nativo.
   Es el final de la migración. El .apk sigue haciendo falta, por sus
   recursos. */
#ifndef LOADER_NATIVE
#define LOADER_NATIVE 0
#endif

/* La arena: una sola región de memoria con la imagen del juego, su head y su
   stack.
 * heap      lo que se pide con malloc o new: dura hasta free o delete.
 * 0x60000000 no tiene nada de especial: es solo una zona libre tanto en una
 * Raspberry Pi como bajo QEMU. QEMU coloca su propio stack hacia 0x40000000 (8
 * MiB), y Linux en ARM coloca sus mappings cerca de 0xB6000000, hacia abajo.
 * Además, un número redondo facilita leer las direcciones. Es una preferencia
 * que se pide a mmap sin MAP_FIXED: si el sistema entrega otra dirección, el
 * programa funciona igual con ella. */
#define ARENA_ADDRESS 0x60000000u
/* 256 MiB: las páginas solo gastan memoria física cuando se utilizan */
#define ARENA_BYTES   0x10000000u

/* Convierte un pointer del juego en uno que este build puede usar (to_local), y
   al revés (to_arm). */
#if defined(__arm__)
/* el mismo proceso: no hay nada que convertir */
static inline void *to_local(arm_ptr g) { return (void *)g; }
static inline arm_ptr to_arm(const void *p) { return (arm_ptr)p; }
#else
extern uint8_t *arena_local;   /* dónde ve la arena este proceso; la elige
                                  Linux */
extern arm_ptr arena_arm;      /* dónde ve loader-arm esa misma arena */

/* Posición dentro de la arena más la dirección local. */
static inline void *to_local(arm_ptr g) { return arena_local + (g - arena_arm); }

/* Posición dentro de la arena más la dirección que ve el juego. */
static inline arm_ptr to_arm(const void *p) {
    return (arm_ptr)((const uint8_t *)p - arena_local) + arena_arm;
}
#endif

/* softfp: un float no es más que 32 bits dentro de un register entero. 
 * Hay que copiar los bits, no convertir el número. */
static inline float bits_to_float(uint32_t b) {
    float f;
    __builtin_memcpy(&f, &b, 4);
    return f;
}
/* El camino contrario: de float a los bits que van en el register. */
static inline uint32_t float_to_bits(float f) {
    uint32_t b;
    __builtin_memcpy(&b, &f, 4);
    return b;
}

/* Lee la palabra i de los argumentos pasados por el stack. */
static inline uint32_t stack_arg(const Regs *r, int i) { return ((uint32_t *)to_local(r->sp))[i]; }

/* Servicios para las funciones nativas y para el programa principal. Funcionan
   igual en las dos builds. */
/* ejecuta código ARM en una dirección */
uint64_t call_arm_at(arm_ptr target, const uint32_t *words, int n);
/* por nombre, nativa o ARM */
uint64_t call_method(const char *name, const uint32_t *words, int n);
/* la parte ARM de call_method */
uint64_t call_arm_by_name(const char *name, const uint32_t *words, int n);
/* lo que tienen las vtables y la PLT para este nombre; 0 si no existe */
arm_ptr arm_symbol(const char *name);
arm_ptr shared_alloc(uint32_t n);        /* memoria visible desde los dos
                                            lados */
arm_ptr arm_jni_env(void);                /* el JNIEnv* que se pasa a los
                                             métodos JNI */
arm_ptr new_jstring(const char *text); /* crea un jstring para pasarlo como
                                          argumento */
int app_main(void);                /* el propio programa: lo escribes tú, en
                                      port/ */

/* Reglas opcionales, solo para excepciones. Sin fichero de reglas, decide
   native_table. */
enum { RULE_ARM, RULE_STUB, RULE_EXIT };
typedef struct Rule {
    const char *name;
    int mode;
    uint32_t ret;     /* stub: valor que devuelve */
    uint32_t address; /* posición en el .so, para funciones sin nombre */
} Rule;
/* lee el fichero de reglas */
void rules_load(const char *path);
/* una línea trap de las reglas: arm/trap.c */
void trap_add(const char *name, const char *spec);
const Rule *rule_find(const char *name); /* la regla de este nombre, o NULL */
const char *symbol_place(const char *name); /* el lugar de un nombre de symbol,
                                               o NULL */

/* la regla número i, o NULL si no existe */
const Rule *rule_at(int i);
/* cuántas reglas hay */
int rule_count(void);
/* 1 si se pidió con --trace o con trace on en el fichero de reglas */
int trace_enabled(void);
void trace_enable(void);
int native_index(const char *name);  /* posición en native_table, o -1 */
int native_active(const char *name); /* posición, o -1 si no está o está forzada
                                        a ARM */
int is_hooked(const char *name);     /* 1 si el juego no debe ejecutar su propio
                                        código */
uint32_t parse_number(const char *c); /* un número en decimal o 0x... */
char *next_word(char *p);            /* termina la palabra en p y devuelve la
                                        siguiente */

/* Matemáticas propias: dan los mismos bits en ARM y en x86_64. */
double math_sin(double x);
double math_cos(double x);

/* con un .apk: lo comprueba, lo extrae en resources/ y devuelve su .so; si no,
   la ruta tal cual.
   Con NULL busca el juego: lo ya extraído o un .apk en apk/ */
const char *apk_library(const char *path);
const char *apk_resources(void); /* resources/{md5} del .apk en uso, con assets/
                                    dentro */
const char *apk_rules(void);     /* rules/{md5}.txt de la versión en uso si
                                    existe, o NULL */
/* --help */
void usage(const char *program, const char *options, const char *environment);
/* --rules FILE o --rules=FILE */
const char *option_rules(int argc, char **argv, int *i);
/* --debug [pause]: 0, 1 o 2 */
int option_debug(int argc, char **argv, int *i);

/* Cómo representa la JVM falsa una cadena o un array. Un jstring es un pointer
   a esto. */
typedef struct JniObject {
    uint32_t len;
    uint32_t dirty;
    uint8_t data[];
} JniObject;

#ifdef __cplusplus
}
#endif
