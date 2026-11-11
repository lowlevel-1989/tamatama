/* transfer/sender.h: el envío de una partida, sin pantallas. Lo usan las
   pantallas de send.c (PC y consolas) y las de Android (TransferActivity,
   por src/platform/android/transfer.c). */
#pragma once
#include "net.h"
#include "saves.h"

#define SENDER_DEVICES 8

/* Un dispositivo en Receive Save. */
typedef struct {
    struct sockaddr_in address;
    char signature[5];
    char name[64];
    uint64_t seen;
} Device;

/* En qué va el envío. */
typedef enum {
    SENDER_IDLE,     /* ninguno: se elige a quién */
    SENDER_OFFERING, /* la oferta, hasta que el otro diga que la tiene */
    SENDER_ASKED,    /* el otro pregunta Accept o Decline */
    SENDER_SENDING,  /* la partida, hasta que diga que la ha guardado */
} SenderStage;

typedef struct {
    Net net;
    char name[64], signature[5]; /* los de este dispositivo */
    Device devices[SENDER_DEVICES];
    int count;
    /* el envío en curso */
    SenderStage stage;
    Device target;
    char offer[512];
    unsigned offers;       /* cuántas ofertas ha hecho: la de ahora */
    char offer_words[16];  /* su número, que va en OFFER */
    uint8_t *data;
    long size;
    char data_words[32];
    uint64_t next_retry, deadline;
    const char *result; /* cuando termina: "save sent", "declined"... */
} Sender;

/* Abre la red con un nombre y una firma nuevos. 1 si va. */
int sender_open(Sender *s);
void sender_close(Sender *s);
/* Una vez por imagen: oye la red, rehace la lista de dispositivos y, en un
   envío, repite lo que toca. Devuelve 1 cuando el envío termina: el
   resultado está en result. */
int sender_poll(Sender *s);
/* Empieza a mandar save, de la versión md5, a devices[device]. NULL si
   empieza, o por qué no. */
const char *sender_start(Sender *s, int device, const char *md5, const Save *save);
/* Corta el envío y se lo dice al otro. */
void sender_cancel(Sender *s);
