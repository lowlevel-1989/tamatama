/* shared/bridge.h: el protocolo entre loader y loader-arm cuando el código ARM corre bajo QEMU en
   otro proceso. Solo lo incluyen los ficheros que hablan por ese canal: host/host.c,
   arm/arm_main.c, arm/arm_mem.c y arm/hooks.c. El código del juego no lo necesita: en otras
   plataformas, o sin .so, no hay dos procesos. */
#pragma once

/* Mensajes de loader a loader-arm: [operación, bytes] y después los datos. */
enum { 
  TO_ARM_CALL_NAME = 1,
  TO_ARM_CALL_ADDRESS,
  TO_ARM_SYMBOL,
  TO_ARM_ALLOC,
  TO_ARM_NATIVE_DONE,
  TO_ARM_EXIT
};

/* Mensajes de loader-arm a loader: 
 * empiezan por una de estas etiquetas. */
enum {
  TO_HOST_RETURN = 1,
  TO_HOST_NATIVE_CALL = 2
};

/* Descriptores que el loader deja abiertos al lanzar QEMU. 
 * 0, 1 y 2 siguen siendo los de siempre, 
 * y por eso estos son los siguientes. */
enum {
  FD_FROM_HOST = 3,
  FD_TO_HOST = 4,
  FD_ARENA = 5
};

/* Primera palabra que envía loader-arm: sus bytes forman "OKOK" en ASCII, fácil de ver en un dump. No es una dirección. */
#define TO_HOST_HELLO 0x4F4B4F4Bu
