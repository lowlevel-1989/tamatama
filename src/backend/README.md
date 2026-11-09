la plataforma (ventana, input y GL); el Makefile compila solo una de sus
carpetas. Solo lo que depende de la plataforma: por ejemplo, subir una
textura a GL. Cargar la imagen es cosa del juego (src/port/game/texture.c).
