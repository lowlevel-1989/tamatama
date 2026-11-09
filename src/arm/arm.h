/* arm/arm.h: declaraciones que solo usa el build ARM. */
#pragma once
#include "common.h"
#include <sys/mman.h>
#include <unistd.h>

/* Dónde está la maquinaria de los hooks: justo después de la arena, es decir,
   en 0x60000000 + 256 MiB = 0x70000000. No hay otra razón: es la primera
   dirección fuera de la arena, y map_at acepta otra si esa está ocupada. El
   código va en sus propias páginas, para que escribir en los datos de un hook
   no obligue a QEMU a tirar su traducción de ese código. */
#define HOOK_CODE_ADDRESS (ARENA_ADDRESS + ARENA_BYTES)
/* 4096 hooks de 16 bytes = 0x10000: los datos empiezan en 0x70010000 */
#define HOOK_DATA_ADDRESS (HOOK_CODE_ADDRESS + 0x10000u)

/* arm_mem.c */
/* Escriben y leen exactamente n bytes de un descriptor. */
void write_all(int fd, const void *p, size_t n);
void read_all(int fd, void *p, size_t n);
/* memoria nueva en una dirección pedida */
void *map_at(uint32_t address, size_t n, int prot);
/* hace visible para la CPU el código recién escrito */
void flush_code(void *start, size_t n);
/* reserva la arena; shared = 1 la toma del descriptor del loader */
void arena_init(int shared);
/* el siguiente trozo libre de la arena */
void *arena_take(uint32_t n, uint32_t align);
/* dónde quedó la arena */
arm_ptr arena_address(void);
/* El heap del juego: en la arena si otro proceso debe leerlo, y en libc si
   no. */
void *arena_malloc(size_t n);
void arena_free(void *p);
void *arena_calloc(size_t count, size_t n);
void *arena_realloc(void *p, size_t n);
void *arena_memalign(size_t align, size_t n);
size_t arena_usable_size(void *p);

/* Lo que context_switch guarda de un thread: lo que AAPCS obliga a conservar.
   El orden lo fija asm/context_switch.s. */
typedef struct Context {
    uint32_t r4_r11[8];
    uint32_t sp;
    uint32_t lr;          /* adónde vuelve: la primera vez, donde empieza el
                             thread */
    uint64_t d8_d15[8];
} Context;

/* asm/call_words.s, asm/switch_stack.s y asm/context_switch.s */
/* llama a fn con n argumentos, según AAPCS */
uint64_t call_words(void *fn, const uint32_t *words, uint32_t n);
/* sigue en otro stack y no vuelve */
void switch_stack(void *sp, void (*fn)(void *), void *arg);
/* guarda el contexto actual en save y sigue en load */
void context_switch(Context *save, const Context *load);
/* threads.c: los threads del juego, por turnos */
/* 0, o -1 si no se pudo */
int thread_create(uint32_t function, uint32_t argument);
/* cede el turno; nunca vuelve antes */
int thread_usleep(uint32_t microseconds);
/* para un método de Java */
uint64_t java_thread_create(const uint32_t *words);
void thread_call_begin(void); /* antes de una llamada al código ARM */
void thread_call_end(void);   /* después: si terminó una de app_main, los
                                 threads tienen su ronda */
/* elf.c */
uint32_t elf_load(const char *path); /* devuelve la dirección de carga */
void elf_run_constructors(void);     /* DT_INIT y DT_INIT_ARRAY */
void *so_find(const char *name);     /* un símbolo definido en el .so */
void *elf_find_exidx(uint32_t pc, int *count); /* la tabla de excepciones del
                                                  .so */
/* 1 si está dentro del .so */
int elf_position(uint32_t address, uint32_t *position);
/* el nombre de su función, o NULL */
const char *elf_function_at(uint32_t position, uint32_t *start);
/* con su posición y su función */
void elf_format_address(uint32_t address, char *text, size_t room);
/* callers.c */
void callers_enable(void);
int callers_enabled(void);
/* "Clase::metodo+0x7a" o "?" */
void describe_position(uint32_t position, char *text, size_t room);
void print_callers(const uint32_t *stack, const char *method);
/* hooks.c */
typedef struct Hook Hook;
/* devuelve el código del hook */
void *hook_with_target(const char *name, void *target);
/* para funciones del .so */
void *hook_without_target(const char *name);
/* lo que recibe una importación del .so */
void *resolve_import(const char *name);
/* ejecuta una función nativa, esté donde esté: r son r0-r3, y recibe el
   resultado en r0 y r1 */
void run_native(int index, uint32_t *r, uint32_t sp);
/* imprime el resumen final */
void hooks_report(void);
/* trap.c: en cada safe point, quién escribió en las variables con trap */
void trap_check(void);
/* jvm.c: los métodos de Java sin native que ha llamado el juego */
void java_report(void);
/* 1: las funciones nativas están en otro proceso */
extern int has_host;
/* atiende las peticiones del loader */
void serve_host_requests(int until_native_done, uint32_t *regs);
/* bionic.c, bionic_pthread.c y jvm.c */
/* Una entrada de las tablas de bionic: nombre, dirección y tipo. */
typedef struct {
    const char *name;
    const void *address;
    int is_data; /* 1: el juego lee esta dirección como dato, así que no debe
                    recibir un hook */
} BionicEntry;
/* la tabla de bionic_pthread.c */
extern const BionicEntry pthread_entries[];
extern const unsigned pthread_entry_count;
/* prepara las tablas de caracteres */
void bionic_init(void);
/* busca un nombre en las tablas */
const void *bionic_find(const char *name, int *is_data);
/* crea el JNIEnv si hace falta y lo devuelve */
void *jni_env(void);
/* lo mismo con el JavaVM */
void *jni_vm(void);
/* la entrada de un método registrado con RegisterNatives */
void *jni_registered(const char *name);
/* los nombres de los argumentos de una función, o NULL si no se conocen */
const char *const *known_args(const char *name);
