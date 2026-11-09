/* backend/sdl/gl.c: una referencia de cómo traducir las funciones GLES1 que
   llama el .so a la OpenGL de tu PC, con un ejemplo de cada tipo de argumento.
   Para añadir otra: copia el ejemplo que se le parezca y añade su línea a
   backend/gl_table.h. Las que falten salen en el informe como
   UNIMPLEMENTED. */
#define GL_GLEXT_PROTOTYPES 1
#include <GL/gl.h>
#include <GL/glext.h>
#include "common.h"

/* Los argumentos de GLES1 ocupan una palabra cada uno: los cuatro primeros van
   en r0-r3 y los demás en el stack, así que basta con su posición. */
static uint32_t arg_word(const Regs *r, int i) { return i < 4 ? r->r[i] : stack_arg(r, i - 4); }
static float arg_float(const Regs *r, int i) { return bits_to_float(arg_word(r, i)); }
/* Punto fijo 16.16: 65536 significa 1,0. */
static float arg_fixed(const Regs *r, int i) { return (float)(int32_t)arg_word(r, i) / 65536.0f; }
/* Una dirección de la arena se convierte en un pointer de aquí. Cualquier otro
   valor se pasa tal cual, porque con un VBO activo es un offset, y 0 significa
   NULL. */
static void *arg_ptr(const Regs *r, int i) {
    uint32_t address = arg_word(r, i);
#ifdef __arm__
    return (void *)address;
#else
    return address - arena_arm < ARENA_BYTES ? to_local(address) : (void *)(uintptr_t)address;
#endif
}
/* OpenGL de escritorio no admite arrays GL_FIXED. Se avisa una vez, porque no
   dibujarían nada. */
static void check_array_type(uint32_t type) {
    static int warned;
    if (type == 0x140C && !warned) {
        warned = 1;
        log_msg("[gl] GL_FIXED arrays are not supported by desktop OpenGL\n");
    }
}


void native_glEnable(Regs *r) { glEnable(arg_word(r, 0)); }
void native_glEnableClientState(Regs *r) { glEnableClientState(arg_word(r, 0)); }
void native_glGetString(Regs *r) {
    const char *text = (const char *)glGetString(arg_word(r, 0));
    if (!text) {
        r->r[0] = 0;
        return;
    }
    uint32_t bytes = (uint32_t)strlen(text) + 1;
    arm_ptr copy = shared_alloc(bytes);
    memcpy(to_local(copy), text, bytes);
    r->r[0] = copy;
}
void native_glFlush(Regs *r) { glFlush(); }

void native_glClear(Regs *r) { glClear(arg_word(r, 0)); }
void native_glClearColor(Regs *r) { glClearColor(arg_float(r, 0), arg_float(r, 1), arg_float(r, 2), arg_float(r, 3)); }
void native_glDrawArrays(Regs *r) { glDrawArrays(arg_word(r, 0), arg_word(r, 1), arg_word(r, 2)); }
void native_glVertexPointer(Regs *r) { check_array_type(arg_word(r, 1)); glVertexPointer(arg_word(r, 0), arg_word(r, 1), arg_word(r, 2), arg_ptr(r, 3)); }
void native_glColorPointer(Regs *r) { check_array_type(arg_word(r, 1)); glColorPointer(arg_word(r, 0), arg_word(r, 1), arg_word(r, 2), arg_ptr(r, 3)); }

void native_glTranslatef(Regs *r) { glTranslatef(arg_float(r, 0), arg_float(r, 1), arg_float(r, 2)); }
void native_glTranslatex(Regs *r) { glTranslatef(arg_fixed(r, 0), arg_fixed(r, 1), arg_fixed(r, 2)); }
void native_glViewport(Regs *r) { glViewport(arg_word(r, 0), arg_word(r, 1), arg_word(r, 2), arg_word(r, 3)); }

void native_glGenTextures(Regs *r) { glGenTextures(arg_word(r, 0), arg_ptr(r, 1)); }
void native_glTexImage2D(Regs *r) { glTexImage2D(arg_word(r, 0), arg_word(r, 1), arg_word(r, 2), arg_word(r, 3), arg_word(r, 4), arg_word(r, 5), arg_word(r, 6), arg_word(r, 7), arg_ptr(r, 8)); }
void native_glTexCoordPointer(Regs *r) { check_array_type(arg_word(r, 1)); glTexCoordPointer(arg_word(r, 0), arg_word(r, 1), arg_word(r, 2), arg_ptr(r, 3)); }
void native_glDisable(Regs *r) { glDisable(arg_word(r, 0)); }
void native_glDisableClientState(Regs *r) { glDisableClientState(arg_word(r, 0)); }
void native_glTexParameterf(Regs *r) { glTexParameterf(arg_word(r, 0), arg_word(r, 1), arg_float(r, 2)); }
void native_glBlendFunc(Regs *r) { glBlendFunc(arg_word(r, 0), arg_word(r, 1)); }
void native_glBindTexture(Regs *r) { glBindTexture(arg_word(r, 0), arg_word(r, 1)); }

void native_glScalef(Regs *r) { glScalef(arg_float(r, 0), arg_float(r, 1), arg_float(r, 2)); }
void native_glRotatef(Regs *r) { glRotatef(arg_float(r, 0), arg_float(r, 1), arg_float(r, 2), arg_float(r, 3)); }
void native_glColor4f(Regs *r) { glColor4f(arg_float(r, 0), arg_float(r, 1), arg_float(r, 2), arg_float(r, 3)); }
void native_glPushMatrix(Regs *r) { (void)r; glPushMatrix(); }
void native_glPopMatrix(Regs *r) { (void)r; glPopMatrix(); }
