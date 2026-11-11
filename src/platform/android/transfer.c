/* platform/android/transfer.c: Transfer Save para TransferActivity, la
   pantalla de Android. La red y las partidas son las de src/transfer
   (sender.c, receiver.c, saves.c); aquí solo se pasan a Java. Todo se
   llama desde el thread de la interfaz: TransferActivity llama a sendPoll
   o receivePoll cada pocos milisegundos mientras se ve. */
#include "common.h"
#include "android_platform.h"
#include "receiver.h"
#include "saves.h"
#include "sender.h"
#include <jni.h>

#define JAVA(name) Java_com_lowlevel1989_tamatama_TransferActivity_##name
#define MAX_VERSIONS 16

static ApkChoice choices[MAX_VERSIONS];
static int count;
static Sender sender;
static Receiver receiver;

static jobjectArray strings(JNIEnv *env, const char *const *values, int n) {
    jclass type = (*env)->FindClass(env, "java/lang/String");
    jobjectArray out = (*env)->NewObjectArray(env, n, type, NULL);
    for (int i = 0; i < n; i++) {
        jstring value = (*env)->NewStringUTF(env, values[i] ? values[i] : "");
        (*env)->SetObjectArrayElement(env, out, i, value);
        (*env)->DeleteLocalRef(env, value);
    }
    return out;
}

static jstring string(JNIEnv *env, const char *value) { return value ? (*env)->NewStringUTF(env, value) : NULL; }

/* El md5 de Java en out; 0 si no es una versión instalada. */
static int version_of(JNIEnv *env, jstring md5, char out[40]) {
    const char *text = (*env)->GetStringUTFChars(env, md5, NULL);
    snprintf(out, 40, "%s", text);
    (*env)->ReleaseStringUTFChars(env, md5, text);
    for (int i = 0; i < count; i++)
        if (!strcmp(choices[i].md5, out)) return 1;
    return 0;
}

/* La carpeta de la app y las versiones instaladas. */
JNIEXPORT void JNICALL JAVA(start)(JNIEnv *env, jclass type, jstring files) {
    (void)type;
    const char *path = (*env)->GetStringUTFChars(env, files, NULL);
    android_set_base(path);
    (*env)->ReleaseStringUTFChars(env, files, path);
    count = apk_choices(choices, MAX_VERSIONS);
}

/* Las versiones con partida (o con backup): {nombre, md5, resumen, fecha}
   por cada una. */
JNIEXPORT jobjectArray JNICALL JAVA(versions)(JNIEnv *env, jclass type, jboolean backup) {
    (void)type;
    static char dates[MAX_VERSIONS][32];
    static Save saves[MAX_VERSIONS];
    const char *values[MAX_VERSIONS * 4];
    int found = 0;
    for (int i = 0; i < count; i++) {
        Save *save = &saves[found];
        if (!save_find(choices[i].md5, backup, save)) continue;
        save_date_text(save->date, dates[found], sizeof dates[0]);
        values[4 * found] = choices[i].name;
        values[4 * found + 1] = choices[i].md5;
        values[4 * found + 2] = save->summary;
        values[4 * found + 3] = dates[found];
        found++;
    }
    return strings(env, values, 4 * found);
}

JNIEXPORT jboolean JNICALL JAVA(restore)(JNIEnv *env, jclass type, jstring md5) {
    (void)type;
    char game[40];
    if (!version_of(env, md5, game)) return JNI_FALSE;
    int ok = save_restore_backup(game);
    log_msg("[transfer] restore backup of %s: %s\n", game, ok ? "done" : "failed");
    return ok ? JNI_TRUE : JNI_FALSE;
}

/* ---------- Send Save ---------- */

/* {nombre, firma} de este dispositivo, o null sin red. */
JNIEXPORT jobjectArray JNICALL JAVA(sendOpen)(JNIEnv *env, jclass type) {
    (void)type;
    if (!sender_open(&sender)) return NULL;
    const char *values[] = {sender.name, sender.signature};
    return strings(env, values, 2);
}

JNIEXPORT void JNICALL JAVA(sendClose)(JNIEnv *env, jclass type) {
    (void)env, (void)type;
    sender_close(&sender);
}

/* true cuando el envío termina: sendResult dice cómo. */
JNIEXPORT jboolean JNICALL JAVA(sendPoll)(JNIEnv *env, jclass type) {
    (void)env, (void)type;
    return sender_poll(&sender) ? JNI_TRUE : JNI_FALSE;
}

/* {firma, nombre} por cada dispositivo en Receive Save. */
JNIEXPORT jobjectArray JNICALL JAVA(sendDevices)(JNIEnv *env, jclass type) {
    (void)type;
    const char *values[SENDER_DEVICES * 2];
    for (int i = 0; i < sender.count; i++) {
        values[2 * i] = sender.devices[i].signature;
        values[2 * i + 1] = sender.devices[i].name;
    }
    return strings(env, values, 2 * sender.count);
}

/* Manda la partida de md5 a la firma signature: null si empieza, o por qué
   no. */
JNIEXPORT jstring JNICALL JAVA(sendStart)(JNIEnv *env, jclass type, jstring signature, jstring md5) {
    (void)type;
    char game[40];
    Save save;
    if (!version_of(env, md5, game) || !save_find(game, 0, &save)) return string(env, "cannot read the save");
    const char *text = (*env)->GetStringUTFChars(env, signature, NULL);
    int device = -1;
    for (int i = 0; i < sender.count; i++)
        if (!strcmp(sender.devices[i].signature, text)) device = i;
    (*env)->ReleaseStringUTFChars(env, signature, text);
    return string(env, sender_start(&sender, device, game, &save));
}

JNIEXPORT jstring JNICALL JAVA(sendResult)(JNIEnv *env, jclass type) {
    (void)type;
    return string(env, sender.result);
}

JNIEXPORT void JNICALL JAVA(sendCancel)(JNIEnv *env, jclass type) {
    (void)env, (void)type;
    sender_cancel(&sender);
}

/* ---------- Receive Save ---------- */

/* {nombre, firma} de este dispositivo, o null sin red. */
JNIEXPORT jobjectArray JNICALL JAVA(receiveOpen)(JNIEnv *env, jclass type) {
    (void)type;
    if (!receiver_open(&receiver, choices, count)) return NULL;
    const char *values[] = {receiver.name, receiver.signature};
    return strings(env, values, 2);
}

JNIEXPORT void JNICALL JAVA(receiveClose)(JNIEnv *env, jclass type) {
    (void)env, (void)type;
    receiver_close(&receiver);
}

/* Si se atienden ofertas nuevas: solo en la pantalla de espera. */
JNIEXPORT void JNICALL JAVA(receiveListen)(JNIEnv *env, jclass type, jboolean on) {
    (void)env, (void)type;
    receiver.listening = on;
}

/* 0, o un evento de receiver.h: 1 oferta, 2 cancelada, 3 terminada. */
JNIEXPORT jint JNICALL JAVA(receivePoll)(JNIEnv *env, jclass type) {
    (void)env, (void)type;
    return receiver_poll(&receiver);
}

/* La oferta: {nombre, firma, versión, fecha, resumen}. */
JNIEXPORT jobjectArray JNICALL JAVA(receiveOffer)(JNIEnv *env, jclass type) {
    (void)type;
    const Offer *o = &receiver.offer;
    char date[32];
    save_date_text((time_t)o->date, date, sizeof date);
    int version = receiver_version(&receiver);
    const char *values[] = {o->name, o->signature, version >= 0 ? choices[version].name : "", date, o->summary};
    return strings(env, values, 5);
}

JNIEXPORT void JNICALL JAVA(receiveAccept)(JNIEnv *env, jclass type) {
    (void)env, (void)type;
    receiver_accept(&receiver);
}

JNIEXPORT void JNICALL JAVA(receiveDecline)(JNIEnv *env, jclass type) {
    (void)env, (void)type;
    receiver_decline(&receiver);
}

JNIEXPORT void JNICALL JAVA(receiveCancel)(JNIEnv *env, jclass type) {
    (void)env, (void)type;
    receiver_cancel(&receiver);
}

JNIEXPORT jstring JNICALL JAVA(receiveResult)(JNIEnv *env, jclass type) {
    (void)type;
    return string(env, receiver.result);
}

/* Lo último que pasó (una oferta rechazada...), o null. */
JNIEXPORT jstring JNICALL JAVA(receiveNotice)(JNIEnv *env, jclass type) {
    (void)type;
    return string(env, receiver.notice);
}
