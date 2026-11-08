@ arm/asm/switch_stack.s
@
@ void switch_stack(void *sp, void (*fn)(void *), void *arg);
@
@ Cambia a otro stack y llama a fn(arg). No vuelve nunca, porque el stack
@ anterior ya no se usa. C no puede cambiar el register sp.
@
@ Al entrar:
@   r0 = el stack nuevo
@   r1 = fn
@   r2 = arg

    .syntax unified
    .arm                                @ modo ARM, válido desde ARMv5TE:
                                        @ Raspberry Pi y QEMU
    .text

    .globl switch_stack
    .type  switch_stack, %function

switch_stack:
    mov  sp, r0                         @ a partir de aquí se usa el stack nuevo
    mov  r0, r2                         @ r0 = arg, el argumento de fn
    blx  r1                             @ llama a fn

.Lforever:
    b    .Lforever                      @ fn no debe volver

    .section .note.GNU-stack,"",%progbits
