/* shared/native_table.c:
 * construye native_table, la lista de todo lo que ya existe en código nativo. 
 *
 * Une dos listas: la tuya (port/port_table.h), si existe,
 * y la de GL (backend/gl_table.h). 
 *
 * La tuya va primero: la búsqueda se queda con la primera 
 * coincidencia, así que si escribes tu propio glClear, se
 * usa el tuyo.
 *
 * Cada línea de esas listas se expande dos veces: 
 * primero declara la función y después crea su 
 * entrada en la tabla. */
#include "common.h"

/* 
 * En la primera pasada se define solo la función asi:
 *   - NATIVE("sum", native_sum)
 *   - void native_sum(Regs *r);
 *
 * En la segunda pasada se crea la entrada en la tabla:
 *   const NativeEntry native_table[] = {
    {"sum", native_sum},
    ...
 *
 * Primera pasada: declara las funciones. 
 * */
#define NATIVE(name, fn) void fn(Regs *r);
#define NATIVE_AT(name, fn, address) void fn(Regs *r);
#if __has_include("port_table.h")
#include "port_table.h"
#endif
#include "gl_table.h"
#undef NATIVE
#undef NATIVE_AT

/* Segunda pasada: crea las entradas. */
#define NATIVE(name, fn) {name, fn},
#define NATIVE_AT(name, fn, address) {name, fn, address},
const NativeEntry native_table[] = {
#if __has_include("port_table.h")
#include "port_table.h"
#endif
#include "gl_table.h"
    {0}};
