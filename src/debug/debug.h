/* debug/debug.h: lo que el resto del programa usa del debug, y lo que cada programa debe
   ofrecerle. Todo lo demás del debug está en esta carpeta. */
#pragma once
#include "common.h"

/* Abre la consola: el socket de debug_server.c, con ese nombre dentro de data_dir(). Con start_paused, el juego espera aquí, antes de
   ejecutar ningún método, hasta que le digas que siga. */
void debug_start(const char *name, int start_paused);
/* Se llama al volver de cada call_method: comprueba las variables y atiende la consola. */
void debug_after_call(const char *method);
/* Se llama al volver de cada función nativa que llama el juego, como un método Java de tu port. */
void debug_after_native(const char *method);
/* Pausa cada vez que vuelva este método. También con la regla pause NOMBRE. */
void debug_pause_at(const char *method);

/* Seguir variables: lo explica watch.c. También puedes usarlas desde tu código. */
void watch_add(const char *name, const char *spec);
int watch_remove(const char *name);
void watch_list(void);
void watch_poll(const char *where);

/* Mensajes: solo a la consola, o también a la terminal del programa. */
void debug_send(const char *format, ...);
void debug_report(const char *format, ...);

/* Lo definen host.c y arm_main.c, porque cada programa ve la memoria del juego a su manera. */
arm_ptr arm_so_base(void);                              /* dónde quedó cargado el .so */
int arm_readable(arm_ptr address);                      /* 1 si esa dirección se puede leer */
void arm_memory_range(arm_ptr *start, uint32_t *bytes); /* dónde tiene sentido buscar */
