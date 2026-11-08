@ arm/asm/hook_common.s
@
@ El punto por el que pasan todos los hooks. Cada hook tiene 16 bytes de
@ código que cargan en ip sus datos y saltan aquí (arm/hooks.c).
@
@ No se puede escribir en C: los argumentos del juego están en r0-r3, y una
@ función en C podría cambiarlos antes de leerlos. Tampoco sabría saltar a
@ otra función como si nada hubiera pasado.
@
@ La decisión se toma en dispatch, en arm/hooks.c:
@   void *dispatch(Hook *hook, uint32_t *regs);
@
@ regs apunta a lo que se guarda en el stack, en este orden:
@   regs[0] = r0
@   regs[1] = r1
@   regs[2] = r2
@   regs[3] = r3
@   regs[4] = r4
@   regs[5] = lr
@
@ Al entrar:
@   ip    = el Hook
@   r0-r3 = los argumentos que puso el juego
@   lr    = adónde hay que volver en el juego

    .syntax unified
    .arm                                @ modo ARM, válido desde ARMv5TE:
                                        @ Raspberry Pi y QEMU
    .text

    .globl hook_common
    .type  hook_common, %function

hook_common:
    push {r0, r1, r2, r3, r4, lr}       @ 6 palabras: el stack sigue aligned a 8

    mov  r0, ip                         @ primer argumento de dispatch: el Hook
    mov  r1, sp                         @ segundo argumento: los registers
                                        @ guardados
    bl   dispatch                       @ r0 = adónde seguir, o 0 si ya está
                                        @ resuelto

    cmp  r0, #0
    beq  .Lfinished

    @ Hay un destino: se salta a él con los registers originales.
    mov  ip, r0                         @ ip = el destino
    pop  {r0, r1, r2, r3, r4, lr}       @ todo vuelve a estar como lo dejó el
                                        @ juego
    bx   ip                             @ esa función volverá directamente al
                                        @ juego

.Lfinished:
    @ Función nativa o stub: dispatch escribió el resultado en regs[0] y
    @ regs[1].
    pop  {r0, r1, r2, r3, r4, pc}       @ r0 y r1 llevan el resultado; pc vuelve
                                        @ al juego

    .section .note.GNU-stack,"",%progbits
