/* host/environment.h: en qué máquina estamos y cómo puede ejecutar su CPU el
   código ARM del juego. Solo lo usa host.c para decidir entre QEMU y la propia
   CPU. */
#pragma once
/* Los entornos que distingue el loader. */
typedef enum { ENV_X86_64, ENV_ARM64, ENV_ARM32, ENV_UNKNOWN } Environment;

/* El código nativo siempre participa. Lo único que se elige es quién ejecuta el
   ARM: la propia
   CPU (ARM_ON_CPU) o QEMU (ARM_ON_QEMU). */
typedef enum { ARM_ON_CPU, ARM_ON_QEMU } ArmRunner;

/* Para qué se compiló este programa. Lo decide el compiler con sus macros
   predefinidas: con --target=arm... aparece __arm__ aunque estés en un PC. */
static inline Environment build_environment(void) {
#if defined(__x86_64__)
    return ENV_X86_64;
#elif defined(__aarch64__)
    return ENV_ARM64;
#elif defined(__arm__)
    return ENV_ARM32;
#else
    return ENV_UNKNOWN;
#endif
}
/* Nombre legible del entorno, para los mensajes. */
static inline const char *environment_name(Environment e) {
    return e == ENV_X86_64  ? "x86_64"
           : e == ENV_ARM64 ? "aarch64"
           : e == ENV_ARM32 ? "arm"
                            : "unknown";
}
