/* port/game/texture.h: las texturas del juego, tal como las espera el .so:
   cada una es un número desde 1, con su nombre en OpenGL, su ancho y su alto.
   Con un id no válido, las consultas devuelven -1. */
#pragma once

/* carga la textura por el nombre que pide el .so; devuelve su id, o -1 */
int texture_load(const char *name);
int texture_gl_name(int id); /* su nombre en OpenGL */
int texture_width(int id);   /* en píxeles */
int texture_height(int id);
/* borra la textura y deja libre su id; 0 si el id no es válido */
int texture_remove(int id);
