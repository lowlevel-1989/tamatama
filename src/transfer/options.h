/* transfer/options.h: las opciones de Transfer Save (send.c, receive.c y
   transfer.c) y lo que comparten. */
#pragma once
#include "common.h"
#include "saves.h"

void send_save(ApkChoice *choices, int count);
void receive_save(ApkChoice *choices, int count);
/* Elige una versión instalada con partida (o con backup): su número en
   choices, con la partida en save, o -1 si se vuelve. */
int choose_version(const char *title, ApkChoice *choices, int count, int backup, Save *save);
