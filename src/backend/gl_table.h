/* backend/gl_table.h: las funciones GL con versión nativa. Es la misma lista
   para todos los backends, así que loader y loader-arm tienen cada función en
   la misma posición. Si un backend no define alguna, backend/gl_defaults.c da
   una versión vacía. Lo incluyen varios ficheros, cada uno con su
   NATIVE, así que no lleva #pragma once. Añade aquí las que use tu juego. */
NATIVE("glEnable", native_glEnable)
NATIVE("glEnableClientState", native_glEnableClientState)
NATIVE("glGetString", native_glGetString)
NATIVE("glFlush", native_glFlush)
NATIVE("glClear", native_glClear)
NATIVE("glClearColor", native_glClearColor)
NATIVE("glDrawArrays", native_glDrawArrays)
NATIVE("glVertexPointer", native_glVertexPointer)
NATIVE("glColorPointer", native_glColorPointer)
NATIVE("glTranslatef", native_glTranslatef)
NATIVE("glTranslatex", native_glTranslatex)
NATIVE("glViewport", native_glViewport)
NATIVE("glGenTextures", native_glGenTextures)
NATIVE("glTexImage2D", native_glTexImage2D)
