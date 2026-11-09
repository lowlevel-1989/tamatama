/* port/natives/textures.c: las texturas, desde Java. Leen los argumentos de los
   registers, llaman a game/texture.c y dejan el resultado en r0. */
#include "common.h"
#include "texture.h"

/* El id de la textura, desde 1, o -1 si no se pudo cargar. Un nombre con @
   delante no es un fichero sino un texto, que hay que dibujar con una fuente:
   de momento no se hace. */
void native_OnLoadTexture(Regs *r) {
    const char *name = (const char *)((const JniObject *)to_local(r->r[0]))->data;
    if (name[0] == '@') {
        log_msg("[texture] %s: text textures are not supported yet\n", name);
        r->r[0] = (uint32_t)-1;
        return;
    }
    r->r[0] = (uint32_t)texture_load(name);
}

/* Su nombre en OpenGL, el que el .so usa con glBindTexture; -1 si el id no es
   válido. */
void native_OnGetTextureID(Regs *r) {
    r->r[0] = (uint32_t)texture_gl_name((int)r->r[0]);
}

/* El ancho, en píxeles; -1 si el id no es válido. */
void native_OnGetTextureWidth(Regs *r) {
    r->r[0] = (uint32_t)texture_width((int)r->r[0]);
}

/* El alto, en píxeles; -1 si el id no es válido. */
void native_OnGetTextureHeight(Regs *r) {
    r->r[0] = (uint32_t)texture_height((int)r->r[0]);
}

/* Quita la textura: la borra en GL y deja libre su id para la siguiente que se
   cargue. Devuelve true, o false si el id no es válido. */
void native_OnRemoveTexture(Regs *r) {
    r->r[0] = (uint32_t)texture_remove((int)r->r[0]);
}
