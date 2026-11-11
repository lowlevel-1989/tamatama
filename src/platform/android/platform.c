/* platform/android/platform.c: la plataforma Android, para la app de
   android/ (make PLATFORM=android). Implementa platform/platform.h:

     base_dir()      la carpeta interna de la app ({files}): ahí van apk/,
                     resources/, keyboard.map y screenshots/
     apk_each(use)   los .apk de {base_dir}/apk/, que copia ahí MainActivity
                     desde el .zip que elige el usuario
     apk_hint()      cómo aportarlos: Import games en la pantalla de inicio
     data_dir()      {base_dir}/resources
     platform_log()  los mensajes, a logcat con la etiqueta "tamatama"

   Y las entradas de Java:
     MainActivity.choices  las versiones que se pueden jugar (apk_choices),
                           para la lista de la pantalla de inicio. Va sin SDL
                           arrancado, así que la carpeta llega de Java.
     SDL_main              ArenaActivity, con la versión elegida y la
                           carpeta de las librerías de la app.
     TransferActivity      Transfer Save: transfer.c, en esta carpeta.

   Hay dos libtama.so, como en el PC hay dos programas:
     armeabi-v7a   loader-arm entero (arm_main.c, con main renombrado a
                   arena_main): el juego y el código nativo en este proceso.
                   Solo en teléfonos que ejecutan ARM de 32 bits.
     arm64-v8a,    loader (host.c, con main renombrado a host_main): pantalla
     x86_64        y funciones nativas aquí, y el .so en loader-arm, otro
                   proceso: directo si la CPU ejecuta ARM de 32 bits, bajo
                   qemu-arm si no. Los dos van en la carpeta de librerías
                   como libloader-arm.so y libqemu-arm.so (estáticos: no
                   necesitan las librerías de 32 bits de Android). */
#include "common.h"
#include "platform.h"
#include "android_platform.h"
#include <SDL.h>
#include <android/log.h>
#include <jni.h>
#include <dirent.h>
#include <limits.h>
#include <unistd.h>
#include <pthread.h>

static char base[PATH_MAX];

void android_set_base(const char *files) { snprintf(base, sizeof base, "%s", files); }

const char *base_dir(void) {
    if (!base[0]) {
        const char *path = SDL_AndroidGetInternalStoragePath();
        if (!path) die("cannot find the app folder");
        snprintf(base, sizeof base, "%s", path);
    }
    return base;
}

/* Solo los ficheros que acaban en .apk, para scandir. */
static int is_apk(const struct dirent *e) {
    size_t n = strlen(e->d_name);
    return n > 4 && !strcmp(e->d_name + n - 4, ".apk");
}

/* Como en Linux: los *.apk de {base}/apk/, en orden alfabético, hasta que use
   devuelva 1. */
int apk_each(int (*use)(const char *path)) {
    char dir[PATH_MAX], path[PATH_MAX];
    if (snprintf(dir, sizeof dir, "%s/apk", base_dir()) >= (int)sizeof dir) return 0;
    struct dirent **list;
    int count = scandir(dir, &list, is_apk, alphasort);
    if (count < 0) return 0;
    int found = 0;
    for (int k = 0; k < count; k++) {
        int fits = snprintf(path, sizeof path, "%s/%s", dir, list[k]->d_name) < (int)sizeof path;
        if (!found && fits && use(path)) found = 1;
        free(list[k]);
    }
    free(list);
    return found;
}

const char *apk_hint(void) { return "choose Import games: a .zip with the game .apk files"; }

const char *data_dir(void) {
    static char dir[PATH_MAX];
    if (!dir[0]) snprintf(dir, sizeof dir, "%s/resources", base_dir());
    return dir;
}

void platform_log(const char *text) { __android_log_write(ANDROID_LOG_INFO, "tamatama", text); }

/* La captura, a la galería: ArenaActivity.toGallery(path) la copia a
   Pictures/Tamatama con MediaStore. Desde el thread del juego: SDL lo une a
   la JVM. */
void platform_screenshot_saved(const char *path) {
    JNIEnv *env = SDL_AndroidGetJNIEnv();
    jobject activity = SDL_AndroidGetActivity();
    if (!env || !activity) return;
    jclass type = (*env)->GetObjectClass(env, activity);
    jmethodID method = (*env)->GetStaticMethodID(env, type, "toGallery", "(Ljava/lang/String;)Z");
    jstring text = (*env)->NewStringUTF(env, path);
    int done = method && (*env)->CallStaticBooleanMethod(env, type, method, text);
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
        done = 0;
    }
    log_msg(done ? "[capture] copied to the gallery: Pictures/Tamatama\n"
                 : "[capture] cannot copy it to the gallery\n");
    (*env)->DeleteLocalRef(env, text);
    (*env)->DeleteLocalRef(env, type);
    (*env)->DeleteLocalRef(env, activity);
}

/* El teléfono vibra por ArenaActivity.vibrate(ms), como vibración de un
   juego. Desde el thread del juego: SDL lo une a la JVM. */
int platform_vibrate(int ms) {
    JNIEnv *env = SDL_AndroidGetJNIEnv();
    jobject activity = SDL_AndroidGetActivity();
    if (!env || !activity) return 0;
    jclass type = (*env)->GetObjectClass(env, activity);
    jmethodID method = (*env)->GetStaticMethodID(env, type, "vibrate", "(I)Z");
    int done = method && (*env)->CallStaticBooleanMethod(env, type, method, (jint)ms);
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
        done = 0;
    }
    (*env)->DeleteLocalRef(env, type);
    (*env)->DeleteLocalRef(env, activity);
    return done;
}

/* ArenaActivity.openUrl(url): un Intent para el navegador. */
int platform_open_url(const char *url) {
    JNIEnv *env = SDL_AndroidGetJNIEnv();
    jobject activity = SDL_AndroidGetActivity();
    if (!env || !activity) return 0;
    jclass type = (*env)->GetObjectClass(env, activity);
    jmethodID method = (*env)->GetStaticMethodID(env, type, "openUrl", "(Ljava/lang/String;)Z");
    jstring text = (*env)->NewStringUTF(env, url);
    int done = method && (*env)->CallStaticBooleanMethod(env, type, method, text);
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
        done = 0;
    }
    (*env)->DeleteLocalRef(env, text);
    (*env)->DeleteLocalRef(env, type);
    (*env)->DeleteLocalRef(env, activity);
    return done;
}

/* MainActivity.choices(files): {nombre, juego, nombre, juego...}, con lo que
   hay que pasarle a ArenaActivity para jugar cada versión. */
JNIEXPORT jobjectArray JNICALL Java_com_lowlevel1989_tamatama_MainActivity_choices(JNIEnv *env, jclass type,
                                                                                   jstring files) {
    (void)type;
    const char *path = (*env)->GetStringUTFChars(env, files, NULL);
    android_set_base(path);
    (*env)->ReleaseStringUTFChars(env, files, path);

    static ApkChoice choices[16];
    int count = apk_choices(choices, 16);
    jclass string = (*env)->FindClass(env, "java/lang/String");
    jobjectArray out = (*env)->NewObjectArray(env, 2 * count, string, NULL);
    for (int i = 0; i < count; i++) {
        jstring name = (*env)->NewStringUTF(env, choices[i].name);
        jstring game = (*env)->NewStringUTF(env, choices[i].game);
        (*env)->SetObjectArrayElement(env, out, 2 * i, name);
        (*env)->SetObjectArrayElement(env, out, 2 * i + 1, game);
        (*env)->DeleteLocalRef(env, name);
        (*env)->DeleteLocalRef(env, game);
    }
    return out;
}

#ifdef __arm__
/* loader-arm, con su main renombrado (android/CMakeLists.txt). */
int arena_main(int argc, char **argv);
#define program_main arena_main
#else
/* loader, con su main renombrado (android/CMakeLists.txt). */
int host_main(int argc, char **argv);
#define program_main host_main

/* Dónde encuentra loader lo suyo (host.c y platform/linux en loader-arm). */
static void host_environment(const char *libs) {
    char path[PATH_MAX];
    snprintf(path, sizeof path, "%s/libloader-arm.so", libs);
    setenv("LOADER_ARM", path, 1);
    snprintf(path, sizeof path, "%s/libqemu-arm.so", libs);
    setenv("QEMU_ARM", path, 1);
    setenv("ARENA_BASE", base_dir(), 1);
}
#endif

/* exit() correría después los destructores de las librerías del sistema
   mientras los threads de la interfaz siguen vivos, y el proceso caería
   (SIGSEGV, SIGABRT). Registrado el último, este se ejecuta el primero y
   termina sin ellos: el juego ya ha guardado lo suyo. */
static void exit_now(void) {
    fflush(NULL);
    _exit(0);
}

static char *program_args[3];

static void *run_program(void *result) {
    *(int *)result = program_main(2, program_args);
    return NULL;
}

/* argv llega de ArenaActivity.getArguments, detrás del nombre del programa:
   la versión elegida y la carpeta de las librerías. Sin versión, el menú de
   SDL.

   El programa va en un thread propio con un stack de PC: el del thread que
   crea Java (SDLThread) es pequeño para loader, que se desborda. */
int SDL_main(int argc, char *argv[]) {
    atexit(exit_now);
#ifndef __arm__
    if (argc > 2) host_environment(argv[2]);
#endif
    program_args[0] = "tamatama";
    program_args[1] = argc > 1 ? argv[1] : "--menu";
    int result = 1;
    pthread_attr_t attributes;
    pthread_t thread;
    pthread_attr_init(&attributes);
    pthread_attr_setstacksize(&attributes, 16 << 20);
    if (pthread_create(&thread, &attributes, run_program, &result)) return program_main(2, program_args);
    pthread_join(thread, NULL);
    return result;
}
