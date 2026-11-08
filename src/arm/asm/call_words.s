@ arm/asm/call_words.s
@
@ uint64_t call_words(void *fn, const uint32_t *words, uint32_t n);
@
@ Llama a fn con n argumentos leídos de un array. En C, el número de
@ argumentos de una llamada se fija al compilar; aquí se decide al ejecutar.
@ Es lo mismo que hace ffi_call en libffi.
@
@ Los coloca como dice AAPCS, la convención de llamadas de ARM:
@   - Los cuatro primeros, en r0, r1, r2 y r3.
@   - El resto, en el stack, aligned a 8.
@   - El resultado vuelve en r0 y r1.
@
@ Al entrar:
@   r0 = fn
@   r1 = words
@   r2 = n

    .syntax unified
    .arm                                @ modo ARM, válido desde ARMv5TE: Raspberry Pi y QEMU
    .text

    .globl call_words
    .type  call_words, %function

call_words:
    push {r4, r5, r6, r7, r11, lr}      @ estos registers deben volver intactos

    mov  r4, r0                         @ r4 = fn
    mov  r5, r1                         @ r5 = words
    mov  r6, r2                         @ r6 = n
    mov  r11, sp                        @ r11 = el stack tal como estaba

    @ ¿Cuántos argumentos van al stack?
    subs r7, r6, #4                     @ r7 = n - 4
    ble  .Lload_registers               @ con 4 o menos, ninguno

    @ Reserva su sitio.
    sub  sp, sp, r7, lsl #2             @ sp = sp - r7 * 4
    bic  sp, sp, #7                     @ baja sp hasta un múltiplo de 8

    @ Copia desde words[4] hasta el final.
    add  r1, r5, #16                    @ r1 = &words[4]
    mov  r2, sp                         @ r2 = destino, en el stack

.Lcopy_stack_args:
    ldr  r3, [r1], #4                   @ r3 = *r1, y r1 avanza 4
    str  r3, [r2], #4                   @ *r2 = r3, y r2 avanza 4
    subs r7, r7, #1                     @ queda uno menos
    bne  .Lcopy_stack_args

.Lload_registers:
    ldm  r5, {r0, r1, r2, r3}           @ r0-r3 = words[0..3]; si n < 4, fn no mira los demás
    blx  r4                             @ llama a fn; el resultado queda en r0 y r1

    mov  sp, r11                        @ quita los argumentos del stack
    pop  {r4, r5, r6, r7, r11, pc}      @ restaura los registers y vuelve

    .section .note.GNU-stack,"",%progbits
