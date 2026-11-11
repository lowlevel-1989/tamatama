/* port/methods.h: cómo se llama a los métodos nativos del .so desde app_main y
   desde touch.c. */
#pragma once

/* El prefijo depende de la versión cargada: está en apk_table.h. */
#define METHOD(name) apk_method(name)

// El juego no debería mirar dentro de jclass.
#define FAKE_JCLASS 0x1000u
