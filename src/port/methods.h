/* port/methods.h: cómo se llama a los métodos nativos del .so desde app_main y
   desde touch.c. */
#pragma once

#define METHOD(name) "Java_com_namcobandaigames_tamagotchisp_SampleGameNatives_" name

// El juego no debería mirar dentro de jclass.
#define FAKE_JCLASS 0x1000u
