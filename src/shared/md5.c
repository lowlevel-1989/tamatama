/* shared/md5.c: el MD5 de un fichero (RFC 1321), sin programas ni libraries
   externas. Solo sirve para reconocer la versión del .apk: no es para
   seguridad.

   MD5 trabaja con bloques de 64 bytes. Cada bloque se mezcla con un estado de
   cuatro palabras de 32 bits (a, b, c, d) en 64 pasos. Al final se añade un 1,
   ceros y la longitud en bits, para que dos ficheros distintos no acaben con el
   mismo último bloque. Todo es little-endian. */
/* Offsets de 64 bits en readdir y fopen también en ARM de 32 bits: si no, bajo
   QEMU readdir falla con las carpetas de ext4 y no se abren ficheros de más de
   2 GiB. */
#define _FILE_OFFSET_BITS 64
#include "md5.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    uint32_t state[4]; /* a, b, c, d */
    uint64_t bytes;    /* cuántos bytes se han leído en total */
    uint8_t block[64]; /* el bloque que se está llenando */
} Md5;

/* Cuánto gira cada paso: cuatro valores por ronda, que se repiten. */
static const uint8_t shift[64] = {
    7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
    5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20,
    4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
    6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21};

/* Las constantes de cada paso: la parte entera de |sin(i + 1)| * 2^32. */
static const uint32_t table[64] = {
    0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
    0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
    0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
    0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
    0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
    0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
    0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
    0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391};

/* Mezcla un bloque de 64 bytes en el estado. */
static void md5_block(Md5 *m, const uint8_t *p) {
    uint32_t w[16];
    for (int i = 0; i < 16; i++)
        w[i] = (uint32_t)p[i * 4] | (uint32_t)p[i * 4 + 1] << 8 | (uint32_t)p[i * 4 + 2] << 16 |
               (uint32_t)p[i * 4 + 3] << 24;
    uint32_t a = m->state[0], b = m->state[1], c = m->state[2], d = m->state[3];
    for (int i = 0; i < 64; i++) {
        uint32_t f;
        int g;
        /* Cuatro rondas de 16 pasos: cada una con su función y su orden de
           palabras. */
        if (i < 16) f = (b & c) | (~b & d), g = i;
        else if (i < 32) f = (d & b) | (~d & c), g = (5 * i + 1) & 15;
        else if (i < 48) f = b ^ c ^ d, g = (3 * i + 5) & 15;
        else f = c ^ (b | ~d), g = (7 * i) & 15;
        f += a + table[i] + w[g];
        a = d;
        d = c;
        c = b;
        b += f << shift[i] | f >> (32 - shift[i]);
    }
    m->state[0] += a;
    m->state[1] += b;
    m->state[2] += c;
    m->state[3] += d;
}

static void md5_init(Md5 *m) {
    m->state[0] = 0x67452301;
    m->state[1] = 0xefcdab89;
    m->state[2] = 0x98badcfe;
    m->state[3] = 0x10325476;
    m->bytes = 0;
}

/* Añade n bytes: mezcla cada bloque en cuanto está completo. */
static void md5_update(Md5 *m, const uint8_t *data, size_t n) {
    size_t used = (size_t)(m->bytes & 63);
    m->bytes += n;
    while (n) {
        size_t take = 64 - used < n ? 64 - used : n;
        memcpy(m->block + used, data, take);
        used += take;
        data += take;
        n -= take;
        if (used == 64) md5_block(m, m->block), used = 0;
    }
}

/* El relleno final: un byte 0x80, ceros hasta dejar 8 bytes libres y la
   longitud en bits. */
static void md5_final(Md5 *m, uint8_t digest[16]) {
    uint64_t bits = m->bytes * 8;
    uint8_t pad[72] = {0x80}, length[8];
    size_t used = (size_t)(m->bytes & 63);
    md5_update(m, pad, used < 56 ? 56 - used : 120 - used);
    for (int i = 0; i < 8; i++) length[i] = (uint8_t)(bits >> (8 * i));
    md5_update(m, length, 8);
    for (int i = 0; i < 16; i++) digest[i] = (uint8_t)(m->state[i / 4] >> (8 * (i % 4)));
}

int md5_file(const char *path, char hex[33]) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    Md5 m;
    md5_init(&m);
    static uint8_t chunk[65536]; /* por bloques: el .apk no tiene que caber en
                                    memoria */
    size_t got;
    while ((got = fread(chunk, 1, sizeof chunk, f)) > 0) md5_update(&m, chunk, got);
    int failed = ferror(f);
    fclose(f);
    if (failed) return -1;
    uint8_t digest[16];
    md5_final(&m, digest);
    for (int i = 0; i < 16; i++) snprintf(hex + i * 2, 3, "%02x", digest[i]);
    return 0;
}
