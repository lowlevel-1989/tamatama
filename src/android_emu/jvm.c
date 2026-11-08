/* android_emu/jvm.c: una JavaVM que no ejecuta Java. El .so solo ve una tabla de funciones y cada
   entrada es un hook, de modo que también se puede hacer hook de toda JNI y sustituirla. */
#include "arm.h"
#include "jni_names.h"
#include <stdarg.h>
#define JNI_COUNT (sizeof jni_names / sizeof jni_names[0])

/* JNIEnv es un pointer a un pointer a la tabla. Viven en la arena, para que el loader pueda
   entregar la misma dirección al código nativo. Con libdemo.so, la celda queda en 0x60805000,
   justo después del .so: es lo siguiente libre de la arena. */
static void ***env_cell;
static void ***vm_cell;

/* Crea una cadena o un array con n bytes, y un 0 al final. */
static JniObject *object_new(const void *p, uint32_t n) {
    JniObject *o = arena_malloc(sizeof(JniObject) + n + 1);
    o->len = n;
    o->dirty = 0;
    if (p) memcpy(o->data, p, n);
    o->data[n] = 0;
    return o;
}
/* Una clase o un método de Java no son más que su nombre. concat3 crea ese texto uniendo tres
   trozos. */
static char *concat3(const char *a, const char *b, const char *c) {
    char *s = arena_malloc(strlen(a) + strlen(b) + strlen(c) + 1);
    strcpy(s, a);
    strcpy(s + strlen(s), b);
    strcpy(s + strlen(s), c);
    return s;
}
/* Lo registrado con RegisterNatives: muchos juegos no exportan sus métodos como Java_... */
typedef struct {
    const char *name, *signature;
    void *fn;
} Method;
/* Los métodos registrados hasta ahora. */
static Method methods[256];
static int method_count;
/* La entrada de un método registrado: el hook, si tiene versión nativa. */
void *jni_registered(const char *n) {
    for (int i = 0; i < method_count; i++)
        if (!strcmp(methods[i].name, n)) return methods[i].fn;
    return 0;
}
/* Las funciones implementadas. Todo lo demás acaba en UNIMPLEMENTED y devuelve 0. ENV_ARG es el
   primer argumento de todas: env, en r0. */
#define ENV_ARG void *env
/* Devuelve JNI_VERSION_1_6. */
static int jni_GetVersion(ENV_ARG) {
    (void)env;
    return 0x00010006;
}
/* La clase es su propio nombre, copiado. */
static void *jni_FindClass(ENV_ARG, const char *n) {
    (void)env;
    log_msg("[jni] FindClass(%s)\n", n);
    return concat3(n, "", "");
}
/* Recorre los métodos que registra el juego. Los que tienen versión nativa reciben un hook. */
static int jni_RegisterNatives(ENV_ARG, void *k, const Method *m, int c) {
    (void)env;
    (void)k;
    for (int i = 0; i < c && method_count < 256; i++) {
        Method method = m[i];
        log_msg("[jni] RegisterNatives %s %s -> %p\n", method.name, method.signature, method.fn);
        if (is_hooked(method.name)) {
            /* Los métodos estáticos no tienen símbolo: este es el único momento para atajarlos. */
            method.fn = hook_with_target(method.name, method.fn);
        }
        methods[method_count++] = method;
    }
    return 0;
}
/* Un identificador de método es el texto Clase.nombre(firma). */
static void *jni_GetMethodID(ENV_ARG, void *k, const char *n, const char *f) {
    (void)env;
    return concat3((char *)k, ".", concat3(n, "", f));
}
/* Muestra una llamada a un método de Java, con sus argumentos. El método es el texto
   Clase.nombre(firma), así que la firma dice qué hay en args:
     Ljava/lang/String;   un jstring, que se muestra como texto
     L...; y [...         otro objeto o un array, que se muestra como dirección
     J                    un long, de 64 bits
     F y D                un float o un double: en args, los dos son double
     Z B C S I            un entero */
static void show_java_call(const char *method, va_list args, uint32_t caller) {
    char text[256] = "";
    int used = 0;
    const char *p = strchr(method, '(');
    for (p = p ? p + 1 : ""; *p && *p != ')' && used < (int)sizeof text;) {
        int room = (int)sizeof text - used;
        const char *separator = used ? ", " : "";
        const char *end = p;
        while (*end == '[') end++;
        if (*end == 'L') end = strchr(end, ';');
        if (!end) break;
        if (!strncmp(p, "Ljava/lang/String;", 18)) {
            JniObject *o = va_arg(args, JniObject *);
            used += snprintf(text + used, (size_t)room, "%s\"%s\"", separator, o ? (char *)o->data : "null");
        } else if (*p == 'L' || *p == '[') {
            used += snprintf(text + used, (size_t)room, "%s0x%x", separator, va_arg(args, uint32_t));
        } else if (*p == 'J') {
            used += snprintf(text + used, (size_t)room, "%s%lld", separator, va_arg(args, long long));
        } else if (*p == 'F' || *p == 'D') {
            used += snprintf(text + used, (size_t)room, "%s%g", separator, va_arg(args, double));
        } else {
            used += snprintf(text + used, (size_t)room, "%s%d", separator, va_arg(args, int));
        }
        p = end + 1;
    }
    char from[160];
    elf_format_address(caller, from, sizeof from);
    log_msg("[hook] Java -> %s%s%s  from %s\n", method, used ? "  args: " : "", text, from);
}
/* Los métodos de Java que atiende loader-arm: la lista está en port/arm_java_table.h. */
static const struct {
    const char *name;
    uint64_t (*fn)(const uint32_t *words); /* words: los argumentos del método */
} arm_java[] = {
#define ARM_JAVA(name, fn) {name, fn},
#if __has_include("arm_java_table.h")
#include "arm_java_table.h"
#endif
#undef ARM_JAVA
    {0, 0}};
/* Ejecuta un método de Java. Si está en arm_java, lo atiende loader-arm. Si no, quien lo
   implementa es src/port, con una línea en port_table.h cuyo
   nombre es el texto del método. La función nativa lo recibe como una llamada normal: el primer
   argumento en r0, el segundo en r1, y el resultado en r0 y r1. Sin versión nativa, muestra la
   llamada y devuelve 0. En ARM, un va_list es un pointer a los argumentos: los cuatro primeros
   van a los registers, y los demás se leen con stack_arg. */
static uint64_t run_java_method(const char *method, va_list args, uint32_t caller) {
    const uint32_t *words = *(const uint32_t **)&args;
    if (callers_enabled()) print_callers(words, method);
    for (int i = 0; arm_java[i].name; i++)
        if (!strcmp(arm_java[i].name, method)) return arm_java[i].fn(words);
    int index = native_active(method);
    if (index < 0) {
        show_java_call(method, args, caller);
        return 0;
    }
    uint32_t r[4] = {words[0], words[1], words[2], words[3]};
    run_native(index, r, (uint32_t)(words + 4));
    return r[0] | ((uint64_t)r[1] << 32);
}
/* Las llamadas a métodos estáticos de Java. Devuelven 64 bits porque, en softfp, cualquier tipo
   vuelve en r0 y r1, así que una sola función sirve para todos. */
static uint64_t jni_CallStaticMethod(ENV_ARG, void *k, void *m, ...) {
    va_list args;
    (void)env;
    (void)k;
    va_start(args, m);
    uint64_t result = run_java_method(m, args, (uint32_t)__builtin_return_address(0));
    va_end(args);
    return result;
}
/* Lo mismo, con los argumentos en un va_list. Es la forma que usa el jni.h de C++. */
static uint64_t jni_CallStaticMethodV(ENV_ARG, void *k, void *m, va_list args) {
    (void)env;
    (void)k;
    return run_java_method(m, args, (uint32_t)__builtin_return_address(0));
}
/* Qué función atiende un CallStatic<Tipo>Method de la tabla JNI: los terminados en Method llevan
   ... y los terminados en MethodV llevan va_list. La forma MethodA aún no está. */
static void *call_static_handler(const char *name) {
    if (strncmp(name, "CallStatic", 10)) return 0;
    char last = name[strlen(name) - 1];
    if (last == 'd') return jni_CallStaticMethod;
    if (last == 'V') return jni_CallStaticMethodV;
    return 0;
}
/* Crea un jstring. */
static void *jni_NewStringUTF(ENV_ARG, const char *s) {
    (void)env;
    return object_new(s, strlen(s));
}
/* Devuelve el texto del jstring, sin copiarlo. */
static const char *jni_GetStringUTFChars(ENV_ARG, JniObject *s, uint8_t *c) {
    (void)env;
    if (c) *c = 0;
    return (char *)s->data;
}
/* No hay nada que liberar. */
static void jni_ReleaseStringUTFChars(ENV_ARG, JniObject *s, const char *c) {
    (void)env;
    (void)s;
    (void)c;
}
/* La longitud de un array. */
static int jni_GetArrayLength(ENV_ARG, JniObject *a) {
    (void)env;
    return (int)a->len;
}
/* Crea un byte[]. En Java, un array nuevo siempre está a cero. */
static void *jni_NewByteArray(ENV_ARG, int length) {
    (void)env;
    JniObject *array = object_new(NULL, (uint32_t)length);
    memset(array->data, 0, (size_t)length);
    return array;
}
/* Copia bytes del juego al array. */
static void jni_SetByteArrayRegion(ENV_ARG, JniObject *a, int start, int length, const void *buffer) {
    (void)env;
    memcpy(a->data + start, buffer, (size_t)length);
}
/* Copia bytes del array al juego. */
static void jni_GetByteArrayRegion(ENV_ARG, JniObject *a, int start, int length, void *buffer) {
    (void)env;
    memcpy(buffer, a->data + start, (size_t)length);
}
/* Entrega los bytes del array, sin copiarlos. */
static void *jni_GetByteArrayElements(ENV_ARG, JniObject *a, uint8_t *c) {
    (void)env;
    if (c) *c = 0;
    return a->data;
}
/* Marca el array como modificado, salvo con JNI_ABORT, que vale 2. */
static void jni_ReleaseByteArrayElements(ENV_ARG, JniObject *a, void *p, int m) {
    (void)env;
    (void)p;
    if (m != 2) a->dirty = 1;
}
/* Las referencias no se cuentan: la global es el mismo objeto. */
static void *jni_NewGlobalRef(ENV_ARG, void *o) {
    (void)env;
    return o;
}
/* Escribe el JavaVM donde pide el juego, y lo crea si aún no existe. */
static int jni_GetJavaVM(ENV_ARG, void **vm) {
    (void)env;
    *vm = jni_vm();
    return 0;
}
/* Las referencias no se cuentan. */
static void jni_DeleteLocalRef(ENV_ARG, void *o) {
    (void)env;
    (void)o;
}
/* Nunca hay excepciones pendientes. */
static uint8_t jni_ExceptionCheck(ENV_ARG) {
    (void)env;
    return 0;
}
/* Tampoco aquí: NULL significa que no hay ninguna. */
static void *jni_ExceptionOccurred(ENV_ARG) {
    (void)env;
    return 0;
}
/* No hay nada que borrar. */
static void jni_ExceptionClear(ENV_ARG) { (void)env; }

typedef struct {
    const char *name;
    void *fn;
} JniImpl;
/* Nombre en la tabla JNI y función que lo atiende. */
static const JniImpl jni_impl[] = {
    {"GetVersion", jni_GetVersion},
    {"FindClass", jni_FindClass},
    {"RegisterNatives", jni_RegisterNatives},
    {"GetMethodID", jni_GetMethodID},
    {"GetStaticMethodID", jni_GetMethodID},
    {"NewStringUTF", jni_NewStringUTF},
    {"GetStringUTFChars", jni_GetStringUTFChars},
    {"ReleaseStringUTFChars", jni_ReleaseStringUTFChars},
    {"GetArrayLength", jni_GetArrayLength},
    {"NewByteArray", jni_NewByteArray},
    {"SetByteArrayRegion", jni_SetByteArrayRegion},
    {"GetByteArrayRegion", jni_GetByteArrayRegion},
    {"GetByteArrayElements", jni_GetByteArrayElements},
    {"ReleaseByteArrayElements", jni_ReleaseByteArrayElements},
    {"DeleteLocalRef", jni_DeleteLocalRef},
    {"NewGlobalRef", jni_NewGlobalRef},
    {"DeleteGlobalRef", jni_DeleteLocalRef},
    {"GetJavaVM", jni_GetJavaVM},
    {"ExceptionCheck", jni_ExceptionCheck},
    {"ExceptionOccurred", jni_ExceptionOccurred},
    {"ExceptionClear", jni_ExceptionClear},
};
/* Escribe el JNIEnv y devuelve JNI_OK. */
static int vm_GetEnv(void *vm, void **env, int v) {
    (void)vm;
    (void)v;
    *env = env_cell;
    return 0;
}
/* Todos los threads usan el mismo JNIEnv. */
static int vm_Attach(void *vm, void **env, void *a) {
    (void)vm;
    (void)a;
    *env = env_cell;
    return 0;
}
/* No hay nada que deshacer. */
static int vm_Detach(void *vm) {
    (void)vm;
    return 0;
}
/* Rellena la tabla: cada entrada es un hook llamado "JNI:Nombre". */
void *jni_env(void) {
    if (!env_cell) {
        env_cell = arena_take(4, 4);
        void **table = arena_take(JNI_COUNT * 4, 4);
        *env_cell = table;
        char name[64];
        for (unsigned i = 0; i < JNI_COUNT; i++) {
            void *target = 0;
            for (unsigned k = 0; k < sizeof jni_impl / sizeof jni_impl[0]; k++)
                if (!strcmp(jni_impl[k].name, jni_names[i])) target = jni_impl[k].fn;
            if (!target) target = call_static_handler(jni_names[i]);
            snprintf(name, sizeof name, "JNI:%s", jni_names[i]);
            /* El hook guarda este pointer, así que el nombre necesita su propia copia. Va en
               la libc de este proceso porque solo lo lee hooks.c: en la arena, la búsqueda de
               la consola lo recorrería como si fueran datos del juego. */
            table[i] = hook_with_target(strdup(name), target);
        }
    }
    return env_cell;
}
/* JavaVM funciona igual, con ocho entradas. GetEnv es la número 6, en el offset 0x18. */
void *jni_vm(void) {
    static const char *const n[8] = {"VM:reserved0",
                                     "VM:reserved1",
                                     "VM:reserved2",
                                     "VM:DestroyJavaVM",
                                     "VM:AttachCurrentThread",
                                     "VM:DetachCurrentThread",
                                     "VM:GetEnv",
                                     "VM:AttachCurrentThreadAsDaemon"};
    void *const t[8] = {0, 0, 0, 0, vm_Attach, vm_Detach, vm_GetEnv, vm_Attach};
    if (!vm_cell) {
        vm_cell = arena_take(4, 4);
        void **table = arena_take(8 * 4, 4);
        *vm_cell = table;
        for (int i = 0; i < 8; i++) table[i] = hook_with_target(n[i], t[i]);
    }
    return vm_cell;
}
