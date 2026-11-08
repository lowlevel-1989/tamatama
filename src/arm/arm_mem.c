/* arm/arm_mem.c: la memoria del programa ARM. libc se ocupa de lo suyo; este
   fichero se ocupa de lo que ve el juego: las direcciones fijas, la arena y el
   heap del juego. */
#include "arm.h"
#include "bridge.h"
#include <malloc.h>

/* Escribe todo el buffer, aunque el kernel lo acepte por trozos. */
void write_all(int fd, const void *p, size_t n) {
    const char *c = p;
    while (n) {
        ssize_t w = write(fd, c, n);
        if (w <= 0) _exit(3);
        c += w;
        n -= (size_t)w;
    }
}
/* Lee exactamente n bytes. Si el otro extremo cerró, el programa ha
   terminado. */
void read_all(int fd, void *p, size_t n) {
    char *c = p;
    while (n) {
        ssize_t r = read(fd, c, n);
        if (r <= 0) exit(0);
        c += r;
        n -= (size_t)r;
    }
}
/* Memoria nueva en una dirección elegida, para que los mensajes y las
   direcciones se repitan en cada ejecución. La dirección es una petición, no
   una orden, porque no se usa MAP_FIXED: si está ocupada, recibimos otra y todo
   sigue funcionando. */
void *map_at(uint32_t address, size_t n, int prot) {
    void *r = mmap((void *)address, n, prot, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (r == MAP_FAILED) die("mmap failed");
    if ((uint32_t)r != address) log_msg("[loader] wanted 0x%x, got %p\n", address, r);
    return r;
}
/* La CPU tiene caches separadas para datos e instrucciones. Puede que el código
   que acabamos de escribir no se vea hasta que se lo pidamos. QEMU no lo
   necesita, pero un ARM real sí. */
void flush_code(void *start, size_t n) {
    __builtin___clear_cache((char *)start, (char *)start + n);
}

/* La arena: aquí vive todo lo que el loader debe poder leer. Nunca se devuelve
   al sistema. */
/* Dónde empieza la arena y cuánto se ha entregado ya. */
static uint8_t *arena_base;
static uint32_t arena_used;

/* Pide la arena en 0x60000000, sin MAP_FIXED: si esa dirección está ocupada, se
   usa la que entregue el sistema.
   Con shared = 1 la mapea desde el descriptor que dejó abierto el loader, para
   que los dos procesos vean las mismas páginas. Con shared = 0 es memoria
   normal del proceso. */
void arena_init(int shared) {
    void *r = shared ? mmap((void *)ARENA_ADDRESS, ARENA_BYTES, PROT_READ | PROT_WRITE,
                            MAP_SHARED, FD_ARENA, 0)
                     : mmap((void *)ARENA_ADDRESS, ARENA_BYTES, PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (r == MAP_FAILED) die("cannot reserve the arena");
    if ((uint32_t)r != ARENA_ADDRESS) log_msg("[loader] arena moved to %p\n", r);
    arena_base = r;
}
/* Entrega el siguiente trozo libre, avanzando siempre. align debe ser una
   potencia de 2. */
void *arena_take(uint32_t n, uint32_t align) {
    uint32_t offset = (arena_used + align - 1) & ~(align - 1);
    if (offset + n > ARENA_BYTES) die("arena exhausted");
    arena_used = offset + n;
    /* La primera palabra de la arena guarda esta cuenta, para que la consola de
       debug sepa hasta dónde buscar desde el otro proceso. Es el fondo del
       stack, que nunca llega a usarse. */
    *(uint32_t *)arena_base = arena_used;
    return arena_base + offset;
}
/* Dirección real de la arena. El loader la recibe en el primer mensaje. */
arm_ptr arena_address(void) { return (arm_ptr)arena_base; }
/* Memoria visible desde los dos lados. En este build es el heap del juego. */
arm_ptr shared_alloc(uint32_t n) { return (arm_ptr)arena_malloc(n); }

/* El heap del juego cuando un loader debe leerlo: tamaños en potencias de 2 y
   una lista libre por tamaño. Cada bloque empieza con dos palabras: su clase de
   tamaño (0: aligned, nunca se reutiliza) y su tamaño. */
#define SIZE_CLASSES 28
static void *free_lists[SIZE_CLASSES];

/* 1 si p está dentro de la arena. Decide a quién hay que devolver un bloque. */
static int in_arena(const void *p) {
    uintptr_t a = (uintptr_t)p, b = (uintptr_t)arena_base;
    return a >= b && a - b < ARENA_BYTES;
}
/* Toma un bloque de la lista de su tamaño o, si está vacía, uno nuevo de la
   arena. Los 8 bytes de header mantienen el alignment a 8. */
static void *block_alloc(size_t n) {
    int c = 4;
    while (((size_t)1 << c) < n + 8) c++;
    uint32_t *header;
    if (c < SIZE_CLASSES && free_lists[c]) {
        header = free_lists[c];
        free_lists[c] = *(void **)(header + 2);
    } else header = arena_take((uint32_t)1 << c, 8);
    header[0] = (uint32_t)c;
    header[1] = (uint32_t)n;
    return header + 2;
}
/* Devuelve el bloque a su lista. Los bloques aligned no se reutilizan. */
static void block_free(void *p) {
    uint32_t *header = (uint32_t *)p - 2;
    int c = (int)header[0];
    if (!c) return;
    *(void **)(header + 2) = free_lists[c];
    free_lists[c] = header;
}

/* Todo lo que reserva el juego pasa por aquí. Sin loader no hay nada que
   compartir, así que lo hace libc y no la arena. */
void *arena_malloc(size_t n) { return has_host ? block_alloc(n) : malloc(n); }
/* Libera un bloque, venga de la arena o de libc. */
void arena_free(void *p) {
    if (!p) return;
    if (in_arena(p)) block_free(p); /* lo que entregó la arena */
    else free(p);                   /* lo que entregó libc, como el resultado de
                                       asprintf */
}
/* Como malloc, con la memoria a cero. */
void *arena_calloc(size_t count, size_t n) {
    void *p = arena_malloc(count * n);
    if (p) memset(p, 0, count * n);
    return p;
}
/* Cambia el tamaño de un bloque. Si cabe en el que ya tiene, no se mueve. */
void *arena_realloc(void *p, size_t n) {
    if (!p) return arena_malloc(n);
    if (!in_arena(p)) return realloc(p, n);
    uint32_t *header = (uint32_t *)p - 2;
    size_t capacity = header[0] ? ((size_t)1 << header[0]) - 8 : header[1];
    if (n <= capacity) {
        header[1] = (uint32_t)n;
        return p;
    }
    void *q = arena_malloc(n);
    memcpy(q, p, header[1]);
    arena_free(p);
    return q;
}
/* Memoria aligned. En la arena reserva de más y coloca el header justo delante
   del resultado. */
void *arena_memalign(size_t align, size_t n) {
    if (!has_host) return memalign(align, n);
    uintptr_t raw = (uintptr_t)arena_take((uint32_t)(n + align + 8), 8);
    uint32_t *p = (uint32_t *)((raw + 8 + align - 1) & ~(align - 1));
    p[-2] = 0;
    p[-1] = (uint32_t)n;
    return p;
}
/* El tamaño pedido de un bloque de la arena o, en libc, su tamaño
   utilizable. */
size_t arena_usable_size(void *p) {
    if (!p) return 0;
    if (!in_arena(p)) return malloc_usable_size(p);
    return ((uint32_t *)p)[-1];
}
