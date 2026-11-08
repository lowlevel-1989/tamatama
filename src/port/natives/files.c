/* port/natives/files.c: los ficheros del juego, desde Java. Las partidas que
   guarda (OpenLocalFile) y los recursos del .apk (OpenResourceFile), abiertos
   con el gestor de resources. Cada función lee los argumentos de los registers,
   llama a game/resource_manager.c y deja el resultado en r0. */
#include "common.h"
#include "resource_manager.h"

void native_OnInputStreamOpenLocalFile(Regs *r) {
    const JniObject *name = to_local(r->r[0]);
    r->r[0] = (uint32_t)resource_open_local((const char *)name->data);
}

/* Un recurso del juego: un asset del .apk. Devuelve su id, o -1. */
void native_OnInputStreamOpenResourceFile(Regs *r) {
    const JniObject *name = to_local(r->r[0]);
    r->r[0] = (uint32_t)resource_open_asset((const char *)name->data);
}

/* Lee hasta size bytes del id y los devuelve en un byte[] nuevo, del tamaño de
   lo leído. Si el id no es válido, devuelve null. */
void native_OnInputStreamRead(Regs *r) {
    int id = (int)r->r[0], size = (int)r->r[1];
    if (size < 0) size = 0;
    /* java_bytes es el byte[] tal como lo ve el juego; bytes, el mismo objeto
       visto desde aquí, para rellenarlo. */
    arm_ptr java_bytes = shared_alloc((uint32_t)(sizeof(JniObject) + size + 1));
    JniObject *bytes = to_local(java_bytes);
    int got = resource_read(id, bytes->data, size);
    if (got < 0) {
        r->r[0] = 0;
        return;
    }
    bytes->len = (uint32_t)got;
    bytes->dirty = 0;
    bytes->data[got] = 0;
    r->r[0] = java_bytes;
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
    const JniObject *bytes = to_local(r->r[1]); /* el byte[] que llega */
    r->r[0] = (uint32_t)resource_write(id, bytes->data, (int)bytes->len);
}
