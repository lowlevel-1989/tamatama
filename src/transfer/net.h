/* transfer/net.h: la red de Transfer Save. Todo va por un socket UDP, en
   paquetes de texto (el protocolo está en README.md). */
#pragma once
#include "saves.h"
#include <netinet/in.h>
#include <stddef.h>
#include <stdint.h>

#define NET_PORT 47474
#define NET_PROTOCOL 1

/* Un paquete recibido: TAMA 1 <kind> <firma> <words>\n<body>. */
typedef struct {
    struct sockaddr_in from;
    char kind[8];      /* HELLO, HERE, OFFER, WAIT, OK, NO... */
    char signature[5]; /* la firma de quien lo manda */
    const char *words; /* el resto de la primera línea, o "" */
    const uint8_t *body;
    size_t body_size;
} Packet;

typedef struct {
    int fd;
    char role; /* 'S' en Send Save, 'R' en Receive Save */
    const char *name, *signature;
    uint64_t next_hello, next_sweep;
    /* las redes: su broadcast y el rango de direcciones que se recorre */
    struct {
        uint32_t broadcast, first, last;
    } networks[8];
    int network_count;
    int sweeping; /* la red que se recorre ahora, o network_count */
    uint32_t host; /* la siguiente dirección */
    int budget;    /* bytes en la cola de salida hasta los que se recorre */
    uint8_t buffer[SAVE_MAX + 1024];
} Net;

/* Abre el socket en NET_PORT (o en uno libre si está ocupado). 1 si va. */
int net_open(Net *n, char role, const char *name, const char *signature);
void net_close(Net *n);
/* El siguiente paquete de otro dispositivo: 1 si hay. Mientras, anuncia
   este cada segundo y contesta a los anuncios de los demás. */
int net_next(Net *n, Packet *p);
/* Manda un paquete; words y body pueden ser NULL. */
void net_send(Net *n, const struct sockaddr_in *to, const char *kind, const char *words, const void *body,
              size_t size);
/* 1 si los dos vienen del mismo sitio (dirección y puerto). */
int net_same(const struct sockaddr_in *a, const struct sockaddr_in *b);
