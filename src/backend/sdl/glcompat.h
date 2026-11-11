/* backend/sdl/glcompat.h: el GL del backend SDL. Con ARENA_GLES (make
   GLES=1: Android, PortMaster) es GLES 1.1, el mismo que habla el juego;
   si no, OpenGL 2.1 de escritorio, cuya función fija se le parece mucho.
   Solo se usa lo que tienen los dos; lo que cambia de nombre va aquí. */
#pragma once
#ifdef ARENA_GLES
#include <GLES/gl.h>
#include <GLES/glext.h>
#define gl_ortho glOrthof
#else
#define GL_GLEXT_PROTOTYPES 1
#include <GL/gl.h>
#include <GL/glext.h>
#define gl_ortho glOrtho
#endif
