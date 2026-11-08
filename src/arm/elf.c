/* arm/elf.c: el ELF loader en sí. Coloca el .so en memoria y lo deja listo para ejecutarse. Solo
   se compila en loader-arm. */
#include "arm.h"

/* Las estructuras ELF de 32 bits, tal como aparecen en el fichero. */
typedef struct {
    uint8_t id[16];
    uint16_t type, machine;
    uint32_t version, entry, phoff, shoff, flags;
    uint16_t ehsize, phentsize, phnum, shentsize, shnum, shstrndx;
} Ehdr;
typedef struct {
    uint32_t type, offset, vaddr, paddr, filesz, memsz, flags, align;
} Phdr;
typedef struct {
    uint32_t name, value, size;
    uint8_t info, other;
    uint16_t shndx;
} Sym;
typedef struct {
    uint32_t offset, info;
} Rel;
typedef struct {
    int32_t tag;
    uint32_t val;
} Dyn;
/* Los tipos de segmento, de entrada dinámica y de relocation que usamos. */
enum { PT_LOAD = 1, PT_DYNAMIC = 2, PT_ARM_EXIDX = 0x70000001 };
enum {
    DT_NEEDED = 1,
    DT_PLTRELSZ = 2,
    DT_HASH = 4,
    DT_STRTAB = 5,
    DT_SYMTAB = 6,
    DT_INIT = 12,
    DT_REL = 17,
    DT_RELSZ = 18,
    DT_JMPREL = 23,
    DT_INIT_ARRAY = 25,
    DT_INIT_ARRAYSZ = 27
};
enum { R_ARM_ABS32 = 2, R_ARM_GLOB_DAT = 21, R_ARM_JUMP_SLOT = 22, R_ARM_RELATIVE = 23 };

static uint8_t *so_base;      /* dónde quedó el .so */
static uint32_t so_bytes;     /* cuánto ocupa */
/* La tabla .ARM.exidx: dice cómo deshacer cada función al propagar una excepción de C++. */
static uint32_t exidx_address, exidx_count;
static Sym *dynsym;        /* la tabla de símbolos dinámicos (dynsym) */
static const char *dynstr; /* sus nombres */
static uint32_t symbol_count;      /* cuántos símbolos hay */
static uint32_t init, init_array, init_arraysz;
/* Busca un símbolo definido en el .so. Una entrada con shndx 0 es una importación. */
static Sym *find_defined(const char *name) {
    for (uint32_t i = 1; i < symbol_count; i++)
        if (dynsym[i].shndx && !strcmp(dynstr + dynsym[i].name, name)) return &dynsym[i];
    return 0;
}
/* La dirección en memoria de un símbolo del .so, o 0 si no está. */
void *so_find(const char *name) {
    Sym *symbol = find_defined(name);
    return symbol ? so_base + symbol->value : 0;
}
/* La dirección que debe recibir el símbolo número idx. Aquí nace la intercepción: todo lo que
   el .so pide fuera de sí mismo pasa por resolve_import. */
static uint32_t symbol_value(uint32_t idx) {
    Sym *symbol = &dynsym[idx];
    const char *name = dynstr + symbol->name;
    if (!symbol->shndx) return (uint32_t)resolve_import(name);
    /* Definido en el .so. Si está sustituido, la PLT, la GOT y las vtables reciben el hook. */
    if (is_hooked(name)) return (uint32_t)hook_without_target(name);
    return (uint32_t)so_base + symbol->value;
}
/* Aplica una tabla de relocations. B es la base, A el valor que ya contiene la posición y S
   la dirección del símbolo. */
static void relocate(Rel *r, uint32_t bytes) {
    for (uint32_t i = 0; i < bytes / sizeof(Rel); i++) {
        uint32_t *p = (uint32_t *)(so_base + r[i].offset);
        uint32_t type = r[i].info & 0xFF, sym = r[i].info >> 8;
        switch (type) {
        case R_ARM_RELATIVE: *p += (uint32_t)so_base; break; /* B + A */
        case R_ARM_GLOB_DAT:
        case R_ARM_JUMP_SLOT: *p = symbol_value(sym); break; /* S     */
        case R_ARM_ABS32: *p += symbol_value(sym); break;    /* S + A */
        default:
            log_msg("[loader] relocation type %u at 0x%x\n", type, r[i].offset);
            die("unsupported relocation type");
        }
    }
}
/* Bytes que ocupa el salto al inicio de una función. El bit 0 de v dice el modo: 8 en ARM, 12
   en Thumb aligned a 4, y 14 si hace falta un nop delante. */
static uint32_t patch_size(uint32_t address) { return !(address & 1) ? 8 : (address & 2) ? 14 : 12; }

/* Sobrescribe el inicio de una función con un salto a su hook. Solo usa instrucciones que
   existen desde ARMv5T. La función original no se vuelve a ejecutar, así que no hace falta
   guardar lo que se pisa. */
static void write_patch(uint32_t address, void *target) {
    uint32_t a = address & ~1u;
    if (!(address & 1)) { /* función ARM */
        uint32_t *w = (uint32_t *)a;
        /* ldr pc, [pc, #-4]: pc vale la instrucción + 8, así que lee la palabra siguiente */
        w[0] = 0xE51FF004;
        w[1] = (uint32_t)target;
        return;
    }
    uint16_t *c = (uint16_t *)a;       /* función Thumb */
    if (a & 2) *c++ = 0x46C0;          /* nop: alinea a 4 bytes */
    c[0] = 0x4778;                     /* bx pc: pasa a modo ARM */
    c[1] = 0x46C0;                     /* nop                    */
    *(uint32_t *)(c + 2) = 0xE51FF004; /* ldr pc, [pc, #-4] */
    *(uint32_t *)(c + 4) = (uint32_t)target;
}
/* Pone el salto en una función del .so, si está ahí y el salto cabe. */
static void hook_function(const char *name, uint32_t given_address) {
    Sym *symbol = find_defined(name);
    uint32_t offset = given_address ? given_address : symbol ? symbol->value : 0;
    /* una importación o un método JNI: ya se encargan las relocations y RegisterNatives */
    if (!offset) return;
    uint32_t address = (uint32_t)so_base + offset, needed = patch_size(address), size = symbol ? symbol->size : 0;
    if (symbol && size < needed) {
        log_msg("[loader] %s is %u bytes and the patch %u: PLT, GOT and vtable only\n", name,
                size, needed);
        return;
    }
    void *hook = hook_without_target(name);
    write_patch(address, hook);
    log_msg("[loader] hook on %s, +0x%x, %u bytes, code at %p\n", name, offset, needed, hook);
}
/* Toda función con versión nativa, más los stubs del fichero de reglas. */
static void hook_replaced_functions(void) {
    for (int i = 0; native_table[i].name; i++)
        if (native_active(native_table[i].name) >= 0)
            hook_function(native_table[i].name, native_table[i].address);
    for (int i = 0; i < rule_count(); i++)
        if (rule_at(i)->mode != RULE_ARM)
            hook_function(rule_at(i)->name, rule_at(i)->address);
}
/* Carga el fichero y devuelve su dirección base. */
uint32_t elf_load(const char *path) {
    /* Paso 1: el fichero entero, en memoria temporal. */
    uint8_t *file = (uint8_t *)read_file(path);
    if (!file) die("cannot open the .so");
    Ehdr *elf_header = (Ehdr *)file;
    if (memcmp(elf_header->id, "\177ELF", 4) || elf_header->id[4] != 1) die("not a 32-bit ELF file");
    if (elf_header->machine != 40) die("not an ARM ELF file");

    /* Paso 2: reserva hasta el final del segmento más alto y copia cada PT_LOAD. La base sale
       de la arena: con el stack delante, 0x60000000 + 8 MiB = 0x60800000. El .so no depende de
       ella. */
    Phdr *segments = (Phdr *)(file + elf_header->phoff);
    uint32_t image_size = 0;
    for (int i = 0; i < elf_header->phnum; i++)
        if (segments[i].type == PT_LOAD && segments[i].vaddr + segments[i].memsz > image_size)
            image_size = segments[i].vaddr + segments[i].memsz;
    image_size = (image_size + 0xFFF) & ~0xFFFu;
    so_base = arena_take(image_size, 4096);
    so_bytes = image_size;
    Dyn *dyn = 0;
    for (int i = 0; i < elf_header->phnum; i++) {
        if (segments[i].type == PT_LOAD)
            memcpy(so_base + segments[i].vaddr, file + segments[i].offset, segments[i].filesz);
        if (segments[i].type == PT_DYNAMIC) dyn = (Dyn *)(so_base + segments[i].vaddr);
        if (segments[i].type == PT_ARM_EXIDX) {
            exidx_address = (uint32_t)so_base + segments[i].vaddr;
            exidx_count = segments[i].memsz / 8; /* cada entrada ocupa 8 bytes */
        }
    }
    log_msg("[loader] %s: base=%p, %u bytes\n", path, so_base, image_size);
    if (!dyn) die("the .so has no dynamic section");

    /* Paso 3: lee la sección dinámica. */
    Rel *rel = 0, *jmp = 0;
    uint32_t relsz = 0, jmpsz = 0;
    for (Dyn *dyn_entry = dyn; dyn_entry->tag; dyn_entry++) switch (dyn_entry->tag) {
        case DT_STRTAB: dynstr = (char *)(so_base + dyn_entry->val); break;
        case DT_SYMTAB: dynsym = (Sym *)(so_base + dyn_entry->val); break;
        case DT_HASH: symbol_count = ((uint32_t *)(so_base + dyn_entry->val))[1]; break;
        case DT_REL: rel = (Rel *)(so_base + dyn_entry->val); break;
        case DT_RELSZ: relsz = dyn_entry->val; break;
        case DT_JMPREL: jmp = (Rel *)(so_base + dyn_entry->val); break;
        case DT_PLTRELSZ: jmpsz = dyn_entry->val; break;
        case DT_INIT: init = dyn_entry->val; break;
        case DT_INIT_ARRAY: init_array = dyn_entry->val; break;
        case DT_INIT_ARRAYSZ: init_arraysz = dyn_entry->val; break;
        }
    if (!symbol_count) die("the .so has no DT_HASH");
    for (Dyn *dyn_entry = dyn; dyn_entry->tag; dyn_entry++)
        if (dyn_entry->tag == DT_NEEDED) log_msg("[loader] needs %s\n", dynstr + dyn_entry->val);

    /* Paso 4: reubica y pone los hooks de las funciones que ya tienen sustituta. */
    if (rel) relocate(rel, relsz);
    if (jmp) relocate(jmp, jmpsz);
    hook_replaced_functions();

    /* Paso 5: los permisos finales de cada segmento. ELF y mprotect usan los mismos tres bits,
       en orden inverso. */
    flush_code(so_base, image_size);
    for (int i = 0; i < elf_header->phnum; i++)
        if (segments[i].type == PT_LOAD) {
            uint32_t start = segments[i].vaddr & ~0xFFFu,
                     end = (segments[i].vaddr + segments[i].memsz + 0xFFF) & ~0xFFFu;
            int prot = ((segments[i].flags & 4) ? 1 : 0) | ((segments[i].flags & 2) ? 2 : 0) |
                       ((segments[i].flags & 1) ? 4 : 0);
            mprotect(so_base + start, end - start, prot);
        }
    free(file);
    return (uint32_t)so_base;
}
/* 1 si la dirección está dentro del .so, y entonces escribe su posición. */
int elf_position(uint32_t address, uint32_t *position) {
    *position = address - (uint32_t)so_base;
    return *position < so_bytes;
}
/* El nombre de la función que contiene esta posición, o NULL. Escribe también dónde empieza. Solo
   conoce los símbolos dinámicos, porque .symtab no se carga en memoria. */
const char *elf_function_at(uint32_t position, uint32_t *start) {
    const Sym *best = 0;
    for (uint32_t i = 1; i < symbol_count; i++) {
        const Sym *symbol = &dynsym[i];
        uint32_t value = symbol->value & ~1u; /* el bit 0 indica Thumb */
        if (!symbol->shndx || (symbol->info & 15) != 2 || value > position) continue;
        if (!best || value > (best->value & ~1u)) best = symbol;
    }
    if (!best) return 0;
    *start = best->value & ~1u;
    if (best->size && position >= *start + best->size) return 0;
    return dynstr + best->name;
}
/* Escribe una dirección y, si está dentro del .so, su posición y su función:
   "0x6082d1e3 (@0x2d1e2 _JNIEnv::CallStaticIntMethod+0x16)". La posición no lleva el bit de Thumb,
   así que coincide con el disassembly. */
void elf_format_address(uint32_t address, char *text, size_t room) {
    uint32_t position;
    if (!elf_position(address, &position)) {
        snprintf(text, room, "0x%x", address);
        return;
    }
    char name[128];
    position &= ~1u;
    describe_position(position, name, sizeof name);
    snprintf(text, room, "0x%x (@0x%x %s)", address, position, name);
}
/* __gnu_Unwind_Find_exidx: el código de excepciones de C++ pregunta por la tabla de la library
   que contiene esta dirección. Solo hay una library ARM: el .so. */
void *elf_find_exidx(uint32_t pc, int *count) {
    if (!exidx_count || pc - (uint32_t)so_base >= so_bytes) {
        *count = 0;
        return 0;
    }
    *count = (int)exidx_count;
    return (void *)exidx_address;
}
/* Paso 6: los constructores globales de C++. Se saltan 0 y 0xFFFFFFFF, que algunas toolchains
   usan como marcas. */
void elf_run_constructors(void) {
    if (init) ((void (*)(void))(so_base + init))();
    for (uint32_t i = 0; i < init_arraysz / 4; i++) {
        uint32_t f = ((uint32_t *)(so_base + init_array))[i];
        if (f && f != 0xFFFFFFFFu) ((void (*)(void))f)();
    }
}
