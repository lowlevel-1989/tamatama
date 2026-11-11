/* host/sandbox.h: lanzar loader-arm (o qemu-arm) desde una app de Android.
   Ver sandbox.c. Solo existe en Android. */
#pragma once
#include <sys/types.h>

/* Crea un proceso hijo que ejecuta child(context), trazado por un thread
   supervisor. child no vuelve (termina con exec o _exit). Devuelve el pid. */
pid_t sandbox_spawn(void (*child)(void *context), void *context);
/* Espera a que termine el hijo de sandbox_spawn y devuelve su estado, como
   waitpid. */
int sandbox_wait(void);
