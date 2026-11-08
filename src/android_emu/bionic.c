/* android_emu/bionic.c: lo que el juego espera de bionic, la libc de Android. Lo que se comporta
   igual va directamente a la libc de este sistema, y lo que difiere pasa por un pequeño
   wrapper. Estas funciones corren dentro del proceso ARM: cruzar a x86_64 por un strlen no
   tendría sentido. */
/* el juego se compiló con time_t y off_t de 32 bits; debe ir antes de cualquier #include */
#undef _TIME_BITS
#undef _FILE_OFFSET_BITS
#include "arm.h"
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <locale.h>
#include <math.h>
#include <sched.h>
#include <poll.h>
#include <signal.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/uio.h>
#include <time.h>
#include <wchar.h>
#include <wctype.h>

/* El .so pasa los float en registers enteros. Si este sistema es hard-float, una función que
   recibe o devuelve un float debe indicarlo, o buscaría sus argumentos en s0, d0... */
#define SOFTFP __attribute__((pcs("aapcs")))

/* ---------- Datos que el juego lee directamente: aquí un hook no serviría ---------- */

/* Protección del stack. Sirve cualquier valor mientras no cambie. Este lleva los bytes 00 (fin de
   cadena), FF (EOF), 0D y 0A (saltos de línea). Las funciones de texto se detienen en ellos,
   así que un overflow rara vez lo copia entero. No es una dirección. */
static uintptr_t bionic_stack_chk_guard = 0x0A0DFF00u;
/* El juego la llama si detecta el stack corrupto. No debe volver. */
static void bionic_stack_chk_fail(void) { die("stack canary destroyed"); }

/* stdin, stdout y stderr: el juego los calcula como &__sF[0], &__sF[1] y &__sF[2], y un FILE
   mide 84 bytes en Android. real_stream() convierte esas direcciones en los flujos reales. */
#define BIONIC_FILE_SIZE 84
static uint8_t fake_stdio[3 * BIONIC_FILE_SIZE];
/* Convierte un FILE* del juego en el flujo real. */
static FILE *real_stream(FILE *f) {
    uint8_t *p = (uint8_t *)f;
    if (p == fake_stdio) return stdin;
    if (p == fake_stdio + BIONIC_FILE_SIZE) return stdout;
    if (p == fake_stdio + 2 * BIONIC_FILE_SIZE) return stderr;
    return f;
}

/* En Android 4.4, isalpha() y compañía son código en línea que lee esta tabla. Empieza una
   posición antes, para que EOF (-1) sea un índice válido. */
/* Los bits de _ctype_: mayúscula, minúscula, número, espacio, puntuación, control, hexadecimal
   y blanco. */
enum { CT_U = 0x01, CT_L = 0x02, CT_N = 0x04, CT_S = 0x08, CT_P = 0x10, CT_C = 0x20, CT_X = 0x40, CT_B = 0x80 };
/* Cada tabla tiene 257 entradas: EOF y los 256 valores de un byte. */
static char bionic_ctype[257];
static short tolower_table[257], toupper_table[257];
static const short *bionic_tolower_tab = tolower_table, *bionic_toupper_tab = toupper_table;
/* __isthreaded: las macros de stdio lo consultan. */
static int bionic_isthreaded = 1;

/* Rellena las tablas de caracteres, con los valores de la configuración regional C. Se llama
   antes de cargar el .so. */
void bionic_init(void) {
    for (int c = 0; c < 256; c++) {
        int v = 0;
        if (c < 128) {
            if (isupper(c)) v |= CT_U;
            if (islower(c)) v |= CT_L;
            if (isdigit(c)) v |= CT_N;
            if (isspace(c)) v |= CT_S;
            if (ispunct(c)) v |= CT_P;
            if (iscntrl(c)) v |= CT_C;
            if (isxdigit(c)) v |= CT_X;
            if (c == ' ') v |= CT_B;
        }
        bionic_ctype[c + 1] = (char)v;
        tolower_table[c + 1] = (short)(c < 128 ? tolower(c) : c);
        toupper_table[c + 1] = (short)(c < 128 ? toupper(c) : c);
    }
    tolower_table[0] = toupper_table[0] = -1;
}

/* ---------- Memoria ---------- */

/* Memoria aligned, sacada del heap del juego. */
static void *bionic_memalign(size_t align, size_t n) { return arena_memalign(align, n); }
static int bionic_posix_memalign(void **out, size_t align, size_t n) {
    *out = arena_memalign(align, n);
    return *out ? 0 : ENOMEM;
}
static void *bionic_valloc(size_t n) { return arena_memalign(4096, n); }
/* Estas reservan memoria, así que lo que devuelven debe venir del heap del juego. */
static char *bionic_strdup(const char *s) {
    size_t n = strlen(s) + 1;
    char *d = arena_malloc(n);
    if (d) memcpy(d, s, n);
    return d;
}
/* Como strdup, con un máximo de bytes. */
static char *bionic_strndup(const char *s, size_t max) {
    size_t n = strnlen(s, max);
    char *d = arena_malloc(n + 1);
    if (d) {
        memcpy(d, s, n);
        d[n] = 0;
    }
    return d;
}

/* ---------- Texto ---------- */

/* glibc solo las tiene desde la versión 2.38. */
static size_t bionic_strlcpy(char *d, const char *s, size_t size) {
    size_t n = strlen(s);
    if (size) {
        size_t c = n < size - 1 ? n : size - 1;
        memcpy(d, s, c);
        d[c] = 0;
    }
    return n;
}
/* Añade a d sin pasar de size bytes en total, y devuelve la longitud que habría tenido. */
static size_t bionic_strlcat(char *d, const char *s, size_t size) {
    size_t used = strnlen(d, size);
    if (used == size) return size + strlen(s);
    return used + bionic_strlcpy(d + used, s, size - used);
}
/* En glibc con _GNU_SOURCE devuelve un char *. En Android, un int. */
static int bionic_strerror_r(int e, char *buf, size_t n) {
    bionic_strlcpy(buf, strerror(e), n);
    return 0;
}

/* Las variantes _chk (FORTIFY): la misma función con un argumento más, el tamaño real del
   buffer, que aquí se ignora. */
static void *bionic_memcpy_chk(void *d, const void *s, size_t n, size_t dl) { (void)dl; return memcpy(d, s, n); }
static void *bionic_memmove_chk(void *d, const void *s, size_t n, size_t dl) { (void)dl; return memmove(d, s, n); }
static void *bionic_memset_chk(void *d, int c, size_t n, size_t dl) { (void)dl; return memset(d, c, n); }
static char *bionic_strcpy_chk(char *d, const char *s, size_t dl) { (void)dl; return strcpy(d, s); }
static char *bionic_strncpy_chk(char *d, const char *s, size_t n, size_t dl) { (void)dl; return strncpy(d, s, n); }
static char *bionic_strcat_chk(char *d, const char *s, size_t dl) { (void)dl; return strcat(d, s); }
static char *bionic_strncat_chk(char *d, const char *s, size_t n, size_t dl) { (void)dl; return strncat(d, s, n); }
static size_t bionic_strlen_chk(const char *s, size_t max) { (void)max; return strlen(s); }
static char *bionic_strchr_chk(const char *s, int c, size_t max) { (void)max; return strchr(s, c); }
static char *bionic_strrchr_chk(const char *s, int c, size_t max) { (void)max; return strrchr(s, c); }
static size_t bionic_strlcpy_chk(char *d, const char *s, size_t n, size_t dl) { (void)dl; return bionic_strlcpy(d, s, n); }
static size_t bionic_strlcat_chk(char *d, const char *s, size_t n, size_t dl) { (void)dl; return bionic_strlcat(d, s, n); }
/* Las cuatro variantes con formato acaban en estas dos. */
static int bionic_vsprintf_chk(char *s, int flags, size_t sl, const char *f, va_list a) {
    (void)flags;
    (void)sl;
    return vsprintf(s, f, a);
}
static int bionic_vsnprintf_chk(char *s, size_t n, int flags, size_t sl, const char *f, va_list a) {
    (void)flags;
    (void)sl;
    return vsnprintf(s, n, f, a);
}
static int bionic_sprintf_chk(char *s, int flags, size_t sl, const char *f, ...) {
    va_list a;
    va_start(a, f);
    int r = bionic_vsprintf_chk(s, flags, sl, f, a);
    va_end(a);
    return r;
}
static int bionic_snprintf_chk(char *s, size_t n, int flags, size_t sl, const char *f, ...) {
    va_list a;
    va_start(a, f);
    int r = bionic_vsnprintf_chk(s, n, flags, sl, f, a);
    va_end(a);
    return r;
}
/* La variante FORTIFY de open, para cuando no hace falta el modo. */
static int bionic_open_2(const char *p, int flags) { return open(p, flags); }
/* La variante FORTIFY de umask. Aquí no se comprueba nada. */
static mode_t bionic_umask_chk(mode_t m) { return umask(m); }

/* ---------- FILE: toda función que recibe un flujo debe traducirlo con real_stream() ---------- */

static int bionic_fclose(FILE *f) { return fclose(real_stream(f)); }
static FILE *bionic_freopen(const char *p, const char *m, FILE *f) { return freopen(p, m, real_stream(f)); }
static size_t bionic_fread(void *p, size_t s, size_t n, FILE *f) { return fread(p, s, n, real_stream(f)); }
static size_t bionic_fwrite(const void *p, size_t s, size_t n, FILE *f) { return fwrite(p, s, n, real_stream(f)); }
static int bionic_fseek(FILE *f, long o, int w) { return fseek(real_stream(f), o, w); }
static long bionic_ftell(FILE *f) { return ftell(real_stream(f)); }
static int bionic_fseeko(FILE *f, off_t o, int w) { return fseeko(real_stream(f), o, w); }
static off_t bionic_ftello(FILE *f) { return ftello(real_stream(f)); }
static void bionic_rewind(FILE *f) { rewind(real_stream(f)); }
static int bionic_fflush(FILE *f) { return fflush(real_stream(f)); }
static int bionic_feof(FILE *f) { return feof(real_stream(f)); }
static int bionic_ferror(FILE *f) { return ferror(real_stream(f)); }
static void bionic_clearerr(FILE *f) { clearerr(real_stream(f)); }
static int bionic_fileno(FILE *f) { return fileno(real_stream(f)); }
static int bionic_fgetc(FILE *f) { return fgetc(real_stream(f)); }
static int bionic_ungetc(int c, FILE *f) { return ungetc(c, real_stream(f)); }
static int bionic_fputc(int c, FILE *f) { return fputc(c, real_stream(f)); }
static char *bionic_fgets(char *s, int n, FILE *f) { return fgets(s, n, real_stream(f)); }
/* fgets con el tamaño real del buffer, que se ignora. */
static char *bionic_fgets_chk(char *s, int n, FILE *f, size_t sl) { (void)sl; return fgets(s, n, real_stream(f)); }
static int bionic_fputs(const char *s, FILE *f) { return fputs(s, real_stream(f)); }
static void bionic_setbuf(FILE *f, char *b) { setbuf(real_stream(f), b); }
static int bionic_setvbuf(FILE *f, char *b, int m, size_t n) { return setvbuf(real_stream(f), b, m, n); }
/* Las variantes con formato también traducen el flujo. */
static int bionic_vfprintf(FILE *f, const char *fmt, va_list a) { return vfprintf(real_stream(f), fmt, a); }
static int bionic_vfscanf(FILE *f, const char *fmt, va_list a) { return vfscanf(real_stream(f), fmt, a); }
static int bionic_fprintf(FILE *f, const char *fmt, ...) {
    va_list a;
    va_start(a, fmt);
    int r = vfprintf(real_stream(f), fmt, a);
    va_end(a);
    return r;
}
static int bionic_fscanf(FILE *f, const char *fmt, ...) {
    va_list a;
    va_start(a, fmt);
    int r = vfscanf(real_stream(f), fmt, a);
    va_end(a);
    return r;
}

/* La hora actual, con milisegundos. Es una función obsoleta, y por eso no depende de la ftime de
   libc. La forma de struct timeb es la de Android en 32 bits. */
typedef struct {
    int32_t time;     /* segundos desde 1970 */
    uint16_t millitm; /* milisegundos */
    int16_t timezone; /* minutos al oeste de UTC */
    int16_t dstflag;  /* 1 si hay horario de verano */
} BionicTimeb;
static int bionic_ftime(BionicTimeb *t) {
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    t->time = (int32_t)now.tv_sec;
    t->millitm = (uint16_t)(now.tv_nsec / 1000000);
    t->timezone = 0;
    t->dstflag = 0;
    return 0;
}

/* ---------- Ficheros: stat y dirent de Android son como las versiones de 64 bits ---------- */

static int bionic_stat(const char *p, struct stat64 *o) { return stat64(p, o); }
static int bionic_lstat(const char *p, struct stat64 *o) { return lstat64(p, o); }
static int bionic_fstat(int fd, struct stat64 *o) { return fstat64(fd, o); }
static int bionic_fstatat(int d, const char *p, struct stat64 *o, int fl) { return fstatat64(d, p, o, fl); }
static struct dirent64 *bionic_readdir(DIR *d) { return readdir64(d); }

/* ---------- Matemáticas: el .so usa softfp, y este programa puede ser hard-float ---------- */

/* sin y cos usan la misma implementación que el código nativo: eso permite comparar imágenes
   enteras entre ARM y x86_64. Cámbialas por las de libm cuando dejes de comparar. */
SOFTFP static double bionic_sin(double x) { return math_sin(x); }
SOFTFP static double bionic_cos(double x) { return math_cos(x); }
SOFTFP static float bionic_sinf(float x) { return (float)math_sin(x); }
SOFTFP static float bionic_cosf(float x) { return (float)math_cos(x); }

/* Generan las versiones double y float de una función de uno o dos argumentos, con la ABI del
   juego. */
#define MATH1(n)                                                                             \
    SOFTFP static double bionic_##n(double x) { return n(x); }                                    \
    SOFTFP static float bionic_##n##f(float x) { return n##f(x); }
#define MATH2(n)                                                                             \
    SOFTFP static double bionic_##n(double x, double y) { return n(x, y); }                       \
    SOFTFP static float bionic_##n##f(float x, float y) { return n##f(x, y); }
MATH1(tan) MATH1(asin) MATH1(acos) MATH1(atan) MATH1(sinh) MATH1(cosh) MATH1(tanh)
MATH1(asinh) MATH1(acosh) MATH1(atanh) MATH1(exp) MATH1(exp2) MATH1(expm1) MATH1(log)
MATH1(log10) MATH1(log2) MATH1(log1p) MATH1(logb) MATH1(sqrt) MATH1(cbrt) MATH1(ceil)
MATH1(floor) MATH1(trunc) MATH1(round) MATH1(rint) MATH1(nearbyint) MATH1(fabs) MATH1(erf)
MATH1(erfc) MATH1(lgamma) MATH1(tgamma)
MATH2(atan2) MATH2(pow) MATH2(fmod) MATH2(hypot) MATH2(copysign) MATH2(fmax) MATH2(fmin)
MATH2(fdim) MATH2(remainder) MATH2(nextafter)

/* Las que llevan un entero o un pointer además del número. */
SOFTFP static double bionic_ldexp(double x, int e) { return ldexp(x, e); }
SOFTFP static float bionic_ldexpf(float x, int e) { return ldexpf(x, e); }
SOFTFP static double bionic_scalbn(double x, int e) { return scalbn(x, e); }
SOFTFP static float bionic_scalbnf(float x, int e) { return scalbnf(x, e); }
SOFTFP static double bionic_frexp(double x, int *e) { return frexp(x, e); }
SOFTFP static float bionic_frexpf(float x, int *e) { return frexpf(x, e); }
SOFTFP static double bionic_modf(double x, double *i) { return modf(x, i); }
SOFTFP static float bionic_modff(float x, float *i) { return modff(x, i); }
SOFTFP static void bionic_sincos(double x, double *s, double *c) { *s = math_sin(x); *c = math_cos(x); }
SOFTFP static void bionic_sincosf(float x, float *s, float *c) { *s = (float)math_sin(x); *c = (float)math_cos(x); }
SOFTFP static double bionic_fma(double x, double y, double z) { return fma(x, y, z); }
SOFTFP static float bionic_fmaf(float x, float y, float z) { return fmaf(x, y, z); }
/* Las que devuelven un entero: el número entra por la ABI del juego. */
SOFTFP static long bionic_lround(double x) { return lround(x); }
SOFTFP static long bionic_lroundf(float x) { return lroundf(x); }
SOFTFP static long bionic_lrint(double x) { return lrint(x); }
SOFTFP static long bionic_lrintf(float x) { return lrintf(x); }
SOFTFP static long long bionic_llround(double x) { return llround(x); }
SOFTFP static long long bionic_llroundf(float x) { return llroundf(x); }
SOFTFP static long long bionic_llrint(double x) { return llrint(x); }
SOFTFP static long long bionic_llrintf(float x) { return llrintf(x); }
SOFTFP static int bionic_ilogb(double x) { return ilogb(x); }
SOFTFP static int bionic_ilogbf(float x) { return ilogbf(x); }

/* Lo que llaman las macros de clasificación de <math.h> en Android. */
SOFTFP static int bionic_isnan(double x) { return isnan(x); }
SOFTFP static int bionic_isnanf(float x) { return isnan(x); }
SOFTFP static int bionic_isinf(double x) { return isinf(x); }
SOFTFP static int bionic_isinff(float x) { return isinf(x); }
SOFTFP static int bionic_isfinite(double x) { return isfinite(x); }
SOFTFP static int bionic_isfinitef(float x) { return isfinite(x); }
SOFTFP static int bionic_isnormal(double x) { return isnormal(x); }
SOFTFP static int bionic_isnormalf(float x) { return isnormal(x); }
SOFTFP static int bionic_signbit(double x) { return signbit(x) != 0; }
SOFTFP static int bionic_signbitf(float x) { return signbit(x) != 0; }
/* Los valores FP_* de Android no son los de glibc. */
static int bionic_fp_class(int c) {
    return c == FP_INFINITE    ? 0x01
           : c == FP_NAN       ? 0x02
           : c == FP_NORMAL    ? 0x04
           : c == FP_SUBNORMAL ? 0x08
                               : 0x10;
}
SOFTFP static int bionic_fpclassifyd(double x) { return bionic_fp_class(fpclassify(x)); }
SOFTFP static int bionic_fpclassifyf(float x) { return bionic_fp_class(fpclassify(x)); }

/* Otras funciones que reciben o devuelven un float o un double. */
SOFTFP static double bionic_strtod(const char *s, char **e) { return strtod(s, e); }
SOFTFP static float bionic_strtof(const char *s, char **e) { return strtof(s, e); }
SOFTFP static double bionic_atof(const char *s) { return strtod(s, NULL); }
SOFTFP static double bionic_drand48(void) { return drand48(); }
SOFTFP static double bionic_difftime(time_t a, time_t b) { return difftime(a, b); }

/* ---------- Sistema ---------- */

/* Los nombres de sysconf de Android no son los de glibc: 0x27 es _SC_PAGESIZE, 0x61 es
   _SC_NPROCESSORS_ONLN... */
static long bionic_sysconf(int name) {
    switch (name) {
    case 0x0006: return sysconf(_SC_CLK_TCK);
    case 0x000B: return sysconf(_SC_OPEN_MAX);
    case 0x0027:
    case 0x0028: return sysconf(_SC_PAGESIZE);
    case 0x0060: return sysconf(_SC_NPROCESSORS_CONF);
    case 0x0061: return sysconf(_SC_NPROCESSORS_ONLN);
    case 0x0062: return sysconf(_SC_PHYS_PAGES);
    case 0x0063: return sysconf(_SC_AVPHYS_PAGES);
    }
    errno = EINVAL;
    return -1;
}
/* La glibc antigua no la exporta, así que se hace con una syscall directa al kernel. */
static pid_t bionic_gettid(void) { return (pid_t)syscall(SYS_gettid); }
/* La syscall propia de ARM, resuelta con la función del compiler. */
static int bionic_cacheflush(long start, long end, long flags) {
    (void)flags;
    __builtin___clear_cache((char *)start, (char *)end);
    return 0;
}
/* Escribe errno y devuelve -1, como esperan las llamadas al sistema de bionic. */
static int bionic_set_errno(int e) {
    errno = e;
    return -1;
}
/* El juego ha abortado: se muestra y se termina. */
static void bionic_abort(void) { die("the .so called abort()"); }
/* Destinos de assert(): muestran el fallo y terminan. */
static void bionic_assert(const char *file, int line, const char *msg) {
    log_msg("[android] assert %s:%d: %s\n", file, line, msg);
    die("the .so failed an assertion");
}
static void bionic_assert2(const char *file, int line, const char *fn, const char *msg) {
    log_msg("[android] assert %s:%d (%s): %s\n", file, line, fn, msg);
    die("the .so failed an assertion");
}
/* Las antiguas operaciones atómicas de Android. Devuelven el valor anterior, y cmpxchg devuelve
   0 si hizo el cambio. */
static int bionic_atomic_cmpxchg(int old, int new_value, volatile int *p) {
    return __sync_val_compare_and_swap(p, old, new_value) != old;
}
static int bionic_atomic_swap(int value, volatile int *p) { return __sync_lock_test_and_set(p, value); }
static int bionic_atomic_dec(volatile int *p) { return __sync_fetch_and_sub(p, 1); }
static int bionic_atomic_inc(volatile int *p) { return __sync_fetch_and_add(p, 1); }
/* Espera en una dirección mientras conserve ese valor, con una syscall directa al kernel. */
static int bionic_futex_wait(volatile void *p, int value, const struct timespec *t) {
    return (int)syscall(SYS_futex, p, 0 /* FUTEX_WAIT */, value, t, NULL, 0);
}
/* Despierta hasta count threads que esperan en esa dirección. */
static int bionic_futex_wake(volatile void *p, int count) {
    return (int)syscall(SYS_futex, p, 1 /* FUTEX_WAKE */, count, NULL, NULL, 0);
}

/* ---------- Log de Android: los mensajes van a la salida de error ---------- */

/* Formatea y envía el mensaje. Las variantes con formato acaban aquí. */
static int bionic_log_vprint(int prio, const char *tag, const char *fmt, va_list a) {
    char b[1024];
    vsnprintf(b, sizeof b, fmt, a);
    log_msg("[android %d/%s] %s\n", prio, tag, b);
    return 0;
}
static int bionic_log_print(int prio, const char *tag, const char *fmt, ...) {
    va_list a;
    va_start(a, fmt);
    int r = bionic_log_vprint(prio, tag, fmt, a);
    va_end(a);
    return r;
}
static int bionic_log_write(int prio, const char *tag, const char *m) {
    log_msg("[android %d/%s] %s\n", prio, tag, m);
    return 0;
}
static int bionic_log_buf_write(int buf, int prio, const char *tag, const char *m) {
    (void)buf;
    return bionic_log_write(prio, tag, m);
}
static int bionic_log_buf_print(int buf, int prio, const char *tag, const char *fmt, ...) {
    (void)buf;
    va_list a;
    va_start(a, fmt);
    int r = bionic_log_vprint(prio, tag, fmt, a);
    va_end(a);
    return r;
}
/* Un fallo fatal del juego: muestra el mensaje y termina. */
static void bionic_log_assert(const char *cond, const char *tag, const char *fmt, ...) {
    char b[1024] = "";
    if (fmt) {
        va_list a;
        va_start(a, fmt);
        vsnprintf(b, sizeof b, fmt, a);
        va_end(a);
    }
    log_msg("[android 7/%s] assert %s: %s\n", tag, cond ? cond : "", b);
    die("the .so called __android_log_assert");
}

/* ---------- C++ ---------- */

/* new y delete son malloc y free. Fíjate en la j de _Znwj: size_t es unsigned int en ARM32. */
static void *cxx_new(size_t n) { return arena_malloc(n); }
static void cxx_delete(void *p) { arena_free(p); }
static void *cxx_new_nothrow(size_t n, const void *tag) { (void)tag; return arena_malloc(n); }
static void cxx_delete_nothrow(void *p, const void *tag) { (void)tag; arena_free(p); }
/* Se llamó a un método virtual puro: es un error del juego. */
static void cxx_pure(void) { die("pure virtual method called"); }
/* El bit 0 de la guard word dice que el objeto está listo. Con threads, aquí haría falta
   un lock de verdad. */
static int cxx_guard_acquire(uint32_t *g) { return !(*g & 1); }
static void cxx_guard_release(uint32_t *g) { *g = 1; }
static void cxx_guard_abort(uint32_t *g) { (void)g; }
/* No se registran destructores de salida: el programa termina sin ejecutarlos. */
static int bionic_cxa_atexit(void *f, void *a, void *d) {
    (void)f;
    (void)a;
    (void)d;
    return 0;
}
static void bionic_cxa_finalize(void *d) { (void)d; }
/* Lo mismo, con el orden de la ABI de ARM: objeto, función y dso. */
static int bionic_aeabi_atexit(void *o, void *f, void *d) {
    (void)o;
    (void)f;
    (void)d;
    return 0;
}
/* std::nothrow: un objeto vacío, y al juego solo le importa su dirección. */
static uint8_t cxx_nothrow;

/* ---------- Funciones de apoyo de la ABI de ARM, para un .so que las importe ---------- */

static int bionic_idiv(int a, int b) { return a / b; }
static unsigned bionic_uidiv(unsigned a, unsigned b) { return a / b; }
/* Cociente en r0 y resto en r1: justo lo que parece un resultado de 64 bits en esta ABI. */
static uint64_t bionic_idivmod(int a, int b) {
    return (uint32_t)(a / b) | ((uint64_t)(uint32_t)(a % b) << 32);
}
static uint64_t bionic_uidivmod(unsigned a, unsigned b) {
    return (a / b) | ((uint64_t)(a % b) << 32);
}
/* Las versiones de la ABI de ARM: no devuelven nada. */
static void bionic_aeabi_memcpy(void *d, const void *s, size_t n) { memcpy(d, s, n); }
static void bionic_aeabi_memmove(void *d, const void *s, size_t n) { memmove(d, s, n); }
/* fíjate en el orden: es distinto del de memset */
static void bionic_aeabi_memset(void *d, size_t n, int c) { memset(d, c, n); }
static void bionic_aeabi_memclr(void *d, size_t n) { memset(d, 0, n); }

/* ---------- libdl ----------
   dlsym pasa por el mismo resolvedor que las importaciones, así que no puede saltarse una
   versión nativa. */

/* No se abre nada: las funciones se resuelven por nombre. */
static void *bionic_dlopen(const char *n, int f) {
    (void)f;
    log_msg("[hook] dlopen(%s)\n", n ? n : "NULL");
    return (void *)1; /* un identificador que nadie lee, solo distinto de NULL */
}
/* La función pedida se resuelve como una importación, y por tanto recibe un hook. */
static void *bionic_dlsym(void *h, const char *n) {
    (void)h;
    return resolve_import(n);
}
static int bionic_dlclose(void *h) {
    (void)h;
    return 0;
}
/* Sin error. */
static const char *bionic_dlerror(void) { return NULL; }

/* ---------- La tabla ----------
   Lo que no aparece en ella recibe un stub que devuelve 0 y avisa la primera vez. Si una
   función tiene versión nativa, lo que indique esta tabla no llega a ejecutarse. */

/* Variables de libc que el juego importa como datos. */
extern char **environ;
extern char *__progname;

/* Macros de la tabla. FUNC asigna un nombre a una función. SAME usa la función de libc con ese
   nombre. WRAP usa el wrapper bionic_nombre. MATHS, sus versiones double y float. DATA, un dato. */
#define FUNC(sym, fn) {sym, (const void *)(fn), 0}
#define SAME(fn) {#fn, (const void *)(fn), 0}
#define WRAP(fn) {#fn, (const void *)(bionic_##fn), 0}
#define MATHS(n) WRAP(n), WRAP(n##f)
#define DATA(sym, p) {sym, (const void *)(p), 1}
static const BionicEntry bionic_table[] = {
    /* Datos */
    DATA("__stack_chk_guard", &bionic_stack_chk_guard),
    DATA("__sF", fake_stdio),
    DATA("_ctype_", bionic_ctype),
    DATA("_tolower_tab_", &bionic_tolower_tab),
    DATA("_toupper_tab_", &bionic_toupper_tab),
    DATA("__isthreaded", &bionic_isthreaded),
    DATA("environ", &environ),
    DATA("__progname", &__progname),
    DATA("_ZSt7nothrow", &cxx_nothrow),
    /* Memoria: el heap del juego */
    FUNC("malloc", arena_malloc),
    FUNC("free", arena_free),
    FUNC("calloc", arena_calloc),
    FUNC("realloc", arena_realloc),
    WRAP(memalign),
    WRAP(posix_memalign),
    WRAP(valloc),
    FUNC("malloc_usable_size", arena_usable_size),
    FUNC("dlmalloc", arena_malloc),
    FUNC("dlfree", arena_free),
    FUNC("dlcalloc", arena_calloc),
    FUNC("dlrealloc", arena_realloc),
    FUNC("dlmemalign", arena_memalign),
    FUNC("dlmalloc_usable_size", arena_usable_size),
    WRAP(strdup),
    WRAP(strndup),
    /* Texto */
    SAME(memcpy), SAME(memmove), SAME(memset), SAME(memcmp), SAME(memchr), SAME(memrchr),
    SAME(memmem), SAME(memccpy), SAME(strlen), SAME(strnlen), SAME(strcmp), SAME(strncmp),
    SAME(strcasecmp), SAME(strncasecmp), SAME(strcoll), SAME(strxfrm), SAME(strcpy),
    SAME(strncpy), SAME(strcat), SAME(strncat), SAME(strchr), SAME(strrchr), SAME(strstr),
    SAME(strcasestr), SAME(strpbrk), SAME(strspn), SAME(strcspn), SAME(strtok), SAME(strtok_r),
    SAME(strsep), SAME(strerror), SAME(strsignal), SAME(bzero), SAME(bcopy), SAME(bcmp),
    SAME(index), SAME(rindex), SAME(ffs),
    WRAP(strlcpy), WRAP(strlcat), WRAP(strerror_r),
    FUNC("__memcpy_chk", bionic_memcpy_chk), FUNC("__memmove_chk", bionic_memmove_chk),
    FUNC("__memset_chk", bionic_memset_chk), FUNC("__strcpy_chk", bionic_strcpy_chk),
    FUNC("__strncpy_chk", bionic_strncpy_chk), FUNC("__strcat_chk", bionic_strcat_chk),
    FUNC("__strncat_chk", bionic_strncat_chk), FUNC("__strlen_chk", bionic_strlen_chk),
    FUNC("__strchr_chk", bionic_strchr_chk), FUNC("__strrchr_chk", bionic_strrchr_chk),
    FUNC("__strlcpy_chk", bionic_strlcpy_chk), FUNC("__strlcat_chk", bionic_strlcat_chk),
    FUNC("__sprintf_chk", bionic_sprintf_chk), FUNC("__snprintf_chk", bionic_snprintf_chk),
    FUNC("__vsprintf_chk", bionic_vsprintf_chk), FUNC("__vsnprintf_chk", bionic_vsnprintf_chk),
    FUNC("__fgets_chk", bionic_fgets_chk), FUNC("__open_2", bionic_open_2), FUNC("__umask_chk", bionic_umask_chk),
    FUNC("__stack_chk_fail", bionic_stack_chk_fail),
    /* Números y entorno */
    SAME(atoi), SAME(atol), SAME(atoll), SAME(strtol), SAME(strtoul), SAME(strtoll),
    SAME(strtoull), SAME(strtoimax), SAME(strtoumax), SAME(qsort), SAME(bsearch), SAME(lrand48),
    SAME(srand48), SAME(mrand48), SAME(nrand48), SAME(getenv), SAME(setenv), SAME(unsetenv),
    SAME(putenv), SAME(clearenv), SAME(system), SAME(exit), SAME(_exit), SAME(mkstemp),
    SAME(mkdtemp), SAME(realpath), WRAP(abort),
    WRAP(strtod), WRAP(strtof), WRAP(atof), WRAP(drand48), WRAP(difftime),
    /* Flujos */
    SAME(fopen), SAME(fdopen), SAME(tmpfile), SAME(remove), SAME(rename), SAME(perror),
    SAME(getchar), SAME(putchar), SAME(puts), SAME(printf), SAME(vprintf), SAME(sprintf),
    SAME(vsprintf), SAME(snprintf), SAME(vsnprintf), SAME(asprintf), SAME(vasprintf),
    SAME(scanf), SAME(sscanf), SAME(vsscanf),
    WRAP(fclose), WRAP(freopen), WRAP(fread), WRAP(fwrite), WRAP(fseek), WRAP(ftell),
    WRAP(fseeko), WRAP(ftello), WRAP(rewind), WRAP(fflush), WRAP(feof), WRAP(ferror),
    WRAP(clearerr), WRAP(fileno), WRAP(fgetc), FUNC("getc", bionic_fgetc), WRAP(ungetc), WRAP(fputc),
    FUNC("putc", bionic_fputc), WRAP(fgets), WRAP(fputs), WRAP(setbuf), WRAP(setvbuf), WRAP(vfprintf),
    WRAP(vfscanf), WRAP(fprintf), WRAP(fscanf),
    /* Descriptores y ficheros */
    SAME(open), SAME(openat), SAME(creat), SAME(close), SAME(read), SAME(write), SAME(pread),
    SAME(pwrite), SAME(pread64), SAME(pwrite64), SAME(readv), SAME(writev), SAME(lseek),
    SAME(lseek64), SAME(ftruncate), SAME(ftruncate64), SAME(truncate), SAME(access), SAME(unlink),
    SAME(link), SAME(symlink), SAME(readlink), SAME(mkdir), SAME(rmdir), SAME(chdir),
    SAME(getcwd), SAME(chmod), SAME(fchmod), SAME(chown), SAME(umask), SAME(fsync),
    SAME(fdatasync), SAME(sync), SAME(fcntl), SAME(ioctl), SAME(flock), SAME(dup), SAME(dup2),
    SAME(pipe), SAME(pipe2), SAME(isatty), SAME(select), SAME(poll),
    WRAP(stat), WRAP(lstat), WRAP(fstat), WRAP(fstatat),
    SAME(opendir), SAME(fdopendir), SAME(closedir), SAME(rewinddir), SAME(dirfd), WRAP(readdir),
    SAME(mmap), SAME(mmap64), SAME(munmap), SAME(mprotect), SAME(msync), SAME(madvise),
    SAME(mlock), SAME(munlock), WRAP(cacheflush),
    /* Tiempo: time_t de 32 bits, como en Android */
    WRAP(ftime),
    SAME(time), SAME(gettimeofday), SAME(clock_gettime), SAME(clock_getres),
    SAME(clock_nanosleep), SAME(clock), SAME(times), SAME(nanosleep), FUNC("usleep", thread_usleep), SAME(sleep),
    SAME(gmtime), SAME(gmtime_r), SAME(localtime), SAME(localtime_r), SAME(mktime), SAME(timegm),
    SAME(asctime), SAME(ctime), SAME(strftime), SAME(strptime), SAME(tzset),
    /* Procesos */
    SAME(getpid), SAME(getppid), SAME(getuid), SAME(geteuid), SAME(getgid), SAME(getegid),
    SAME(getpagesize), SAME(gethostname), SAME(raise), SAME(kill), WRAP(sysconf), WRAP(gettid),
    FUNC("bsd_signal", signal), FUNC("sysv_signal", signal), SAME(syscall),
    /* Caracteres */
    SAME(wctob), SAME(btowc), SAME(wctype), SAME(iswctype),
    SAME(isalnum), SAME(isalpha), SAME(isblank), SAME(iscntrl), SAME(isdigit), SAME(isgraph),
    SAME(islower), SAME(isprint), SAME(ispunct), SAME(isspace), SAME(isupper), SAME(isxdigit),
    SAME(tolower), SAME(toupper), SAME(setlocale), SAME(wcslen), SAME(wcscmp), SAME(wcscpy),
    SAME(wcsncpy), SAME(wcscat), SAME(wcschr), SAME(wcsstr), SAME(wmemcmp), SAME(wmemcpy),
    SAME(wmemset),
    /* Solo en Android */
    FUNC("__errno", __errno_location), FUNC("__set_errno", bionic_set_errno), FUNC("__assert", bionic_assert),
    FUNC("__assert2", bionic_assert2), FUNC("__cxa_atexit", bionic_cxa_atexit), FUNC("__cxa_finalize", bionic_cxa_finalize),
    FUNC("__atomic_cmpxchg", bionic_atomic_cmpxchg), FUNC("__atomic_swap", bionic_atomic_swap),
    FUNC("__atomic_dec", bionic_atomic_dec), FUNC("__atomic_inc", bionic_atomic_inc),
    FUNC("__futex_wait", bionic_futex_wait), FUNC("__futex_wake", bionic_futex_wake),
    /* Log de Android */
    FUNC("__android_log_print", bionic_log_print), FUNC("__android_log_write", bionic_log_write),
    FUNC("__android_log_vprint", bionic_log_vprint), FUNC("__android_log_buf_write", bionic_log_buf_write),
    FUNC("__android_log_buf_print", bionic_log_buf_print), FUNC("__android_log_assert", bionic_log_assert),
    /* C++ */
    FUNC("_Znwj", cxx_new), FUNC("_Znaj", cxx_new), FUNC("_ZdlPv", cxx_delete), FUNC("_ZdaPv", cxx_delete),
    FUNC("_ZnwjRKSt9nothrow_t", cxx_new_nothrow), FUNC("_ZnajRKSt9nothrow_t", cxx_new_nothrow),
    FUNC("_ZdlPvRKSt9nothrow_t", cxx_delete_nothrow), FUNC("_ZdaPvRKSt9nothrow_t", cxx_delete_nothrow),
    FUNC("__cxa_pure_virtual", cxx_pure), FUNC("__cxa_guard_acquire", cxx_guard_acquire),
    FUNC("__cxa_guard_release", cxx_guard_release), FUNC("__cxa_guard_abort", cxx_guard_abort),
    FUNC("__aeabi_atexit", bionic_aeabi_atexit),
    /* ABI de ARM */
    FUNC("__aeabi_idiv", bionic_idiv), FUNC("__aeabi_uidiv", bionic_uidiv), FUNC("__aeabi_idivmod", bionic_idivmod),
    FUNC("__aeabi_uidivmod", bionic_uidivmod), FUNC("__aeabi_memcpy", bionic_aeabi_memcpy),
    FUNC("__aeabi_memcpy4", bionic_aeabi_memcpy), FUNC("__aeabi_memcpy8", bionic_aeabi_memcpy),
    FUNC("__aeabi_memmove", bionic_aeabi_memmove), FUNC("__aeabi_memset", bionic_aeabi_memset),
    FUNC("__aeabi_memclr", bionic_aeabi_memclr), FUNC("__aeabi_memclr4", bionic_aeabi_memclr),
    FUNC("__aeabi_memclr8", bionic_aeabi_memclr),
    /* libdl */
    FUNC("__gnu_Unwind_Find_exidx", elf_find_exidx), FUNC("dl_unwind_find_exidx", elf_find_exidx),
    FUNC("dlopen", bionic_dlopen), FUNC("dlsym", bionic_dlsym), FUNC("dlclose", bionic_dlclose), FUNC("dlerror", bionic_dlerror),
    /* Matemáticas */
    MATHS(sin), MATHS(cos), MATHS(tan), MATHS(asin), MATHS(acos), MATHS(atan), MATHS(sinh),
    MATHS(cosh), MATHS(tanh), MATHS(asinh), MATHS(acosh), MATHS(atanh), MATHS(exp), MATHS(exp2),
    MATHS(expm1), MATHS(log), MATHS(log10), MATHS(log2), MATHS(log1p), MATHS(logb), MATHS(sqrt),
    MATHS(cbrt), MATHS(ceil), MATHS(floor), MATHS(trunc), MATHS(round), MATHS(rint),
    MATHS(nearbyint), MATHS(fabs), MATHS(erf), MATHS(erfc), MATHS(lgamma), MATHS(tgamma),
    MATHS(atan2), MATHS(pow), MATHS(fmod), MATHS(hypot), MATHS(copysign), MATHS(fmax), MATHS(fmin),
    MATHS(fdim), MATHS(remainder), MATHS(nextafter), MATHS(ldexp), MATHS(scalbn), MATHS(frexp),
    MATHS(modf), MATHS(sincos), MATHS(fma), MATHS(lround), MATHS(lrint), MATHS(llround),
    MATHS(llrint), MATHS(ilogb),
    FUNC("__isnan", bionic_isnan), FUNC("__isnanf", bionic_isnanf), FUNC("isnan", bionic_isnan), FUNC("isnanf", bionic_isnanf),
    FUNC("__isinf", bionic_isinf), FUNC("__isinff", bionic_isinff), FUNC("isinf", bionic_isinf), FUNC("isinff", bionic_isinff),
    FUNC("__isfinite", bionic_isfinite), FUNC("__isfinitef", bionic_isfinitef), FUNC("finite", bionic_isfinite),
    FUNC("finitef", bionic_isfinitef), FUNC("__isnormal", bionic_isnormal), FUNC("__isnormalf", bionic_isnormalf),
    FUNC("__signbit", bionic_signbit), FUNC("__signbitf", bionic_signbitf), FUNC("__fpclassifyd", bionic_fpclassifyd),
    FUNC("__fpclassifyf", bionic_fpclassifyf),
};

/* Busca un nombre en las dos tablas. is_data dice si es un dato. Devuelve 0 si no está. */
const void *bionic_find(const char *name, int *is_data) {
    *is_data = 0;
    for (unsigned i = 0; i < sizeof bionic_table / sizeof bionic_table[0]; i++)
        if (!strcmp(bionic_table[i].name, name)) {
            *is_data = bionic_table[i].is_data;
            return bionic_table[i].address;
        }
    for (unsigned i = 0; i < pthread_entry_count; i++)
        if (!strcmp(pthread_entries[i].name, name)) return pthread_entries[i].address;
    return 0;
}
