/* port/natives/java.c: el puente con el código ARM. Cada función traduce una llamada de Java: lee los
   argumentos de los registers, llama a tu código y deja el resultado en r0. */
#include "common.h"
#include "resource_manager.h"


int reload_counter = 0;

/* En Android cuenta las llamadas a onSurfaceChanged. Aquí la ventana solo la "cambia" una vez, al crearse. */
void native_OnGetReloadCounter(Regs *r) {
    r->r[0] = (uint32_t)reload_counter;
}

void native_OnInputStreamOpenLocalFile(Regs *r) {
    const JniObject *name = to_local(r->r[0]);
    r->r[0] = (uint32_t)resource_open_local((const char *)name->data);
}

void native_OnOutputStreamOpenLocalFile(Regs *r) {
    const JniObject *name = to_local(r->r[0]);
    r->r[0] = (uint32_t)resource_create_local((const char *)name->data);
}

void native_OnOutputStreamClose(Regs *r) {
    r->r[0] = (uint32_t)resource_close((int)r->r[0]);
}

void native_OnOutputStreamWrite(Regs *r) {
    int id = (int)r->r[0];
    const JniObject *array = to_local(r->r[1]);
    r->r[0] = (uint32_t)resource_write(id, array->data, (int)array->len);
}
