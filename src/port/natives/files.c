/* port/natives/files.c: los ficheros del juego, desde Java. Las partidas que
   guarda (OpenLocalFile) y los recursos del .apk (OpenResourceFile), abiertos
   con el gestor de resources. Cada función lee los argumentos de los registers,
   llama a game/resource_manager.c y deja el resultado en r0. */
#include "common.h"
#include "resource_manager.h"
#include "controls.h"

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

/* Salta count bytes del id sin leerlos. Devuelve los saltados, o -1. */
void native_OnInputStreamSkip(Regs *r) {
    r->r[0] = (uint32_t)resource_skip((int)r->r[0], (int)r->r[1]);
}

/* Un fichero entero en un byte[], sin abrirlo con un id: nombre, offset y
   tamaño. Tamaño 0 es hasta el final, como pide el juego con
   OnReadFile("tc_logo_str.csv", 0, 0). Si el fichero no existe, devuelve null. */
void native_OnReadFile(Regs *r) {
    const JniObject *name = to_local(r->r[0]);
    int offset = (int)r->r[1], size = (int)r->r[2];
    int total = resource_file_size((const char *)name->data);
    if (total < 0 || offset < 0 || offset > total) {
        log_msg("[resource] read %s: not found\n", (const char *)name->data);
        r->r[0] = 0;
        return;
    }
    if (size <= 0 || size > total - offset) size = total - offset;
    /* java_bytes es el byte[] tal como lo ve el juego; bytes, el mismo objeto
       visto desde aquí, para rellenarlo. */
    arm_ptr java_bytes = shared_alloc((uint32_t)(sizeof(JniObject) + size + 1));
    JniObject *bytes = to_local(java_bytes);
    int got = resource_read_file((const char *)name->data, offset, bytes->data, size);
    /* Los CSV de botones dicen al mando en qué pantalla está. */
    if (got > 0) controls_csv_read((const char *)name->data, (const char *)bytes->data, got);
    bytes->len = got > 0 ? (uint32_t)got : 0;
    bytes->dirty = 0;
    bytes->data[bytes->len] = 0;
    r->r[0] = java_bytes;
}

/* Cierra un fichero abierto para leer: una partida o un recurso. */
void native_OnInputStreamClose(Regs *r) {
    r->r[0] = (uint32_t)resource_close((int)r->r[0]);
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

/* La partida de Angel y L.i.f.e.: en vez de los streams, un fichero entero de
   golpe. OnReadGameData(nombre, tamaño) devuelve sus bytes en un byte[], o
   null si aún no hay partida (el juego crea una nueva). */
void native_OnReadGameData(Regs *r) {
    const JniObject *name = to_local(r->r[0]);
    int size = (int)r->r[1];
    int id = resource_open_local((const char *)name->data);
    if (id < 0 || size < 0) {
        if (id >= 0) resource_close(id);
        r->r[0] = 0;
        return;
    }
    /* java_bytes es el byte[] tal como lo ve el juego; bytes, el mismo objeto
       visto desde aquí, para rellenarlo. */
    arm_ptr java_bytes = shared_alloc((uint32_t)(sizeof(JniObject) + size + 1));
    JniObject *bytes = to_local(java_bytes);
    int got = resource_read(id, bytes->data, size);
    resource_close(id);
    if (got < 0) {
        r->r[0] = 0;
        return;
    }
    bytes->len = (uint32_t)got;
    bytes->dirty = 0;
    bytes->data[got] = 0;
    r->r[0] = java_bytes;
}

/* OnWriteGameData(nombre, byte[]): guarda el fichero entero. Devuelve true si
   se ha escrito todo. */
void native_OnWriteGameData(Regs *r) {
    const JniObject *name = to_local(r->r[0]);
    const JniObject *bytes = to_local(r->r[1]);
    int id = resource_create_local((const char *)name->data);
    int wrote = id < 0 ? -1 : resource_write(id, bytes->data, (int)bytes->len);
    int closed = id < 0 ? -1 : resource_close(id);
    r->r[0] = wrote == (int)bytes->len && closed >= 0;
}
