/* transfer/receiver.h: la recepción de una partida, sin pantallas. Lo usan
   las pantallas de receive.c (PC y consolas) y las de Android
   (TransferActivity, por src/platform/android/transfer.c). */
#pragma once
#include "common.h"
#include "net.h"

/* Una oferta: de quién y qué partida. */
typedef struct {
    char name[64], signature[5], game[40], file[40], summary[32];
    long size;
    long long date;
} Offer;

typedef enum {
    RECEIVER_IDLE,     /* esperando una oferta */
    RECEIVER_ASKING,   /* llegó una: Accept o Decline */
    RECEIVER_ACCEPTED, /* esperando la partida */
    RECEIVER_FINISHED, /* contestado: se repite a lo que el otro repita */
} ReceiverStage;

/* Lo que devuelve receiver_poll. */
enum {
    RECEIVER_OFFER = 1, /* llegó una oferta: está en offer */
    RECEIVER_CANCELLED, /* el que envía la ha cancelado */
    RECEIVER_DONE,      /* tras Accept, terminó: el resultado en result */
};

typedef struct {
    Net net;
    char name[64], signature[5]; /* los de este dispositivo */
    ApkChoice *choices;          /* las versiones instaladas */
    int count;
    int listening; /* se atienden ofertas nuevas; si no, "busy" */
    ReceiverStage stage;
    struct sockaddr_in from;
    Offer offer;
    char offer_number[16]; /* el de OFFER: el mismo es una repetición */
    char reply_kind[8], reply_words[16]; /* lo último contestado */
    uint64_t deadline;
    const char *result; /* "save received", "the save arrived damaged"... */
    const char *notice; /* lo último que pasó: una oferta rechazada... */
} Receiver;

/* Abre la red con un nombre y una firma nuevos. 1 si va. */
int receiver_open(Receiver *r, ApkChoice *choices, int count);
void receiver_close(Receiver *r);
/* Una vez por imagen: oye la red y contesta. Devuelve 0 o un evento. */
int receiver_poll(Receiver *r);
/* La versión de la oferta: su número en choices. */
int receiver_version(const Receiver *r);
void receiver_accept(Receiver *r);
void receiver_decline(Receiver *r);
/* Tras Accept, deja de esperar la partida. */
void receiver_cancel(Receiver *r);
