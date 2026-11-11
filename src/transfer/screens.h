/* transfer/screens.h: qué tiene cada pantalla de Transfer Save. Aquí solo
   se arman (los textos y las opciones, ver doc/screen.es.md); quién las
   enseña y qué se hace con lo elegido está en transfer.c, send.c y
   receive.c. Los avisos con OK son screen_message, en esos mismos
   ficheros. */
#pragma once
#include "screen.h"

/* ---------- transfer.c ---------- */

/* La primera: qué hacer. */
enum { HOME_SEND, HOME_RECEIVE, HOME_RESTORE, HOME_BACK };
void screens_home(Screen *s);

/* Elegir la versión: names son las que tienen partida (o backup), y la
   opción count es Back. */
void screens_versions(Screen *s, const char *title, const char *const *names, int count);

/* Restore Backup: de qué versión y de cuándo es el backup. */
enum { RESTORE_YES, RESTORE_BACK };
void screens_restore(Screen *s, const char *version, const char *date, const char *summary);

/* ---------- send.c ---------- */

/* A quién mandar la partida. La lista de dispositivos cambia mientras se
   ve: cada vez, screens_devices_begin, un screens_device por cada uno y
   screens_devices_end, que añade Back (la opción found). */
void screens_send(Screen *s, const char *version, const char *summary, const char *me, const char *signature);
void screens_devices_begin(Screen *s);
void screens_device(Screen *s, const char *signature, const char *name);
void screens_devices_end(Screen *s, int found);

/* Esperando a que el otro pulse Accept. Su única opción es Cancel. */
void screens_waiting_answer(Screen *s, const char *name, const char *signature);

/* ---------- receive.c ---------- */

/* Esperando una partida: este dispositivo y su firma. Su opción es Back. */
void screens_receive(Screen *s, const char *me, const char *signature);

/* Llegó una oferta: de quién, de qué versión y de cuándo. */
enum { ASK_ACCEPT, ASK_DECLINE };
void screens_ask(Screen *s, const char *name, const char *signature, const char *version, const char *date,
                 const char *summary);

/* Tras Accept, mientras llega la partida. */
void screens_receiving(Screen *s);
