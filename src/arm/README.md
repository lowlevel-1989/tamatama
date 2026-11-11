loader-arm: carga el .so, pone los hooks y ejecuta el código ARM del juego (bajo
QEMU en el PC, directamente en la Raspberry Pi). trap.c dice qué instrucción
escribe una variable: la regla trap de doc/debug.es.md.
