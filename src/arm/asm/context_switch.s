@ arm/asm/context_switch.s
@
@ void context_switch(Context *save, const Context *load);
@
@ Cambia de un thread a otro dentro del mismo thread del sistema. Guarda en
@ save lo que debe sobrevivir a una llamada, carga lo de load y salta a su lr.
@ Para el otro contexto es como si su llamada a context_switch acabara de
@ volver. C no puede cambiar sp ni elegir adónde vuelve una función.
@
@ Solo guarda lo que AAPCS obliga a conservar a quien es llamado:
@   - r4-r11, sp y lr.
@   - d8-d15, la mitad de los registers de VFP.
@ r0-r3, r12 y d0-d7 no: quien llama a una función ya cuenta con que cambien.
@
@ Context, en arm/arm.h:
@   +0   r4-r11       8 palabras
@   +32  sp
@   +36  lr           adónde vuelve este contexto
@   +40  d8-d15       8 dobles palabras
@
@ Al entrar:
@   r0 = save
@   r1 = load

    .syntax unified
    .arm                                @ modo ARM, válido desde ARMv5TE: Raspberry Pi y QEMU
    .fpu vfp                            @ para poder nombrar d8-d15
    .text

    .globl context_switch
    .type  context_switch, %function

context_switch:
    @ Guarda el contexto actual en save.
    stmia r0!, {r4-r11}                 @ save->r4..r11, y r0 avanza 32 bytes
    str   sp, [r0], #4                  @ save->sp: sp por separado, en stm está desaconsejado
    str   lr, [r0], #4                  @ save->lr: al volver a este contexto se sigue ahí
    vstmia r0, {d8-d15}                 @ save->d8..d15

    @ Carga el otro contexto desde load.
    ldmia r1!, {r4-r11}                 @ r4..r11 = load->r4..r11, y r1 avanza 32 bytes
    ldr   sp, [r1], #4                  @ a partir de aquí se usa el stack del otro contexto
    ldr   lr, [r1], #4                  @ lr = adónde vuelve el otro contexto
    vldmia r1, {d8-d15}                 @ d8..d15 = load->d8..d15

    bx    lr                            @ sigue el otro: vuelve de su context_switch, o empieza

    .section .note.GNU-stack,"",%progbits
