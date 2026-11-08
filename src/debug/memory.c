/* debug/memory.c: leer la memoria del juego por tipo y por lugar. */
#include "memory.h"

static const char *const type_names[] = {"i8", "u8", "i16", "u16", "i32", "u32", "f32", "ptr", "str"};

int value_type_parse(const char *text, ValueType *type) {
    for (int i = 0; i <= TYPE_STR; i++)
        if (!strcmp(text, type_names[i])) {
            *type = (ValueType)i;
            return 1;
        }
    return 0;
}
int value_bytes(ValueType type) {
    switch (type) {
    case TYPE_I16:
    case TYPE_U16: return 2;
    case TYPE_I32:
    case TYPE_U32:
    case TYPE_F32:
    case TYPE_PTR: return 4;
    default: return 1;
    }
}
void value_format(ValueType type, arm_ptr address, char *text, size_t room) {
    const void *p = to_local(address);
    int8_t i8;
    uint8_t u8;
    int16_t i16;
    uint16_t u16;
    int32_t i32;
    uint32_t u32;
    float f32;
    switch (type) {
    case TYPE_I8: memcpy(&i8, p, 1); snprintf(text, room, "%d", i8); break;
    case TYPE_U8: memcpy(&u8, p, 1); snprintf(text, room, "%u", u8); break;
    case TYPE_I16: memcpy(&i16, p, 2); snprintf(text, room, "%d", i16); break;
    case TYPE_U16: memcpy(&u16, p, 2); snprintf(text, room, "%u", u16); break;
    case TYPE_I32: memcpy(&i32, p, 4); snprintf(text, room, "%d", i32); break;
    case TYPE_U32: memcpy(&u32, p, 4); snprintf(text, room, "%u", u32); break;
    case TYPE_F32: memcpy(&f32, p, 4); snprintf(text, room, "%g", (double)f32); break;
    case TYPE_PTR: memcpy(&u32, p, 4); snprintf(text, room, "0x%x", u32); break;
    case TYPE_STR: snprintf(text, room, "\"%.48s\"", (const char *)p); break;
    }
}
/* Dónde empieza un lugar: lo que hay antes del primer + o ->. */
arm_ptr place_start(const char *place) {
    if (place[0] == '@') return arm_so_base() + parse_number(place + 1);
    if (place[0] == '#') return parse_number(place + 1);
    if (place[0] == '0' && place[1] == 'x') return parse_number(place);
    char name[256];
    snprintf(name, sizeof name, "%.*s", (int)strcspn(place, "+-"), place);
    const char *own = symbol_place(name); /* los tuyos, y después los que exporta el .so */
    return own ? place_resolve(own) : arm_symbol(name);
}
/* Recorre el resto del lugar desde address, igual que en C:
     +0x18    suma 0x18
     -0x18    resta 0x18
     ->0x18   lee un pointer en la dirección actual y le suma 0x18
   Devuelve 0 si algún pointer no se puede leer. */
arm_ptr place_follow(arm_ptr address, const char *place) {
    if (!address) return 0; /* un símbolo que aún no existe: se volverá a preguntar */
    for (const char *p = place + strcspn(place, "+-"); *p; p += strcspn(p + 1, "+-") + 1) {
        if (*p == '+') {
            address += parse_number(p + 1);
        } else if (p[1] != '>') {
            address -= parse_number(p + 1);
        } else {
            if (!arm_readable(address)) return 0;
            uint32_t pointer;
            memcpy(&pointer, to_local(address), 4);
            address = pointer + parse_number(p + 2);
        }
    }
    return address;
}
arm_ptr place_resolve(const char *place) { return place_follow(place_start(place), place); }
