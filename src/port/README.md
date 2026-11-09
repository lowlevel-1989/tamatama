el código del juego a portear:
- app.c: app_main, que conduce el juego (init, step...).
- game/: el juego en C normal, sin nada de ARM. Solo puede incluir services.h,
  platform.h, backend.h y sus propios .h (lo comprueba el Makefile): así sirve
  tal cual cuando el juego sea nativo del todo.
- natives/: las funciones nativas que sustituyen funciones del .so y métodos de
  Java. Leen los registers de ARM (Regs) y llaman a game/. Van agrupadas por el
  trabajo que hacen: device.c (Device_PrintMessage), files.c (partidas y
  recursos), screen.c (la pantalla), textures.c (las texturas), time.c (la
  hora); el sonido irá en sound.c.
- port_table.h: qué funciones nativas hay. apk_table.h: las versiones
  compatibles del .apk. arm_java_table.h: los métodos de Java que atiende
  loader-arm.
