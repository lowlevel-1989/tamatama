#!/usr/bin/env python3
"""tools/clean_pseudo.py: limpia el pseudocode de r2ghidra: pone nombre a las llamadas y a las
variables globales, y resuelve los strings. Lee el pseudocode por la entrada estándar.

  r2 -q -e scr.color=0 -a arm -b 16 -c 'aaa; s 0x2d410; pdg; s 0x2ef74; pdg; s 0x2cf44; pdg; s 0x3f460; pdg; s 0x3f408; pdg; s 0x3ec6c; pdg; s 0x3ec10; pdg; s 0x3ec10: pdg; s 0x3f698; pdg; s 0x51664; pdg; s 0x51651; pdg; s 0x44d4c; pdg; s 0x44d40; pdg; s 0x44d00; pdg; s 0x3f1c4; pdg; s 0x3f1a8; pdg; s 0x3ed40; pdg; s 0x515c4; pdg; s 0x51648; pdg; s 0x516f0; pdg; s 0x506b8; pdg; s 0x506d8; pdg; s 0x52bb8; pdg; s 0x52b18; pdg; s 0x3d560; pdg; s 0x2f5e0; pdg; s 0x38928; pdg; s 0x38908; pdg; s 0x32e90; pdg; s 0x44cec; pdg; s 0x2f684; pdg; s 0x32e80; pdg; s 0x32d20; pdg; s 0x32880; pdg; s 0x2f628; af; pdg; s 0x3be88; pdg; s 0x3bd64; pdg; s 0x320f4; pdg; s 0x320b0; pdg; s 0x30f40; pdg; s 0x3b0a0; pdg; s 0x3bc7c; pdg; s 0x3b184; pdg; s 0x38e48; pdg; s 0x38bc4; pdg' apk/lib/armeabi/libAndroidGame001JNI.so | python tools/clean_pseudo.py apk/lib/armeabi/libAndroidGame001JNI.so --rules rules/bringup.txt | tee decode.txt

Si hay un rules.txt en esta carpeta, usa sus reglas symbol. Con --rules eliges otro fichero.
"""
# Lo que hace:
#   (**(*x + 0x1c4))(x,a,b)          pasa a  env->GetStaticMethodID(a,b), si la función usa JNI
#   (**(*x + 0x24))(x,a)             pasa a  x->vcall_0x24(a), un método virtual, si no la usa
#   (**(*x + 0x28))(x,a,extraout_r2,*(*x + 0x28),in_r3)  pasa a  x->vcall_0x28(a), sin los restos
#   f(uint noname_0,int16_t arg1) con //A::f(char const*)  pasa a  A::f(char const* arg1)
#   *0x2e7cc + 0x2e76a               pasa a  "OnOutputStreamWrite", si ahí hay un texto
#   *0x52b40 + 0x52b2a               pasa a  &DAT_c2d64, la dirección de una global
#   f(x, extraout_s1)                pasa a  f(x, /* r1 */): un .so sin VFP no tiene s1; se lee r1 de las instrucciones
#   varias llamadas a f              sus argumentos se leen por orden, si están en el nivel de arriba y sin saltos
#   v = &g; f(v); v[1] = 0           pasa a  f(&g); g.field_0x4 = 0, aunque v se reutilice para otra global
#   String::operator+(&local_24, &a, &b)  pasa a  string local_24 = a + b, si se sabe la clase de local_24
#   *0x2f6c8 + -0x948                igual, con el pc entero (0x2f6b8): Ghidra a veces deja solo sus 16 bits bajos
#   *(*0x52b40 + 0x52b2a)            pasa a  DAT_c2d64, su valor
#   *(iVar6 + *0x32f78)              pasa a  &DAT_c00c0: una global, leída a través de la GOT
#   method.Java::Out.write_int_(     pasa a  Out::write(
#   sym.Java::Out::_Out____1(        pasa a  Out::~Out(
#   pcVar1 = *0x9f938; (*pcVar1)(a)  pasa a  __kuser_cmpxchg(a), una función del kernel
#   iVar1 = &stack0xfffffff0; ... iVar1 + -8    pasa a  &local_18
#   iStack_18 y &stack0xffffffe8     pasan a  local_18 y &local_18
#   *0x515f0                         pasa a  0.7f, o a 0x10004: la constante que hay en esa dirección
#   iVar1 = *0x517a4; ... iVar1 + 0x5170c      pasa a  &DAT_c2d50, si se suman el literal y su pc
#   sym.Gamedata_SetVolume_float_()  pasa a  Gamedata_SetVolume(/* float */): falta un argumento
#   y si ese register no se ha tocado, pasa a  Gamedata_SetVolume(arg1 /* r0 */)
#   iVar2 = arg1; ... f(iVar2)       pasa a  f(arg1), si ninguna de las dos cambia
#   piVar1 = f(); piVar1->g();       pasa a  f()->g(), si piVar1 no se usa más
#   __gnu_thumb1_case_sqi()          pasa a  un comentario, y se quitan los restos de VFP de la tabla
#   if (5 < x)                       pasa a  if (x > 5)
#   code_r0x0005171e y joined_r0x00038f1a   pasan a  L_5171e y L_38f1a
#   if ((x + -0xc != &vacia) && ...) el destructor de std::string, expandido, pasa a  x.~string()
#   x = &vtable for A + 8; B::~B(&x) un destructor expandido, pasa a  x.~A()
#   p = &t; e = p + 0x40; do {...}   un recorrido con pointers, pasa a  for (int i...) con t[i]
#   A::A(&local_1c, a)               pasa a  A local_1c(a), si se sabe la firma
#   A::m(&local_1c, a)               pasa a  local_1c.m(a)
#   B::f(&local_18, a), como primer uso, pasa a  A local_18 = B::f(a): devuelve el objeto
#   Clase::m_x, dentro de Clase      pasa a  m_x
#   f(/* char const* */)             pasa a  f("texto"), si lo dicen las instrucciones Thumb del .so
#   f(uint noname_0) con in_r0       pasa a  f(uint arg0): in_r0 es el mismo register que ese parámetro
#   vm->AttachCurrentThread(&local_1c, 0)    local_1c pasa a  JNIEnv *env, y vm a  JavaVM *vm y g_vm
#   __aeabi_f2d(x); env->CallStaticVoidMethod()    pasa a  env->CallStaticVoidMethod(clase, metodo, (double)x)
#   __aeabi_fmul(a, 100.0f)          pasa a  a * 100.0f, y __fixsfsi(x) a  (int)x
#   __aeabi_fcmplt(a, b)             pasa a  a < b
#   __aeabi_idivmod(a, b); x = extraout_r1;    pasa a  x = a % b, y v = __aeabi_idiv(a, b) a  v = a / b
#   __aeabi_ldivmod(g, (&g)[1])      pasa a  g / 0x3e8: el divisor va en r2:r3 y se lee de las instrucciones
#   ((x >> 0x1f) >> 0x11) + x >> 0xf pasa a  x / 0x8000: la división con signo entre una potencia de 2
#   v = a < b; if (v == 0) {         pasa a  if (a >= b) {
#   arg1_01 = arg1                   arg1_01 es arg1 con otro nombre
#   Camera::f(x, a), con f(float)    pasa a  x->f(a): sobra un argumento, así que es el this
#   (***x)()                         pasa a  x->vcall_0x0(), la primera entrada de la vtable
#   (**(*x + 4))()                   pasa a  x->vcall_0x4(): Ghidra escribe en decimal los menores de 10
#   &local_18 + -0x14                pasa a  &local_2c
#   int16_t arg1                     pasa a  int arg1: Ghidra adivina, y en ARM los registers son de 32 bits
#   p = &DAT_bc6fc; **(p + 0x4bc)    pasa a  la global a la que apunta esa entrada de la GOT
#   v = 60.0f; *(this + 0xc) = v     pasa a  this->field_0xc = 60.0f
#   v = g; *(v + 0x20) = *(v + 0x20) + 1    pasa a  g->field_0x20 += 1
#   ftime(&t); srand48(local_18)     pasa a  struct timeb t; srand48(t.millitm)
#   iVar3 = (float)a; ... iVar3 = 5  pasa a  fVar3 = (float)a; ... iVar3 = 5: un register, dos variables
#   Camera *arg1_00                  pasa a  Camera *camera, si no hay otra con ese nombre
#   las líneas que Ghidra parte se unen, y todo se vuelve a sangrar según sus llaves
#   p = &g; *p = 1                   pasa a  g = 1
#   p = &g; ... p[4], con saltos y loops   pasa a  g[4] si p se copia o recorre g, y si no a  g.field_0x10
#   v = (&g)[1]; h.field_0x0 = g; h.field_0x4 = v   pasa a  h = g: los 8 bytes de un long long, mitad a mitad
#   *(&g + 0xc) y &g + 0x18          pasan a  g.field_0xc y &g.field_0x18: cuentan bytes, no elementos
#   *(&g + i * 8 + 4)                pasa a  g[i].field_0x4, también con v = i * 8; *(&g + v) y v = &g + i * 8; *(v + 4)
#   g.field_0x44, si g tiene elementos de 0x24 bytes    pasa a  g[1].field_0x20
#   local_38 = arg1; g = local_38;   pasa a  g = arg1, si nadie toma la dirección de local_38
#   -(c) | 0xb4                      pasa a  (c) ? -1 : 0xb4
#   iVar2 = 0x248; ... *(iVar3 + iVar2)    la asignación desaparece cuando ya se sabe qué global es
#   printf("x=%d")                   pasa a  printf("x=%d", /* r1 */): el formato pide un valor que Ghidra no enseña
#   uint32_t arg1_01, sin ser parámetro     pasa a  uVar18, como las demás variables
#   0x41c64e6d * x + 0x3039          pasa a  x * 0x41c64e6d + 0x3039, y 0xfff0fff7 & x a  x & 0xfff0fff7
#   v = f(); g = v; if (v != 0)      pasa a  g = f(); if (g != 0)
#   v = "(I)V"; ... f(v)             pasa a  f("(I)V")
#   memcpy(&t, "texto", 0x34); f(&t) pasa a  f("texto")
#   la comprobación de __stack_chk_guard, que añade el compilador, desaparece
#   if (a) { if (b) { if (c) {...}}} pasa a  if (!a) goto L_done; if (!b) goto L_done; ...
#   sym.A::f(x)                      lleva encima  // @0x52a10  A::f(int), para seguir con pdg @ 0x52a10
#   DAT_c2d4c y fcn.0002d2e8         pasan al nombre de symbol en rules.txt, con la dirección encima
#   loc.imp.memcpy, class.Stream y method.const.A.m___const    pasan a  memcpy, Stream::Stream y A::m
#   f(noname_1, x) con noname_1 sin asignar    pasa a  f(/* r1 */, x), o al valor si lo dicen las instrucciones
#   (uint noname_0, int arg1)        pasa a  (uint arg0, uint arg1): el nombre dice en qué register entró
#   printf(f, v, v >> 0x20)          pasa a  printf(f, v): las dos mitades de un double
#   p = this; p[7] = 1               pasa a  this->field_0x1c = 1: p[7] cuenta palabras, y el campo, bytes
#   *(*(x + 0x8c) + 8)               pasa a  x->field_0x8c->field_0x8
#   this + arg2 * 0x14               pasa a  &this->array_0x0[arg2]: la dirección de un elemento, contando bytes
#   p = this; do {...; p += 5;} while (p != this + 0x23)    pasa a  for (i < 7) con this->array_0x0[i].field_0x8
#   Camera::Init(this), dentro de Camera    pasa a  Init()
#   uint * A::A(void) {...; return this;}   pasa a  A::A(void) {...}: un constructor no tiene tipo
#   0x3f800000                       pasa a  1.0f, si como float es redondo y tiene más de un bit a 1
# Los nombres env y vm salen de cómo se usa cada variable, así que conviene revisar el resultado.
import argparse
import os
import re
import shutil
import struct
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_NAMES = os.path.join(HERE, "..", "src", "android", "jni_names.h")
DEFAULT_RULES = "rules.txt"
SO_BASE = 0x60800000   # donde el loader carga el .so: 0x60000000 + 8 MiB de stack

# JavaVM comparte offsets con JNI. Se distinguen por el número de argumentos, con vm incluido.
VM_CALLS = {0x0c: ("DestroyJavaVM", 1), 0x10: ("AttachCurrentThread", 3), 0x14: ("DetachCurrentThread", 1),
            0x18: ("GetEnv", 3), 0x1c: ("AttachCurrentThreadAsDaemon", 3)}
INDIRECT = re.compile(r"\(\*\*\(\*([\w\[\]]+) \+ (0x[0-9a-f]+)\)\)\(")
SYMBOL = re.compile(r"\b(?:method|sym|class|loc\.imp)\.[\w:.~]+(?=\()")
# Lo que radare2 pone delante del nombre real de una función.
SYMBOL_PREFIX = r"^(?:loc\.imp\.|(?:method|sym)\.(?:imp\.|const\.|destructor\.|constructor\.|virtual\.|static\.)?)"
LITERAL = re.compile(r"\*0x([0-9a-f]+) \+ (-?0x[0-9a-f]+)")
HEADER = re.compile(r"^(?P<type>\w[\w *]*?[ *])(?P<name>[\w:~.]+)\((?P<params>[^()\n]*)\)(?=\s*\{)", re.M)
# Los nombres que da Ghidra a lo que ya había en un register o en el stack.
LEFTOVER = r"(?:extraout|in|unaff)_(?:r\d+|lr|stack_\w+)"
LAST_ARGUMENT = re.compile(rf",\s*((?:\([\w *]+\))?{LEFTOVER}|\*\(\*[\w\[\]]+ \+ 0x[0-9a-f]+\))\s*(?=\))")
SWITCH_CALL = re.compile(r"^([ \t]*)(?:\w+ = )?__gnu_thumb1_case_\w+\(.*\);$", re.M)
VFP_LEFTOVER = r"(?:extraout|in|unaff)_[sdq]\d+"
ASSIGNED_CALL = re.compile(r"^[ \t]*(\w+Var\d+|arg\d+) = ([\w:.~>\-\[\]]+\(.*\));\n([ \t]*)([^\n]*)\n", re.M)
STRING_DESTRUCTOR = re.compile(
    r"^([ \t]*)if \(\((\w+) \+ -0xc != (?:&[^)\n]+|\w+)\) && \((\w+) = [\w:.]+\(\2 \+ -4,\s*-1\),\s*\3 < 1\)\) \{\n"
    r"[ \t]*[\w:.]+\(\2 \+ -0xc,\s*&(\w+)\);\n[ \t]*\}\n", re.M)
INLINED_DESTRUCTOR = re.compile(r"^([ \t]*)(\w+) = &vtable for ([\w:]+) \+ 8;\n[ \t]*[\w:]+::~\w+\(&\2\);\n", re.M)
POINTER_LOOP = re.compile(
    r"^([ \t]*)(\w+) = ([^;\n]+);\n[ \t]*(\w+) = \2 \+ (\w+);\n[ \t]*do \{\n[ \t]*(\w+) = \*\2;\n[ \t]*\2 = \2 \+ 1;\n"
    r"((?:(?!\1\}).*\n)*?)\1\} while \(\2 != \4\);\n", re.M)
OBJECT_CALL = re.compile(r"(?<![\w:.>])((?:\w+::)*\w+)::(~?\w+|operator[^\w\s(]{1,3})\(&?(local_\w+)(?=[,)])")
# Las funciones que usa el compilador cuando no hay unidad de coma flotante.
BINARY_HELPERS = {"__aeabi_fmul": "*", "__mulsf3": "*", "__aeabi_dmul": "*", "__muldf3": "*",
                  "__aeabi_fadd": "+", "__addsf3": "+", "__aeabi_dadd": "+", "__adddf3": "+",
                  "__aeabi_fsub": "-", "__subsf3": "-", "__aeabi_dsub": "-", "__subdf3": "-",
                  "__aeabi_fdiv": "/", "__divsf3": "/", "__aeabi_ddiv": "/", "__divdf3": "/",
                  "__aeabi_fcmplt": "<", "__aeabi_dcmplt": "<", "__aeabi_fcmple": "<=", "__aeabi_dcmple": "<=",
                  "__aeabi_fcmpgt": ">", "__aeabi_dcmpgt": ">", "__aeabi_fcmpge": ">=", "__aeabi_dcmpge": ">=",
                  "__aeabi_fcmpeq": "==", "__aeabi_dcmpeq": "=="}
CAST_HELPERS = {"__aeabi_f2d": "double", "__extendsfdf2": "double", "__aeabi_i2d": "double", "__floatsidf": "double",
                "__aeabi_d2f": "float", "__truncdfsf2": "float", "__aeabi_i2f": "float", "__floatsisf": "float",
                "__aeabi_f2iz": "int", "__fixsfsi": "int", "__aeabi_d2iz": "int", "__fixdfsi": "int",
                "__aeabi_f2uiz": "uint", "__fixunssfsi": "uint", "__aeabi_d2uiz": "uint", "__fixunsdfsi": "uint"}
HELPER_CALL = re.compile(r"(?<![\w:.>])(" + "|".join(list(BINARY_HELPERS) + list(CAST_HELPERS)) + r")\(")
ELEMENT_SIZES = {"uint": 4, "int": 4, "int32_t": 4, "uint32_t": 4, "float": 4, "ushort": 2, "short": 2, "uchar": 1, "char": 1}
FIELD = re.compile(r"\*\((?!p+[a-zA-Z]*Var\d)([A-Za-z_][\w:]*(?:->field_0x[0-9a-f]+)*) \+ (0x[0-9a-f]+|\d+)\)")
GUESSED_TYPE = re.compile(r"(?:u?int(?:8|16|32)_t|u?int|u?short|u?char|u?long|byte|undefined\d?)")
R_ARM_ABS32, R_ARM_GLOB_DAT, R_ARM_JUMP_SLOT, R_ARM_RELATIVE = 2, 21, 22, 23
# Las funciones que el kernel de Linux ofrece a los programas ARM, en direcciones fijas.
KUSER_HELPERS = {0xffff0f60: "__kuser_cmpxchg64", 0xffff0fa0: "__kuser_memory_barrier",
                 0xffff0fc0: "__kuser_cmpxchg", 0xffff0fe0: "__kuser_get_tls"}


def read_names(path):
    text = open(path, encoding="utf-8").read()
    return re.findall(r'"(\w+)"', text[text.index("{"):])


def demangle(name):
    if name.startswith("_Z") and shutil.which("c++filt"):
        result = subprocess.run(["c++filt", name], capture_output=True, text=True)
        if result.returncode == 0:
            return result.stdout.strip()
    return name


def demangle_all(names):
    if not shutil.which("c++filt"):
        return names
    result = subprocess.run(["c++filt"], input="\n".join(names), capture_output=True, text=True)
    lines = result.stdout.split("\n")
    return lines[:len(names)] if result.returncode == 0 and len(lines) >= len(names) else names


def split_types(text):
    """Separa los tipos de una firma. Las comas dentro de <> o de () no separan nada."""
    types, current, depth = [], "", 0
    for c in text:
        depth += (c in "<(") - (c in ">)")
        if c == "," and depth == 0:
            types.append(current.strip())
            current = ""
        else:
            current += c
    if current.strip():
        types.append(current.strip())
    return [] if types == ["void"] else types


def squash(name):
    """r2 cambia los símbolos a su manera. Sin signos ni guiones bajos, los dos nombres coinciden."""
    return re.sub(r"\W|_", "", name)


def function_key(real):
    """squash quita la ~, así que A::A() y A::~A() tendrían la misma clave y uno pisaría al otro."""
    return ("~" if "~" in real.split("(")[0] else "") + squash(real)


def constant_text(value):
    """Si los bits son un float redondo, como 0.7, sale como float. Si no, en hexadecimal."""
    bits = struct.pack("<I", value)
    number = struct.unpack("<f", bits)[0]
    if 1e-4 <= abs(number) <= 1e7:
        short = float(f"{number:.4g}")
        if struct.pack("<f", short) == bits:
            return f"{short}f"
    return hex(value)


def read_global(base, offset):
    """Lo que se lee en una dirección conocida: &DAT_c2d64 con 4 da DAT_c2d68, y &g_x con 0 da g_x."""
    if base and base.startswith("&DAT_"):
        return f"DAT_{int(base[5:], 16) + offset:x}"
    return base[1:] if base and base.startswith("&") and not offset else None


def stops_scan(code):
    """Saltos, llamadas y el final de una función: hacia atrás, a partir de aquí ya no se sabe nada."""
    return (code >> 12 == 0b1101 or code >> 11 == 0b11100 or code >> 11 in (0b11101, 0b11110, 0b11111)
            or code & 0xFF00 == 0xBD00 or code & 0xFF87 == 0x4700)


def writes_register(code, register):
    """¿Esta instrucción Thumb cambia este register (r0-r3)? Instrucciones que no conoce: se asume que sí."""
    low, high = code & 7, (code >> 8) & 7
    if code >> 13 == 0b000:                                  # offsets, add y sub de 3 operandos
        return low == register
    if code >> 13 == 0b001:                                  # mov, cmp, add y sub con inmediato
        return high == register and (code >> 11) & 3 != 1
    if code >> 10 == 0b010000:                               # operaciones entre registers
        return low == register and (code >> 6) & 15 not in (8, 10, 11)
    if code >> 10 == 0b010001:                               # add, cmp y mov con registers altos
        return (code >> 8) & 3 != 1 and (code & 7 | (code >> 4) & 8) == register
    if code >> 11 == 0b01001:                                # ldr rd, [pc, #n]
        return high == register
    if code >> 12 == 0b0101:                                 # ldr y str con register
        return low == register and (code >> 9) & 7 not in (0, 1, 2)
    if code >> 13 == 0b011 or code >> 12 == 0b1000:          # ldr y str con offset
        return low == register and bool(code & 0x800)
    if code >> 12 == 0b1001:                                 # ldr y str con sp
        return high == register and bool(code & 0x800)
    if code >> 12 == 0b1010:                                 # add rd, pc o sp
        return high == register
    if code & 0xFF00 == 0xB000 or code & 0xFE00 == 0xB400:   # add sp y push
        return False
    if code >> 12 == 0b1101 or code >> 11 == 0b11100:        # saltos
        return False
    return True


def copied_register(code):
    """mov rd, rm o adds rd, rm, #0: de qué register se copia. None si no es una copia entre r0-r7."""
    if code >> 6 == 0b0001110000:                            # adds rd, rm, #0
        return (code >> 3) & 7
    if code >> 6 == 0:                                       # movs rd, rm
        return (code >> 3) & 7
    if code >> 8 == 0x46 and not code & 0x40:                # mov rd, rm
        return (code >> 3) & 7
    return None


def read_symbols(library, imported=False):
    """Los símbolos de .dynsym y .symtab definidos en el .so: (dirección, tamaño, tipo, nombre). Con imported,
    los que el .so usa pero no define."""
    data = library.data
    shoff = struct.unpack_from("<I", data, 32)[0]
    shentsize, shnum = struct.unpack_from("<HH", data, 46)
    sections = [struct.unpack_from("<10I", data, shoff + i * shentsize) for i in range(shnum)] if shoff else []
    found = {}
    for section in sections:
        if section[1] not in (2, 11):   # SHT_SYMTAB y SHT_DYNSYM
            continue
        strings = sections[section[6]][4]
        for i in range(section[5] // 16):
            name, value, size, info, _, shndx = struct.unpack_from("<3IBBH", data, section[4] + i * 16)
            if (not shndx or not value) != imported or not name:
                continue
            end = data.index(b"\0", strings + name)
            found[data[strings + name:end].decode("ascii", "replace")] = (value, size, info & 15)
    return found


class Library:
    """Lo que hace falta del .so: sus segmentos, sus símbolos y a qué apunta cada entrada de la GOT."""

    def __init__(self, path):
        self.data = open(path, "rb").read()
        if self.data[:5] != b"\x7fELF\x01":
            sys.exit(f"{path} is not a 32-bit ELF file")
        self.segments, self.writable, dynamic = [], [], None
        phoff = struct.unpack_from("<I", self.data, 28)[0]
        phentsize, phnum = struct.unpack_from("<HH", self.data, 42)
        for i in range(phnum):
            kind, offset, vaddr, _, filesz = struct.unpack_from("<5I", self.data, phoff + i * phentsize)
            if kind == 1:
                self.segments.append((vaddr, filesz, offset))
                memsz, flags = struct.unpack_from("<2I", self.data, phoff + i * phentsize + 20)
                if flags & 2:   # PF_W
                    self.writable.append((vaddr, memsz))
            elif kind == 2:
                dynamic = (offset, filesz)
        self.slots = self.read_slots(dynamic) if dynamic else {}
        self.functions, self.globals, self.addresses, self.sizes = {}, {}, {}, {}
        self.read_names()

    def read_names(self):
        """Los nombres reales: dan los tipos de cada función y el nombre de cada global."""
        symbols = read_symbols(self)
        names = sorted(symbols)
        for name, real in zip(names, demangle_all(names)):
            value, size, kind = symbols[name]
            if kind == 2 and value & 1:   # solo se leen las instrucciones de las funciones Thumb
                self.addresses[name] = value & ~1
                self.sizes[value & ~1] = size
            if kind == 1:   # STT_OBJECT
                self.globals[value] = real
            elif kind == 2:   # STT_FUNC. El bit 0 de la dirección solo indica Thumb.
                if "(" not in real:
                    self.functions[function_key(real)] = (real, None, value & ~1)
                elif "<" not in real.split("(")[0]:
                    head, rest = real.split("(", 1)
                    short = "::".join(head.split("::")[-2:])
                    self.functions[function_key(real)] = (short, split_types(rest.rsplit(")", 1)[0]), value & ~1)
        imported = sorted(read_symbols(self, True))
        for real in demangle_all(imported):   # sin dirección: en el .so solo está su nombre
            if "(" in real and "<" not in real.split("(")[0]:
                head, rest = real.split("(", 1)
                self.functions.setdefault(function_key(real), ("::".join(head.split("::")[-2:]), split_types(rest.rsplit(")", 1)[0]), None))
            elif "(" not in real:
                self.functions.setdefault(function_key(real), (real, None, None))

    def function(self, symbol):
        """method.A::B.m_int_ o sym.m_int_: el nombre corto, los tipos y la dirección, si está en el .so.
        class.X no dice qué método es: radare2 se lo pone al primero de esa clase, por dirección."""
        if symbol.startswith("class."):
            name = symbol[6:].split("::")[-1]
            found = [item for item in self.functions.values() if item[0].startswith(f"{name}::") and item[2]]
            return min(found, key=lambda item: item[2]) if found else None
        name = re.sub(SYMBOL_PREFIX, "", symbol)
        # radare2 escribe ~A como _A: A::_A__ o A._A__.
        destructor = symbol.startswith("method.destructor.") or re.search(r"(\w+)(?:::|\.)_\1(?:_|$)", name)
        return self.functions.get(("~" if destructor else "") + squash(name))

    def halfword(self, address):
        offset = self.file_offset(address, 2)
        return None if offset is None else struct.unpack_from("<H", self.data, offset)[0]

    def literal_pc(self, literal, hint):
        """El pc que se suma a un literal: el de add rd, pc tras el ldr rd, [pc, #n] que lo lee. A veces Ghidra
        escribe solo los 16 bits bajos, con signo: -0x948 en lugar de 0x2f6b8. hint es lo que escribió, y sus 16
        bits bajos tienen que coincidir con los del pc encontrado."""
        found = set()
        for address in range(max(literal - 1020, 0) & ~1, literal, 2):
            code = self.halfword(address)
            if code is None or code & 0xF800 != 0x4800 or ((address + 4) & ~3) + (code & 0xFF) * 4 != literal:
                continue
            register = (code >> 8) & 7
            for later in range(address + 2, min(address + 40, literal), 2):
                if self.halfword(later) == 0x4478 | register:              # add rd, pc
                    found.add(later + 4)
                    break
        matching = [pc for pc in found if (pc - hint) & 0xFFFF == 0]
        return matching[0] if len(matching) == 1 else None

    def plt_target(self, address):
        """Si address es una entrada de la PLT, la función a la que salta. Si no, la misma dirección."""
        entry = address + 4 if self.halfword(address) == 0x4778 else address   # bx pc: la entrada es ARM
        words = [self.word(entry + i * 4) for i in range(3)]
        if None in words or [word & 0xFFFFF000 for word in words] != [0xE28FC000, 0xE28CC000, 0xE5BCF000]:
            return address

        def immediate(word):
            turns = ((word >> 8) & 15) * 2
            return ((word & 255) >> turns | (word & 255) << (32 - turns)) & 0xFFFFFFFF
        slot = entry + 8 + immediate(words[0]) + immediate(words[1]) + (words[2] & 0xFFF)
        return self.addresses.get(self.slots.get(slot), address)

    def calls_at(self, address):
        """La llamada bl o blx que hay en address: [(dónde, a qué función)], o una lista vacía."""
        first, second = self.halfword(address), self.halfword(address + 2)
        if first is None or second is None or first >> 11 != 0b11110 or second >> 11 not in (0b11111, 0b11101):
            return []
        offset = (first & 0x7FF) << 12 | (second & 0x7FF) << 1
        target = address + 4 + (offset - 0x800000 if offset & 0x400000 else offset)
        if second >> 11 == 0b11101:   # blx: pasa a ARM, a una dirección aligned a 4
            target &= ~3
        return [(address, self.plt_target(target))]

    def calls_in(self, start):
        """Las llamadas bl y blx de una función Thumb con símbolo: [(dónde, a qué función)]."""
        calls, address, end = [], start, start + self.sizes.get(start, 0)
        while address + 4 <= end:
            found = self.calls_at(address)
            calls += found
            address += 4 if found else 2
        return calls

    def data_in(self, start):
        """Las direcciones de una función que son datos: literales y tablas de switch. None si no se sabe."""
        data, address, end = set(), start, start + self.sizes.get(start, 0)
        helpers = {self.addresses.get(f"__gnu_thumb1_case_{kind}"): size
                   for kind, size in (("sqi", 1), ("uqi", 1), ("shi", 2), ("uhi", 2), ("si", 4))}
        while address + 2 <= end:
            code, second = self.halfword(address), self.halfword(address + 2)
            if code is None:
                return None
            if address in data:
                address += 2
            elif code >> 11 == 0b01001:                     # ldr rd, [pc, #n]: el literal es un dato
                where = ((address + 4) & ~3) + (code & 255) * 4
                data.update((where, where + 2))
                address += 2
            elif second is not None and code >> 11 == 0b11110 and second >> 11 in (0b11111, 0b11101):
                target = [found for where, found in self.calls_at(address) if where == address]
                size = helpers.get(target[0]) if target else None
                if size:                                    # bl __gnu_thumb1_case_sqi: la tabla va detrás
                    compare = next((self.halfword(address - i) for i in range(2, 14, 2)
                                    if (self.halfword(address - i) or 0) >> 11 == 0b00101), None)
                    if compare is None:
                        return None
                    bytes_used = ((compare & 255) + 1) * size
                    data.update(range(address + 4, address + 4 + bytes_used + 1, 2))
                address += 4
            else:
                address += 2
        return data

    def only_write(self, register, start):
        """La única instrucción de una función que cambia este register. None si hay más, o no se sabe."""
        data = self.data_in(start)
        if data is None:
            return None
        found, address, end = [], start, start + self.sizes.get(start, 0)
        while address + 2 <= end:
            code, second = self.halfword(address), self.halfword(address + 2)
            if address in data or code & 0xFE00 == 0xBC00:  # un pop solo restaura el valor al salir
                address += 2
            elif second is not None and code >> 11 == 0b11110 and second >> 11 in (0b11111, 0b11101):
                address += 4                                # una llamada conserva r4-r7
            else:
                if writes_register(code, register):
                    found.append(address)
                address += 2
        return found[0] if len(found) == 1 else None

    def register_before(self, address, register, start):
        """Lo que hay en un register antes de address, como texto, si lo dicen las instrucciones anteriores.
        Se mira hacia atrás hasta un salto o una llamada: más allá ya no se sabría por dónde se llegó. Si se
        llega al principio sin que nada lo cambie, es un parámetro: devuelve param:N."""
        pc_plus = None   # con add rd, pc: lo que hay que sumar a lo que se lea después
        for _ in range(64):
            address -= 2
            if address < start:
                return f"param:{register}" if register < 4 and pc_plus is None else None
            code = self.halfword(address)
            if code is None or stops_scan(code):
                return None
            if not writes_register(code, register):
                continue
            if code & 0xFF78 == 0x4478 and pc_plus is None:                 # add rd, pc
                pc_plus = address + 4
            elif code >> 11 == 0b01001:                                     # ldr rd, [pc, #n]
                where = ((address + 4) & ~3) + (code & 255) * 4
                word = self.word(where)
                if pc_plus is not None:
                    return None if word is None else self.address_text((word + pc_plus) & 0xFFFFFFFF)
                return self.slot_text(where) or (None if word is None else constant_text(word))
            elif code >> 11 == 0b00100 and pc_plus is None:                 # movs rd, #n
                return str(code & 255) if code & 255 < 10 else hex(code & 255)
            elif code >> 11 == 0b01101 and pc_plus is None:                 # ldr rd, [rn, #n]
                base = self.register_before(address, (code >> 3) & 7, start)
                offset = ((code >> 6) & 31) * 4
                slot = self.slot_text(int(base[5:], 16) + offset) if base and base.startswith("&DAT_") else None
                return slot or read_global(base, offset)                    # una entrada de la GOT da una dirección
            elif code >> 9 == 0b0101100 and pc_plus is None:                # ldr rd, [rn, rm]: la GOT
                base = self.register_before(address, (code >> 3) & 7, start)
                index = self.register_before(address, (code >> 6) & 7, start)
                if base and index and base.startswith("&DAT_") and index.startswith("0x"):
                    return self.slot_text(int(base[5:], 16) + int(index, 16))
                return None
            elif copied_register(code) is not None and pc_plus is None:
                source = copied_register(code)
                if source < 4:
                    return self.register_before(address, source, start)
                # r4-r7 sobreviven a las llamadas: vale lo que se le asignó, si solo se asigna una vez.
                write = self.only_write(source, start)
                return None if write is None else self.register_before(write + 2, source, start)
            else:
                return None
        return None

    def call_argument(self, caller, callee, register, order=None):
        """El argumento de una llamada de caller a callee. Con varias, solo si se sabe cuál es: order es su
        posición entre las del pseudocode y cuántas hay, y tienen que ser las mismas que en las instrucciones."""
        found = sorted(where for where, target in self.calls_in(caller) if target == callee)
        if len(found) == 1:
            return self.register_before(found[0], register, caller)
        if order and order[1] == len(found):
            return self.register_before(found[order[0]], register, caller)
        return None

    def is_writable(self, address):
        return any(vaddr <= address < vaddr + memsz for vaddr, memsz in self.writable)

    def address_text(self, address):
        string = self.string(address)
        return f'"{string}"' if string else f"&DAT_{address:x}"

    def word_text(self, address):
        """Lo que da leer *0x515f0: una dirección reubicada, una global si se puede escribir, o una constante."""
        if address in self.slots:
            return self.slot_text(address)
        if self.is_writable(address):
            return f"DAT_{address:x}"
        value = self.word(address)
        return None if value is None else constant_text(value)

    def file_offset(self, address, size=1):
        for vaddr, filesz, offset in self.segments:
            if vaddr <= address and address + size <= vaddr + filesz:
                return offset + address - vaddr
        return None

    def word(self, address):
        offset = self.file_offset(address, 4)
        return None if offset is None else struct.unpack_from("<I", self.data, offset)[0]

    def string(self, address):
        offset = self.file_offset(address)
        if offset is None:
            return None
        end = self.data.find(b"\0", offset)
        raw = self.data[offset:end] if end >= 0 else b""
        if len(raw) < 2 or not all(32 <= b < 127 or b in (9, 10, 13) for b in raw):
            return None
        return raw.decode("ascii").replace("\n", "\\n").replace("\t", "\\t").replace("\r", "\\r")

    def read_slots(self, dynamic):
        """Las relocations: qué recibirá cada palabra al cargar el .so, símbolo o dirección."""
        tags = {}
        for i in range(dynamic[1] // 8):
            tag, value = struct.unpack_from("<2I", self.data, dynamic[0] + i * 8)
            tags[tag] = value
        slots = {}
        for table, size in ((tags.get(17), tags.get(18, 0)), (tags.get(23), tags.get(2, 0))):   # DT_REL y DT_JMPREL
            start = self.file_offset(table) if table else None
            if start is None:
                continue
            for i in range(size // 8):
                where, info = struct.unpack_from("<2I", self.data, start + i * 8)
                kind, index = info & 0xFF, info >> 8
                if kind == R_ARM_RELATIVE:
                    value = self.word(where)
                    if value is not None:
                        slots[where] = f"DAT_{value:x}"
                elif kind in (R_ARM_ABS32, R_ARM_GLOB_DAT, R_ARM_JUMP_SLOT) and 6 in tags and 5 in tags:
                    symbol = self.file_offset(tags[6] + index * 16, 16)
                    name = struct.unpack_from("<I", self.data, symbol)[0]
                    start_name = self.file_offset(tags[5] + name)
                    end = self.data.index(b"\0", start_name)
                    slots[where] = self.data[start_name:end].decode("ascii", "replace")
        return slots

    def slot_text(self, slot):
        """Lo que hay en esta entrada de la GOT: la dirección de una global."""
        value = self.slots.get(slot)
        return f"&{demangle(value)}" if value else None


def read_addresses(text, library):
    """*(&DAT_bcb6c) lee esa palabra. Si es una entrada de la GOT, lo leído es la dirección de una global."""
    def read(match):
        return library.slot_text(int(match.group(1), 16)) or f"DAT_{match.group(1)}"
    text = re.sub(r"\*\(&DAT_([0-9a-f]+)\)", read, text)
    return re.sub(r"\*&(?=[\w:])", "", text)


def resolve_literals(text, library):
    """*0xA + 0xB es una lectura relativa a pc: la palabra de A más B da la dirección de un dato. Si B no es el pc
    entero, como el -0x948 de 0x2f6b8, se busca el de verdad en las instrucciones."""
    def replace(match):
        literal, pc = int(match.group(1), 16), int(match.group(2), 16)
        word = library.word(literal)
        real = library.literal_pc(literal, pc) if word is not None else None
        if word is None or (pc < 0 and real is None):     # un pc no es negativo: sin comprobarlo, no se toca
            return match.group(0)
        return library.address_text((word + (pc if real is None else real)) & 0xFFFFFFFF)
    return read_addresses(LITERAL.sub(replace, text), library)


def resolve_split_literals(text, library):
    """iVar1 = *0x517a4; ... iVar1 + 0x5170c es lo mismo que *0x517a4 + 0x5170c, en dos líneas. El pc tiene
    que estar cerca del literal: si no, es una suma normal. La variable puede reutilizarse después, así que
    cada literal solo vale hasta la siguiente asignación."""
    lines, current, used = text.split("\n"), {}, set()
    for index, line in enumerate(lines):
        for variable, (literal, word, start) in list(current.items()):
            def replace(match):
                pc = int(match.group(1), 16)
                if abs(pc - literal) >= 0x1000:
                    return match.group(0)
                used.add(start)
                return library.address_text((word + pc) & 0xFFFFFFFF)
            line = re.sub(rf"\b{variable} \+ 0x([0-9a-f]+)\b", replace, line)
        lines[index] = line
        assigned = re.fullmatch(r"[ \t]*(\w+) = \*0x([0-9a-f]+);", line)
        if assigned and library.word(int(assigned[2], 16)) is not None:
            current[assigned[1]] = (int(assigned[2], 16), library.word(int(assigned[2], 16)), index)
        else:
            for variable in list(current):
                if re.match(rf"[ \t]*{variable} = ", line):
                    del current[variable]
    # Quita el literal si, hasta la siguiente asignación, ya nadie lee esa variable.
    names = set()
    for start in sorted(used, reverse=True):
        variable = lines[start].split("=")[0].strip()
        rest = []
        for line in lines[start + 1:]:
            if re.match(rf"[ \t]*{variable} = ", line):
                break
            rest.append(line)
        if not any(re.search(rf"\b{variable}\b", line) for line in rest):
            del lines[start]
            names.add(variable)
    text = "\n".join(lines)
    for variable in names:
        if len(re.findall(rf"\b{variable}\b", text)) == 1:
            text = re.sub(rf"^(?![ \t]*return\b)[ \t]*[\w ]+\*?\b{variable};\n", "", text, flags=re.M)
    return read_addresses(text, library)


def resolve_words(text, library):
    """*0x515f0 lee una palabra del .so, normalmente una constante que no cabe en una instrucción."""
    def replace(match):
        return library.word_text(int(match.group(1), 16)) or match.group(0)
    return re.sub(r"(?<![\w)\]*])\*0x([0-9a-f]+)\b", replace, text)


def read_rule_symbols(path):
    """Las reglas symbol NOMBRE LUGAR de un fichero: {posición dentro del .so: nombre}."""
    found = {}
    with open(path, encoding="utf-8") as rules:
        for line in rules:
            words = line.split()
            if len(words) < 3 or words[0] != "symbol":
                continue
            total = 0
            for part in re.findall(r"[+-]?(?:0x[0-9a-fA-F]+|\d+)", words[2].lstrip("@#")):
                digits = part.lstrip("+-")
                value = int(digits, 16 if digits.startswith("0x") else 10)
                total += -value if part[0] == "-" else value
            found[total if words[2][0] == "@" else total - SO_BASE] = words[1]
    return found


def name_addresses(text, own, library):
    """DAT_c2d4c y fcn.0002d2e8 llevan el nombre que les des con symbol. Un DAT_ sin regla lleva el del
    símbolo del .so, si existe. Devuelve también cada nombre usado con su dirección."""
    named = {}

    def replace(match):
        is_function, address = match.group(1) == "fcn.", int(match.group(2), 16)
        name = own.get(address) or (own.get(address + 1) if is_function else None)   # +1: función Thumb
        if not name and not is_function and library:
            name = library.globals.get(address)
        if not name:
            return match.group(0)
        named.setdefault(name, {})[address] = name
        return name
    return re.sub(r"\b(DAT_|fcn\.)([0-9a-f]+)\b", replace, text), named


def resolve_kuser_helpers(text, library):
    """pcVar1 = *0x9f938; lee una constante. Si es una función del kernel, la llamada lleva su nombre."""
    for variable, address in re.findall(r"\b(\w+) = \*0x([0-9a-f]+);", text):
        name = KUSER_HELPERS.get(library.word(int(address, 16)))
        if not name:
            continue
        text = re.sub(rf"\(\*{variable}\)\(", f"{name}(", text)
        text = remove_if_unused(text, variable, rf"[ \t]*{variable} = \*0x{address};\n")
    return text


def got_offset_variables(text, base_variable, base, library):
    """iVar2 = 0x248; **(iVar3 + iVar2) = 0xf0;: el offset de la entrada está en otra variable. Vale el
    de su última asignación, si entre medias no hay saltos ni loops."""
    lines, used = text.split("\n"), {}
    for index, line in enumerate(lines):
        for match in re.finditer(rf"\*\({base_variable} \+ (\w+Var\d+)\)", line):
            value = None
            for earlier in range(index - 1, -1, -1):
                if re.search(r"^\s*\w+:$|\bgoto\b|^\s*(?:\}|while|for|do|switch)\b", lines[earlier]):
                    break
                found = re.fullmatch(rf"[ \t]*{match[1]} = (?:(0x[0-9a-f]+|\d+);|.*)", lines[earlier])
                if found:
                    value = found[1]
                    break
            slot = library.slot_text(base + int(value, 0)) if value else None
            if slot:
                lines[index] = lines[index].replace(match[0], slot, 1)
                used[earlier] = match[1]
    # iVar2 = 0x248; solo servía para llegar a la entrada. Si ya no queda ninguna por resolver con esa
    # variable, sobra: un offset dentro de la GOT no es un dato del juego.
    text = "\n".join(lines)
    for earlier, variable in sorted(used.items(), reverse=True):
        if not re.search(rf"\*\({base_variable} \+ {variable}\)", text):
            del lines[earlier]
    text = "\n".join(lines)
    for variable in set(used.values()):
        text = remove_if_unused(text, variable, r"(?!)")
    return text


def resolve_got(text, library):
    """iVar6 = DAT_bc6fc; es el inicio de la GOT. Una entrada se lee de dos formas: con el offset en
    un literal, *(iVar6 + *0x32f78), o directamente, *(iVar6 + 0x4bc). Da la dirección de una global."""
    for variable, base in re.findall(r"\b(\w+) = &?DAT_([0-9a-f]+);", text):
        def replace(match):
            word = library.word(int(match.group(1), 16))
            if word is None:
                return match.group(0)
            slot = (word + int(base, 16)) & 0xFFFFFFFF
            return library.slot_text(slot) or f"DAT_{slot:x}"

        def replace_direct(match):
            return library.slot_text(int(base, 16) + int(match.group(1), 16)) or match.group(0)
        literal = rf"\*\({variable} \+ \*0x([0-9a-f]+)\)"
        direct = rf"\*\({variable} \+ (0x[0-9a-f]+)\)"
        text = got_offset_variables(text, variable, int(base, 16), library)
        if not re.search(literal, text) and not re.search(direct, text):
            continue
        text = re.sub(direct, replace_direct, re.sub(literal, replace, text))
        text = remove_if_unused(text, variable, rf"[ \t]*{variable} = &?DAT_{base};\n")
    return re.sub(r"\*&(?=[\w:])", "", text)


def remove_if_unused(text, variable, assignment):
    """Quita la asignación y la declaración de una variable que ya nadie usa."""
    rest = re.sub(assignment, "", text)
    if len(re.findall(rf"\b{variable}\b", rest)) <= 1:   # solo queda la declaración
        rest = re.sub(rf"^(?![ \t]*return\b)[ \t]*[\w ]+\*?\b{variable};\n", "", rest, flags=re.M)
        return rest
    return text


def unwrap_lines(text):
    """Ghidra parte las líneas largas: vuelve a unir lo que tiene los paréntesis sin cerrar."""
    lines, pending = [], None
    for line in text.split("\n"):
        if pending is not None:
            line = f"{pending} {line.strip()}"
            pending = None
        code = re.sub(r'"[^"\n]*"', '""', line)
        if not line.lstrip().startswith("//") and code.count("(") > code.count(")"):
            pending = line
        elif line.strip() == ";" and lines:
            lines[-1] += ";"
        else:
            lines.append(line)
    if pending is not None:
        lines.append(pending)
    return "\n".join(lines)


def reindent(text):
    """Sangra cada línea según sus llaves, porque Ghidra deja de sangrar a partir de cierta profundidad."""
    depth, lines = 0, []
    for line in text.split("\n"):
        stripped = line.strip()
        comment = stripped.startswith("//")
        code = "" if comment else re.sub(r'"[^"\n]*"', '""', stripped)
        closes = 1 if code.startswith("}") else 0
        depth = max(depth - closes, 0)
        label = re.fullmatch(r"\w+:", stripped)
        lines.append(stripped if not stripped or label else " " * (4 * depth) + stripped)
        depth = max(depth + code.count("{") - code.count("}") + closes, 0)
    return "\n".join(lines)


def split_call(text, start):
    """Separa los argumentos de una llamada. start es la posición justo después del paréntesis."""
    depth, quoted, args, current, i = 0, False, [], "", start
    while i < len(text):
        c = text[i]
        if c == '"':
            quoted = not quoted
        if not quoted:
            if c == "(":
                depth += 1
            elif c == ")":
                if depth == 0:
                    if current.strip():
                        args.append(current.strip())
                    return args, i
                depth -= 1
            elif c == "," and depth == 0:
                args.append(current.strip())
                current = ""
                i += 1
                continue
        current += c
        i += 1
    return args, i


def find_call(text, position):
    """Busca hacia atrás el paréntesis de la llamada. Da su posición y el índice del último argumento."""
    depth, index, quoted = 0, 1, False
    for i in range(position - 1, -1, -1):
        c = text[i]
        if c == '"':
            quoted = not quoted
        elif quoted:
            continue
        elif c == ")":
            depth += 1
        elif c == "(":
            if depth == 0:
                return i, index
            depth -= 1
        elif c == "," and depth == 0:
            index += 1
    return -1, index


def is_leftover(argument, index, before_call):
    """¿Este argumento solo aparece porque el valor seguía en un register o en el stack?"""
    pointer = re.fullmatch(r"\*\((\*[\w\[\]]+ \+ 0x[0-9a-f]+)\)", argument)
    if pointer:
        return before_call.endswith(f"(**({pointer.group(1)}))")   # blx r3: r3 es la propia función
    name = argument.rsplit(")", 1)[-1]                              # sin el cast
    if re.fullmatch(r"extraout_r[1-9]\d*", name):
        return True                                                 # r1-r3 tras otra llamada
    return index >= 4                                               # lo que guardó un push en el stack


def drop_leftovers(text):
    """Ghidra no conoce la firma de una llamada por vtable, así que enseña los registers que no se tocan.
    Solo se quitan del final: quitar uno del medio movería los argumentos que van detrás."""
    changed = True
    while changed:
        changed = False
        for match in reversed(list(LAST_ARGUMENT.finditer(text))):
            opening, index = find_call(text, match.start())
            if is_leftover(match.group(1), index, text[:opening]):
                text = text[:match.start()] + text[match.end():]
                changed = True
    for name in set(re.findall(rf"\b({LEFTOVER});\n", text)):
        if len(re.findall(rf"\b{name}\b", text)) == 1:               # solo queda la declaración
            text = re.sub(rf"^(?![ \t]*return\b)[ \t]*[\w ]+\*?\b{name};\n", "", text, flags=re.M)
    return text


def uses_jni(text):
    """Una llamada por tabla puede ser JNI o el método virtual de un objeto: tienen la misma forma.
    Solo se trata como JNI si la función consigue un env con el JavaVM o es un método Java_."""
    if "Java_" in text or re.search(r"\b_JNIEnv\s*\*", text):
        return True
    for match in INDIRECT.finditer(text):
        args = split_call(text, match.end())[0]
        offset = int(match.group(2), 16)
        if offset in VM_CALLS and VM_CALLS[offset][1] == 3 and len(args) == 3 and args[1].startswith("&"):
            return True
    return False


def name_calls(text, names, force_jni=False):
    jni = force_jni or uses_jni(text)
    renames = {}

    def rewrite(text):
        result, position = "", 0
        while True:
            match = INDIRECT.search(text, position)
            if not match:
                return result + text[position:]
            args, end = split_call(text, match.end())
            args = [rewrite(arg) for arg in args]
            offset = int(match.group(2), 16)
            variable = match.group(1)
            result += text[position:match.start()]
            position = end + 1
            if not jni:
                rest = args[1:] if args and args[0] == variable else args
                result += f"{variable}->vcall_0x{offset:x}({','.join(rest)})"
                continue
            if offset in VM_CALLS and len(args) == VM_CALLS[offset][1]:
                owner, name = "vm", VM_CALLS[offset][0]
            elif offset % 4 == 0 and offset // 4 < len(names):
                owner, name = "env", names[offset // 4]
            else:
                result += text[match.start():end + 1]
                continue
            renames[variable] = owner
            if args and args[0] == variable:
                args = args[1:]
            result += f"{owner}->{name}({','.join(args)})"
    return rewrite(text), renames


def clean_symbol(symbol):
    """method.A::B.m_int_ pasa a B::m, sym.A::B::_B____1 a B::~B, sym.imp.memset a memset y class.A a A::A."""
    if symbol.startswith("class."):
        name = symbol[6:].split("::")[-1]
        return f"{name}::{name}"
    name = re.sub(SYMBOL_PREFIX, "", symbol)
    if symbol.startswith("method.destructor."):
        owner = name.split(".")[0].split("::")[-1]
        return f"{owner}::~{owner}"
    if "." in name:
        owner, method = name.split(".", 1)
        return f"{owner.split('::')[-1]}::{method.split('_')[0]}"   # r2 añade los tipos detrás
    parts = name.split("::")
    if len(parts) >= 2:
        last = parts[-1]
        if last.startswith("_"):
            return f"{parts[-2]}::~{parts[-2]}"
        return f"{parts[-2]}::{last.split('_')[0]}"
    return name


def name_methods(text, library=None):
    """Da nombre a las llamadas y apunta los tipos y la dirección de cada función que se llama."""
    called, targets = {}, {}

    def rewrite(text):
        result, position = "", 0
        while True:
            match = SYMBOL.search(text, position)
            if not match:
                return result + text[position:]
            args, end = split_call(text, match.end() + 1)
            args = [rewrite(arg) for arg in args]
            symbol = match.group(0)
            name, types, address = (library and library.function(symbol)) or (clean_symbol(symbol), None, None)
            called[name] = types if called.get(name, types) == types else None   # None si hay sobrecargas
            if address and not name.startswith("__"):   # los __aeabi_ y similares son del compilador
                label = name if types is None else f"{name}({', '.join(types)})"
                targets.setdefault(name, {})[address] = label
            result += text[position:match.start()]
            before = text[text.rfind("\n", 0, match.start()) + 1:match.start()]
            if re.fullmatch(r"\w[\w *]*[ *]", before) and not before.startswith("return"):   # es el header
                result += f"{name}({','.join(args)})"
            elif name.startswith("_JNIEnv::"):
                missing = [f"/* {kind.strip('_*')} */" for kind in (types or ())[len(args) - 1:]]
                result += f"env->{name.split('::')[1]}({','.join(args[1:] + missing)})"
            elif types and len(args) < len(types):              # Ghidra no ve lo que sigue en el register
                missing = [f"/* {kind} */" for kind in types[len(args):] if kind != "..."]
                result += f"{name}({','.join(args + missing)})"
            else:
                result += f"{name}({','.join(args)})"
            position = end + 1
    return rewrite(text), called, targets


def apply_renames(text, renames):
    # piStack_24 y &stack0xffffffdc son la misma variable: -0x24 = 0xffffffdc. Ghidra también puede
    # verla como un array, apiStack_1c[0].
    for variable in list(renames):
        found = re.search(r"Stack_([0-9a-f]+)(?:\[0\])?$", variable)
        if found:
            offset = (0x100000000 - int(found.group(1), 16)) & 0xFFFFFFFF
            text = re.sub(rf"\bstack0x{offset:08x}\b", variable, text)
    for variable, name in renames.items():
        text = re.sub(rf"(?<!\w){re.escape(variable)}(?!\w)", name, text)
        # int *apiStack_1c [2]; pasa a int *env;, si no se usa ninguna otra posición.
        base = variable.split("[")[0]
        if variable.endswith("[0]") and f"{base}[" not in text:
            text = re.sub(rf"\b{base} \[\d+\];", f"{name};", text)
    return text


def clean_stack(text):
    """Un solo nombre para cada variable del stack: local_18 es la que está en sp - 0x18."""
    # iVar1 = &stack0xfffffff0; hace que iVar1 + -8 sea la dirección de otra variable.
    for variable, frame in re.findall(r"\b(\w+) = &stack0x([0-9a-f]{8});", text):
        top = int(frame, 16)

        def replace(match):
            amount = int(match.group(1), 0)
            return f"&local_{(0x100000000 - (top - amount)) & 0xFFFFFFFF:x}"
        text = re.sub(rf"\b{variable} \+ -((?:0x)?[0-9a-f]+)\b", replace, text)
        text = remove_if_unused(text, variable, rf"[ \t]*{variable} = &stack0x{frame};\n")
    text = re.sub(r"\bstack0x([0-9a-f]{8})\b", lambda m: f"local_{0x100000000 - int(m.group(1), 16):x}", text)
    text = re.sub(r"\b[a-z]*Stack_([0-9a-f]+)\b", r"local_\1", text)
    # &local_18 + -0x14 está 0x14 bytes más abajo en el stack: es &local_2c.
    return re.sub(r"&local_([0-9a-f]+) \+ -(0x[0-9a-f]+|\d+)\b",
                  lambda m: f"&local_{int(m[1], 16) + int(m[2], 0):x}", text)


def join_doubles(text):
    """printf(f, v, v >> 0x20): un double ocupa dos registers, y Ghidra enseña cada mitad. Es un solo valor."""
    return re.sub(r"\b(\w+Var\d+),\s*\1 >> 0x20\b", r"\1", text)


def clean_switches(text):
    """__gnu_thumb1_case_sqi es un switch con su tabla justo detrás de la llamada, y Ghidra lee esa tabla
    como instrucciones. Thumb-1 no tiene registers VFP, así que lo que los usa sale de la tabla."""
    if "__gnu_thumb1_case_" not in text:
        return text
    text = SWITCH_CALL.sub(r"\1// unresolved switch: what follows may not be real", text)
    return re.sub(rf"^[^\n]*\b{VFP_LEFTOVER}\b[^\n]*\n", "", text, flags=re.M)


def each_function(text, fix):
    headers = list(HEADER.finditer(text))
    for i in range(len(headers) - 1, -1, -1):
        end = headers[i + 1].start() if i + 1 < len(headers) else len(text)
        text = text[:headers[i].start()] + fix(headers[i], text[headers[i].end():end]) + text[end:]
    return text


def fix_signature(header, body):
    """//A::m(char const*) es la firma real: da los tipos y deja ver qué parámetro es this."""
    short = re.escape(header["name"].split("::")[-1])
    comment = re.search(rf"^[ \t]*//\s*((?:[\w~]+::)*{short}\w*)\((.*?)\)(?: const)?[ \t]*\n", body, re.M)
    if not comment:
        return header[0] + body
    real, found = split_types(comment[2]), split_types(header["params"])
    if len(found) not in (len(real), len(real) + 1):
        if len(found) <= len(real) + 1 or "::" not in comment[1]:
            return header[0] + body
        # Ghidra ve más parámetros de los que tiene el método. Los que sobran son registers que nadie pasó,
        # como un s1 en un .so sin VFP: se tratan como valores desconocidos.
        for extra in found[len(real) + 1:]:
            body = re.sub(rf"\b{extra.split()[-1].lstrip('*')}\b", "unknown_register", body)
        found = found[:len(real) + 1]
    body = body[:comment.start()] + body[comment.end():]
    first = found[0].split()[-1].lstrip("*") if found else None
    # Con el mismo número, el primero solo puede ser this si se usa como pointer y su tipo no lo es: Ghidra no
    # enseña un parámetro que no se usa, como el float de update(float).
    missing_this = (found and "::" in comment[1] and not re.search(r"\*|&", real[0] if real else "*")
                    and re.search(rf"\*\(?{first}\b", body))
    has_this = len(found) > len(real) or bool(missing_this)
    if has_this:
        this = found.pop(0).split()[-1].lstrip("*")
        body = re.sub(rf"\b{this}\b", "this", body)
        body = re.sub(r"\bin_r0\b", "this", remove_declaration(body, "in_r0"))
    names = [param.split()[-1].lstrip("*") for param in found]
    if any(re.fullmatch(r"noname_\d+", name) for name in names) or (missing_this and not names):
        names, body = names_by_register(names, body, 1 if has_this else 0)
    names += [f"arg{i + (1 if has_this else 0)}" for i in range(len(names), len(real))]
    params = ", ".join(f"{kind} {name}" for kind, name in zip(real, names)) or "void"
    return f"{header['type']}{comment[1]}({params})" + body


CALLEE = r"(?:[\w:]+::)?operator[^\w\s(]{1,3}|[\w:]+"   # el nombre de una función, también operator+
FAKE_REGISTER = r"\b(?:unknown_register|(?:extraout|in|unaff)_[sdq]\d+(?:_\d+)?)\b"


def fake_registers(header, body):
    """extraout_s1 en un .so de Thumb-1 no existe: ese .so no tiene registers VFP. Ghidra lo enseña cuando se
    equivoca con la firma de una función. Como argumento, lo que de verdad se pasa es el register normal de esa
    posición, y se marca para leerlo de las instrucciones: f(x, extraout_s1) pasa a f(x, /* r1 */). Las
    asignaciones de esos valores no significan nada y se quitan."""
    if not re.search(FAKE_REGISTER, body):
        return header[0] + body
    fake = FAKE_REGISTER
    for name in set(re.findall(rf"\b(\w+Var\d+) = {FAKE_REGISTER}", body)):
        values = re.findall(rf"(?<![\w*]){name} = ([^,;)]+)", body)
        if all(re.fullmatch(FAKE_REGISTER, value.strip()) for value in values):
            fake += rf"|\b{name}\b"                   # uVar6 = extraout_s1: uVar6 tampoco existe
    fake = rf"(?:{fake})"
    body = re.sub(rf"^[ \t]*\w+ = {fake};\n", "", body, flags=re.M)
    body = re.sub(rf",\s*\w+ = {fake}(?=[,)])|\b\w+ = {fake},\s*", "", body)
    lines = body.split("\n")
    for index, line in enumerate(lines):
        position = 0
        while re.search(fake, line[position:]):
            call = re.compile(r"[\w:.>+\-]\(").search(line, position)
            if not call:
                break
            args, end = split_call(line, call.end())
            new = [f" /* r{i} */" if re.fullmatch(rf"\s*{fake}\s*", arg) and i < 4 else arg
                   for i, arg in enumerate(args)]
            text = ",".join(new).lstrip()
            line = line[:call.end()] + text + line[end:]
            position = call.end()
        lines[index] = line
    body = "\n".join(lines)
    for name in set(re.findall(rf"^[ \t]*[\w ]+\*?({fake});$", body, re.M)):
        body = remove_declaration(body, name)
    for name in set(re.findall(r"^[ \t]*[\w ]+\*?\b(\w+Var\d+);$", body, re.M)):
        body = drop_if_unused(body, name)
    return header[0] + body


# Dónde va el formato en cada función tipo printf: lo que lo sigue lo pide el propio formato.
FORMAT_POSITION = {"printf": 0, "sprintf": 1, "snprintf": 2, "fprintf": 1, "__android_log_print": 2}
FORMAT_CALL = re.compile(r"(?<![\w:.>])(" + "|".join(FORMAT_POSITION) + r")\(")
SPECIFIER = re.compile(r"%(?:%|[-+ #0]*(\*|\d+)?(?:\.(\*|\d+))?(?:hh|h|ll|l|L|q|j|z|t)?[diouxXeEfFgGaAcspn])")


def format_arguments(text):
    """printf("x=%d"): Ghidra no conoce la firma, así que no enseña lo que pide el formato. Ese valor va en el
    register siguiente: printf("x=%d", /* r1 */)."""
    lines = text.split("\n")
    for index, line in enumerate(lines):
        position = 0
        while not line.lstrip().startswith("//"):
            call = FORMAT_CALL.search(line, position)
            if not call:
                break
            args, end = split_call(line, call.end())
            position = call.end()
            where = FORMAT_POSITION[call[1]]
            if len(args) <= where or not re.fullmatch(r'"[^"]*"', args[where]):
                continue
            needed = where + 1
            for found in SPECIFIER.finditer(args[where]):
                if found[0] != "%%":
                    needed += 1 + (found[1] == "*") + (found[2] == "*")
            missing = [f"/* r{i} */" if i < 4 else "/* stack */" for i in range(len(args), needed)]
            if missing:
                line = line[:call.end()] + ", ".join(args + missing) + line[end:]
        lines[index] = line
    return "\n".join(lines)


def call_order(body, callee, line_number, column):
    """El orden de una llamada entre las que van a la misma función: (número, total). Solo vale si el texto
    sigue el orden de las instrucciones: todas en el nivel de arriba, sin saltos, loops ni else."""
    if re.search(r"\bgoto\b|^\s*\w+:$|\b(?:while|for|switch|else)\b|\bdo \{", body, re.M):
        return None
    found = []
    for number, line in enumerate(body.split("\n")):
        if line.lstrip().startswith("//"):          # el comentario con la dirección no es una llamada
            continue
        for match in re.finditer(rf"(?<![\w:.>]){re.escape(callee)}\(", line):
            if line.startswith("        "):
                return None
            found.append((number, match.start()))
    return (found.index((line_number, column)), len(found)) if (line_number, column) in found else None


def names_by_register(names, body, first_register):
    """Si Ghidra llama noname_1 a un parámetro y arg1 al siguiente, dos pasarían a llamarse igual. Se renombran
    todos por su register: arg2 es lo que entró en r2."""
    new = [f"arg{first_register + i}" if re.fullmatch(r"noname_\d+|arg\d+", name) else name for i, name in enumerate(names)]
    for i, name in enumerate(names):
        body = re.sub(rf"\b{name}\b", f"\0{i}\0", body)
    return new, re.sub(r"\0(\d+)\0", lambda m: new[int(m[1])], body)


def remove_declaration(body, variable):
    return re.sub(rf"^(?![ \t]*return\b)[ \t]*[\w ]+\*?\b{variable};\n", "", body, flags=re.M)


def use_params(body, params):
    """iVar2 = arg1; es arg1 con otro nombre, si ninguna de las dos cambia después. *p = 0 no cambia p. Si p apunta
    a palabras, p[7] está en el byte 0x1c, y se escribe así para no mezclar las dos formas de contar."""
    for variable, param in re.findall(r"^    (\w+Var\d+|arg\d+_\d+) = (\w+);\n", body, re.M):
        if (param in params and len(re.findall(rf"(?<![*\w]){variable} = ", body)) == 1
                and not re.search(rf"(?<![*\w]){param} = ", body) and f"&{variable}" not in body):
            body = re.sub(rf"^    {variable} = {param};\n", "", body, flags=re.M)
            declared = re.search(rf"^[ \t]*([\w ]+?) \*{variable};$", body, re.M)
            size = ELEMENT_SIZES.get(declared[1]) if declared else None
            if size:
                body = re.sub(rf"\b{variable}\[(\d+)\]", lambda m: f"*({param} + {int(m[1]) * size:#x})", body)
            body = re.sub(rf"\b{variable}\b", param, remove_declaration(body, variable))
    return body


def inline_calls(body):
    """piVar1 = f(); piVar1->g(); es f()->g(), si piVar1 solo se usa en la línea siguiente."""
    position = 0
    while True:
        match = ASSIGNED_CALL.search(body, position)
        if not match:
            return body
        position = match.end(2)
        variable, value, indent, following = match.groups()
        if (len(re.findall(rf"\b{variable}\b", body)) != 3
                or not re.search(rf"^(?![ \t]*return\b)[ \t]*[\w ]+\*?\b{variable};$", body, re.M)
                or not re.search(rf"\b{variable}\b", following)
                or re.match(r"while\b|for\b|do\b|\}|//", following)
                or split_call(value, value.index("(") + 1)[1] != len(value) - 1):
            continue
        new_line = re.sub(rf"\b{variable}\b", lambda _: value, following)
        body = remove_declaration(body[:match.start()] + indent + new_line + "\n" + body[match.end():], variable)
        position = 0


def no_call_before(lines, index):
    """1 si ninguna llamada ha podido ejecutarse antes de esta línea, así que r0-r3 siguen como al entrar.
    Una etiqueta, un else o un loop dejan otros caminos sin ver, y entonces no se puede saber."""
    i = index - 1
    while i >= 0:
        text = lines[i].strip()
        if re.match(r"\w+:$|else\b|while\b|for\b|do\b|switch\b", text):
            return 0
        if text == "}":
            before = next((lines[j].strip() for j in range(i - 1, -1, -1) if lines[j].strip()), "")
            # Un bloque que no sigue hacia abajo no llega hasta aquí: se salta entero.
            if re.match(r"goto\b|return\b|// unresolved switch", before):
                closing = lines[i].index("}")
                i -= 1
                while i > 0 and not (lines[i].endswith("{") and len(lines[i]) - len(lines[i].lstrip()) == closing):
                    i -= 1
                continue
        elif not text.startswith("//") and re.search(r"\w\(", text):
            return 0
        i -= 1
    return 1


def argument_text(header, value):
    """param:1 es lo que entró en r1. En una función libre es el parámetro de esa posición."""
    if not value or not value.startswith("param:"):
        return value
    params = split_types(header["params"])
    index = int(value[6:])
    if "::" in header["name"] or index >= len(params):
        return None
    return params[index].split()[-1].lstrip("*")


def fill_from_code(header, params, body, called, read_argument):
    """Rellena f(/* char const* */) con lo que dicen las instrucciones del .so. En un método, r0 suele ser
    this y las posiciones no coinciden: solo se rellena si se ve que no lo es, porque su primer argumento
    es un parámetro de la función con el tipo que espera, como un float."""
    kinds = {param.split()[-1].lstrip("*"): param.rpartition(" ")[0] for param in params}

    def fill(match, number=0):
        args = match.group(2).split(",")
        order = call_order(body, match.group(1), number, match.start())
        if "::" in match.group(1):
            types = called.get(match.group(1))
            if not types or re.search(r"\*|&", types[0]) or kinds.get(args[0]) != types[0]:
                return match.group(0)
        for i, arg in enumerate(args):
            if re.fullmatch(r"/\* .* \*/", arg) and i < 4:
                args[i] = argument_text(header, read_argument(header["name"], match.group(1), i, order)) or arg
        return f"{match.group(1)}({','.join(args)})"
    return "\n".join(line if line.lstrip().startswith("//") else
                     re.sub(rf"(?<![\w:.>])({CALLEE})\(([^()]*)\)", lambda m, n=number: fill(m, n), line)
                     for number, line in enumerate(body.split("\n")))


def fill_from_registers(header, body):
    """Ghidra no enseña un argumento si su register no se ha tocado: f(/* float */) recibe r0 tal como
    entró en esta función. Se rellena con el parámetro de la misma posición si ninguna llamada ha podido
    cambiarlo. Solo entre funciones libres: en un método, r0 es this y las posiciones no coinciden."""
    params = split_types(header["params"])
    if "::" in header["name"] or not params or re.search(r"double|long long", header["params"]):
        return body
    lines = body.split("\n")

    def fill(match):
        args = match.group(2).split(",")
        for i, arg in enumerate(args):
            placeholder = re.fullmatch(r"/\* (.*) \*/", arg)
            if not placeholder or i >= len(params) or re.search(r"double|long long", placeholder.group(1)):
                continue
            kind, _, name = params[i].rpartition(" ")
            name = name.lstrip("*")
            changed = re.search(rf"\b{name} = ", body) or f"&{name}" in body
            if not changed and (kind == placeholder.group(1) or GUESSED_TYPE.fullmatch(kind)):
                args[i] = f"{name} /* r{i} */"
        return f"{match.group(1)}({','.join(args)})"
    for index, line in enumerate(lines):
        if "/* " in line and not line.lstrip().startswith("//") and no_call_before(lines, index):
            lines[index] = re.sub(r"(?<![\w:.>])(\w+)\(([^()]*)\)", fill, line)
    return "\n".join(lines)


def typed_params(header, body, called):
    """Si un parámetro se pasa tal cual a una función con tipos conocidos, ese es su tipo."""
    params = split_types(header["params"])
    for i, param in enumerate(params):
        kind, _, name = param.rpartition(" ")
        if not GUESSED_TYPE.fullmatch(kind):
            continue
        for function, types in called.items():
            for match in re.finditer(rf"(?<![\w:]){re.escape(function)}\(", body) if types else ():
                args = [re.sub(r"\s*/\*.*\*/", "", arg) for arg in split_call(body, match.end())[0]]
                extra = len(args) - len(types)                # 1 si se ve el this
                if extra in (0, 1) and name in args[extra:]:
                    params[i] = f"{types[args.index(name, extra) - extra]} {name}"
    return f"{header['type']}{header['name']}({', '.join(params) or 'void'})"


def name_jni_variables(text):
    """vm->AttachCurrentThread(&local_1c, 0) deja el JNIEnv en local_1c. Ghidra lo ve como un array."""
    match = re.search(r"\bvm->(?:GetEnv|AttachCurrentThread(?:AsDaemon)?)\(&(\w+)", text)
    if match and match[1] != "env":
        name = match[1]
        declaration = rf"^(?![ \t]*return\b)([ \t]*)[\w ]+\*?\b{name}(?: \[\w+\])?;\n"
        text = re.sub(declaration, r"\1int *env;\n", text, flags=re.M)
        text = re.sub(rf"\b{name}\b(?:\[0\])?", "env", text)
    text = re.sub(r"^(?![ \t]*return\b)([ \t]*)[\w ]+\*?\bvm;$", r"\1JavaVM *vm;", text, flags=re.M)
    text = re.sub(r"^(\w[^\n(]*\()\w+ \*?env\b", r"\1JNIEnv *env", text, flags=re.M)
    return re.sub(r"^(?![ \t]*return\b)([ \t]*)[\w ]+\*?\benv;$", r"\1JNIEnv *env;", text, flags=re.M)


def fill_jni_calls(header, body, read_argument):
    """env->CallStaticVoidMethod(/* jclass */, /* jmethodID */, /* ... */): la clase y el método se leen de las
    instrucciones. Un __aeabi_f2d(x) justo antes es el float que se pasa: en ... todo float llega como double."""
    lines = body.split("\n")
    for index, line in enumerate(lines):
        match = re.search(r"\benv->(\w+)\(", line)
        if not match or ("/* " not in line and not (index and "__aeabi_f2d(" in lines[index - 1])):
            continue
        args, end = split_call(line, match.end())
        for i, arg in enumerate(args):
            if arg in ("/* jclass */", "/* jmethodID */") or re.fullmatch(r"/\* r\d \*/", arg):
                value = read_argument(header["name"], f"_JNIEnv::{match[1]}", i + 1)
                args[i] = argument_text(header, value) or arg
            elif (arg == "/* ... */" or (arg == "env" and i == 2)) and index:
                value = re.fullmatch(r"\s*__aeabi_f2d\((\w+)\);", lines[index - 1])
                if value:
                    args[i] = f"(double){value[1]}"
                    lines[index - 1] = None
        lines[index] = line[:match.end()] + ",".join(args) + line[end:]
    return "\n".join(line for line in lines if line is not None)


def fill_tested_method(body):
    """if (jMethodX == 0) {...} else { env->CallStaticVoidMethod(a, /* jmethodID */) }: es ese método."""
    lines = body.split("\n")
    for index, line in enumerate(lines):
        if "/* jmethodID */" not in line or not index or lines[index - 1].strip() != "else {":
            continue
        indent = lines[index - 1][:lines[index - 1].index("else")]
        for before in reversed(lines[:index - 1]):
            if before.startswith(indent) and before[len(indent):len(indent) + 1] not in (" ", "}"):
                tested = re.match(r"\s*if \((\w*[Mm]ethod\w*) == 0\) \{$", before)
                if tested:
                    lines[index] = line.replace("/* jmethodID */", tested[1])
                break
    return "\n".join(lines)


def use_operators(line):
    """__aeabi_fmul(a, b) es a * b, y __fixsfsi(x) es (int)x."""
    def rewrite(text):
        result, position = "", 0
        while True:
            match = HELPER_CALL.search(text, position)
            if not match:
                return result + text[position:]
            args, end = split_call(text, match.end())
            args = [rewrite(arg) for arg in args]
            if match[1].startswith(("__aeabi_f", "__mulsf", "__addsf", "__subsf", "__divsf")):
                args = [constant_text(int(arg, 16)) if re.fullmatch(r"0x[0-9a-f]{8}", arg) else arg for arg in args]
            result += text[position:match.start()]
            position = end + 1
            if match[1] in BINARY_HELPERS and len(args) == 2:
                result += f"({args[0]} {BINARY_HELPERS[match[1]]} {args[1]})"
            elif match[1] in CAST_HELPERS and len(args) == 1:
                value = args[0]
                simple = re.fullmatch(r"[\w.]+", value) or (
                    value.startswith("(") and split_call(value, 1)[1] == len(value) - 1)
                result += f"({CAST_HELPERS[match[1]]}){value if simple else f'({value})'}"
            else:
                result += text[match.start():end + 1]
    if line.lstrip().startswith("//") or re.fullmatch(r"\s*\w+\([^()]*\);", line):
        if re.match(r"\s*__aeabi_f", line):                  # sin usar su resultado, sigue siendo una llamada
            return re.sub(r"\b0x[0-9a-f]{8}\b", lambda m: constant_text(int(m[0], 16)), line)
        return line
    line = rewrite(line)
    return re.sub(r"(= |return )\(([^()]*)\);$", r"\1\2;", line)


def fold_comparisons(body):
    """v = a < b; if (v == 0) { es if (a >= b) {, si v no se lee hasta su siguiente asignación."""
    lines = body.split("\n")
    for i in range(len(lines) - 2, -1, -1):
        value = re.fullmatch(r"[ \t]*(\w+Var\d+) = ([^;]+ (?:<|>|<=|>=|==) [^;]+);", lines[i])
        test = re.fullmatch(r"([ \t]*)if \((\w+) (==|!=) 0\) \{", lines[i + 1]) if value else None
        if not test or test[2] != value[1]:
            continue
        later = next((line for line in lines[i + 2:] if re.search(rf"\b{value[1]}\b", line)), "")
        if later and not re.match(rf"[ \t]*{value[1]} = (?!.*\b{value[1]}\b)", later):
            continue
        condition = negate(value[2]) if test[3] == "==" else value[2]
        lines[i:i + 2] = [f"{test[1]}if ({condition}) {{"]
    return "\n".join(lines)


# Las divisiones enteras: ARMv5 no tiene instrucción para dividir y el compilador llama a estas funciones.
# Devuelven el cociente en r0 (r0:r1 las de 64 bits) y el resto en el register que se indica.
DIVISIONS = {"__aeabi_idiv": None, "__aeabi_uidiv": None, "__aeabi_idivmod": 1, "__aeabi_uidivmod": 1,
             "__aeabi_ldivmod": 2, "__aeabi_uldivmod": 2}
DIVISION_CALL = re.compile(r"(?<![\w:.>])(" + "|".join(DIVISIONS) + r")\(")
LABEL = re.compile(r"\s*\w+:")
LOOP_HEAD = re.compile(r"\s*(?:do \{|while\b|for\b)")


def operand(text):
    """a + 1 pasa a (a + 1): entre paréntesis si tiene operadores fuera de paréntesis."""
    depth, quoted = 0, False
    for i, c in enumerate(text):
        if c == '"':
            quoted = not quoted
        elif not quoted and c in "([":
            depth += 1
        elif not quoted and c in ")]":
            depth -= 1
        elif not quoted and depth == 0 and c == " " and re.match(r" (?:[-+*/%&|^<>=!?]+|&&|\|\|) ", text[i:]):
            return f"({text})"
    return text


def ghidra_name(body, kind):
    """Un nombre nuevo al estilo de Ghidra para una variable de ese tipo: uVar18, iVar18, puVar18..."""
    stars = kind.count("*")
    letter = {"int": "i", "int32_t": "i", "float": "f", "char": "c", "bool": "b", "uchar": "b", "byte": "b",
              "short": "s", "long": "l", "longlong": "l"}.get(kind.replace("*", "").strip(), "u")
    numbers = [int(n) for n in re.findall(r"\b[a-z]*Var(\d+)\b", body)]
    return f"{'p' * stars}{letter}Var{max(numbers, default=0) + 1}"


def declared_type(body, name):
    found = re.search(rf"^(?![ \t]*return\b)[ \t]*([\w ]+?) ?(\**){name};$", body, re.M)
    return f"{found[1]} {found[2]}".strip() if found else None


def straight_order(body, callee, line_number, column):
    """Como call_order, pero basta con que todas las llamadas a callee vayan antes del primer salto, etiqueta,
    loop o else: hasta ahí el texto sigue el orden de las instrucciones."""
    found, broken = [], False
    for number, line in enumerate(body.split("\n")):
        if line.lstrip().startswith("//"):
            continue
        for match in re.finditer(rf"(?<![\w:.>]){re.escape(callee)}\(", line):
            if broken:
                return None
            found.append((number, match.start()))
        if re.search(r"\bgoto\b|^\s*\w+:$|\b(?:while|for|switch|else)\b|\bdo \{", line):
            broken = True
    return (found.index((line_number, column)), len(found)) if (line_number, column) in found else None


def remainder_variable(lines, start, body, exclude=()):
    """Ghidra no sabe que la función deja el resto en un register, así que lo enseña después como una
    variable que nadie asigna: extraout_r1, o incluso arg1_01. Se busca en las líneas siguientes, hasta otra
    llamada, una etiqueta o un bloque."""
    for line in lines[start:start + 8]:
        text = line.strip()
        if text.startswith("//") or not text:
            continue
        if LABEL.fullmatch(line) or text in ("{", "}") or text.endswith("{") or text.startswith("}"):
            return None
        for name in re.findall(r"\b(?:extraout_r\d(?:_\d+)?|[a-z]*Var\d+|arg\d+(?:_\d+)?)\b", line):
            if (name not in exclude and declared_type(body, name)
                    and not re.search(rf"(?<![\w*.>]){name} = (?!=)", body)):
                return name
        if re.search(r"(?<![\w:.>])[A-Za-z_][\w:.]*\(", line.split("=", 1)[-1]):
            return None
    return None


def inline_once(lines, index, variable):
    """v = expr; seguido de un solo uso de v es ese uso con expr dentro, si entre medias no cambia nada de lo
    que lee expr y nadie más puede leer v. Devuelve las líneas nuevas, o None."""
    match = re.fullmatch(rf"([ \t]*){variable} = (.+);", lines[index])
    if not match:
        return None
    expr, names = match[2], set(re.findall(r"[A-Za-z_]\w*", match[2]))
    writes = [i for i, line in enumerate(lines) if re.match(rf"[ \t]*{variable} = (?!=)", line)]
    uses = [i for i, line in enumerate(lines) if i not in writes and re.search(rf"\b{variable}\b", line)
            and not re.fullmatch(rf"(?![ \t]*return\b)[ \t]*[\w ]+\*?\b{variable};", line)]
    following = [i for i in uses if i > index and all(not index < w < i for w in writes)]
    if len(following) != 1:
        return None
    use = following[0]
    if len(writes) > 1:
        # Con varias asignaciones, un loop o una etiqueta podrían llevar otro valor hasta el uso.
        later = [w for w in writes if w > index]
        end = later[0] if later else len(lines)
        if any(i > use and i < end for i in uses) or any(LABEL.fullmatch(line) for line in lines[index:end]):
            return None
        level = len(match[1])
        for line in reversed(lines[:index]):
            if line.strip() and len(line) - len(line.lstrip()) < level and not LABEL.fullmatch(line):
                level = len(line) - len(line.lstrip())
                if LOOP_HEAD.match(line):
                    return None
    elif len(uses) != 1:
        return None
    for line in lines[index + 1:use]:
        text = line.strip()
        if not text or text.startswith("//"):
            continue
        if (LABEL.fullmatch(line) or "{" in text or "}" in text or re.search(r"[\w\]]\(", text)
                or any(re.search(rf"(?<![\w.>]){name}\b[^=;]*? = (?!=)", text) for name in names)):
            return None
    if len(re.findall(rf"\b{variable}\b", lines[use])) != 1:
        return None
    whole = re.fullmatch(rf"([ \t]*[^=\n]+ = ){variable};", lines[use])
    value = expr if whole else operand(expr)
    new = list(lines)
    new[use] = re.sub(rf"\b{variable}\b", lambda _: value, lines[use])
    del new[index]
    return new


def use_division(header, body, read_helper):
    """__aeabi_idivmod(a, b); x = extraout_r1;  pasa a  x = a % b, y v = __aeabi_idiv(a, b) a v = a / b. En las
    de 64 bits el divisor va en r2:r3, que Ghidra no enseña: se lee de las instrucciones."""
    if not DIVISION_CALL.search(body):
        return header[0] + body
    lines, touched, halves = body.split("\n"), set(), set()
    divisors = {}
    for number, line in enumerate(lines):
        if line.lstrip().startswith("//"):
            continue
        for k, match in enumerate(DIVISION_CALL.finditer(line)):
            if DIVISIONS[match[1]] != 2:
                continue
            order = straight_order(body, match[1], number, match.start())
            low, high = (read_helper(header["name"], match[1], register, order) for register in (2, 3))
            known = [value for value in (low, high) if value and not value.startswith("param:")]
            if len(known) == 2:
                # La mitad alta suele ser 0: con movs sale 0, y leída de un literal, 0x0.
                zero = re.fullmatch(r"0|0x0+", high)
                divisors[(number, k)] = low if zero else f"((int64_t){high} << 32 | {low})"

    def pieces(name, args, number, k):
        """El dividendo y el divisor. k es cuál de las divisiones de la línea: se reescriben en orden."""
        if DIVISIONS[name] != 2:
            return (args + ["/* r1 */"])[:2] if args else ["/* r0 */", "/* r1 */"]
        divisor = divisors.get((number, k), "/* r2:r3 */")
        if len(args) == 2:
            low = re.search(rf"^[ \t]*{re.escape(args[0])} = (.+);$", body, re.M)
            high = re.search(rf"^[ \t]*{re.escape(args[1])} = (.+);$", body, re.M)
            if low and high and high[1] == f"(&{low[1]})[1]":     # las dos mitades de la misma global
                halves.update(args)
                return [low[1], divisor]
            return [f"((int64_t){args[1]} << 32 | {args[0]})", divisor]
        return [args[0] if args else "/* r0:r1 */", divisor]

    for number in range(len(lines)):
        line = lines[number]
        if line.lstrip().startswith("//") or not DIVISION_CALL.search(line):
            continue
        k = -1
        while True:
            match = DIVISION_CALL.search(line)
            if not match:
                break
            k += 1
            args, end = split_call(line, match.end())
            dividend, divisor = pieces(match[1], args, number, k)
            remainder = DIVISIONS[match[1]]
            after = re.match(r",\s*(extraout_r\d(?:_\d+)?)\b", line[end + 1:])
            statement = re.fullmatch(rf"([ \t]*){re.escape(line[match.start():end + 1])};", line)
            assigned = re.fullmatch(rf"([ \t]*)(\w+) = {re.escape(line[match.start():end + 1])};", line)
            # (__aeabi_idivmod(a, b), extraout_r1 == 0): el resto, dentro de una condición.
            if remainder and after and line[match.start() - 1] == "(":
                new = f"{operand(dividend)} % {operand(divisor)}"
                line = line[:match.start()] + new + line[end + 1 + after.end():]
                touched.add(after[1])
            elif remainder and statement:
                found = remainder_variable(lines, number + 1, body)
                if not found:
                    break
                line = f"{statement[1]}{found} = {operand(dividend)} % {operand(divisor)};"
                touched.add(found)
                break
            else:
                new = f"({operand(dividend)} / {operand(divisor)})"
                line = line[:match.start()] + new + line[end + 1:]
                if assigned and remainder:
                    found = remainder_variable(lines, number + 1, body, (assigned[2],))
                    if found:
                        line += f"\n{assigned[1]}{found} = {operand(dividend)} % {operand(divisor)};"
                        touched.add(found)
                if assigned:
                    touched.add(assigned[2])
        lines[number] = re.sub(r"(= |return )\(([^()]*(?:\([^()]*\)[^()]*)*)\);$", r"\1\2;", line)
    lines = "\n".join(lines).split("\n")
    changed = True
    while changed:
        changed = False
        for index, line in enumerate(lines):
            name = re.match(r"[ \t]*(\w+) = ", line)
            if name and name[1] in touched:
                new = inline_once(lines, index, name[1])
                if new:
                    lines, changed = new, True
                    break
    body = "\n".join(lines)
    for name in halves:
        if len(re.findall(rf"\b{name}\b", body)) == 2:   # la declaración y la asignación
            body = remove_declaration(re.sub(rf"^[ \t]*{name} = .+;\n", "", body, flags=re.M), name)
    for name in sorted(touched):
        if len(re.findall(rf"\b{name}\b", body)) == 1:
            body = remove_declaration(body, name)
        elif re.fullmatch(r"extraout_r\d(?:_\d+)?|arg\d+_\d+", name) and declared_type(body, name):
            body = re.sub(rf"\b{name}\b", ghidra_name(body, declared_type(body, name)), body)
    return header[0] + body


def use_entry_registers(header, body):
    """in_r0 es lo que entró en r0, igual que el parámetro de esa posición: Ghidra los separa si cree que
    ese parámetro no se usa. noname_0 pasa a arg0, para que el nombre diga en qué register entró."""
    params = split_types(header["params"])
    if "::" not in header["name"]:
        # in_s0 también es r0: un .so de Thumb-1 no tiene VFP, y los float van en los registers normales.
        for kind, bank, register in re.findall(r"^(?![ \t]*return\b)[ \t]*([\w ]+?\*?) ?\bin_([rs])([0-3]);\n", body, re.M):
            register = int(register)
            if register == len(params):
                names = [param.split()[-1].lstrip("*") for param in params]
                if f"arg{register}" in names:   # radare empieza en arg1 = r0: se pasa todo a su register
                    new, body = names_by_register(names, body, 0)
                    params = [re.sub(r"\w+$", name, param) for param, name in zip(params, new)]
                params.append(f"{kind} arg{register}")
            if register < len(params):
                name = params[register].split()[-1].lstrip("*")
                body = remove_declaration(body, f"in_{bank}{register}")
                body = re.sub(rf"\bin_{bank}{register}\b", name, body)
    names = [param.split()[-1].lstrip("*") for param in params]
    if any(re.fullmatch(r"noname_\d+", name) for name in names):
        new, body = names_by_register(names, body, 0)
        params = [re.sub(r"\w+$", name, param) for param, name in zip(params, new)]
    return f"{header['type']}{header['name']}({','.join(params) or 'void'})" + body


def hidden_registers(header, body, read_argument):
    """uint noname_0; sin asignar, y f(noname_0, x): Ghidra no enseña lo que hay en ese register. Se lee de las
    instrucciones si f solo se llama una vez. Si no, queda f(/* r0 */, x)."""
    for name, register in re.findall(r"^[ \t]*\w+ (noname_(\d+)(?:_\d+)?);$", body, re.M):
        body = re.sub(rf"\b{name}\b", f"/* r{register} */", remove_declaration(body, name))

    lines = body.split("\n")
    for index, line in enumerate(lines):
        for call in re.finditer(rf"(?<![\w:.>])({CALLEE})\(([^()]*)\)", line) if "/* r" in line else ():
            args = call[2].split(",")
            order = call_order(body, call[1], index, call.start())
            for i, arg in enumerate(args):
                hidden = re.fullmatch(r"/\* r(\d) \*/", arg.strip())
                if hidden:
                    value = read_argument(header["name"], call[1], int(hidden[1]), order)
                    args[i] = (" " if arg.startswith(" ") else "") + argument_text(header, value) if value else arg
            lines[index] = lines[index].replace(call[0], f"{call[1]}({','.join(args)})")
    return "\n".join(lines)


def simplify_function(header, body, called, read_argument):
    params = [param.split()[-1].lstrip("*") for param in split_types(header["params"])] + ["this"]
    body = hidden_registers(header, body, read_argument)
    body = inline_calls(use_params(body, params))
    typed = typed_params(header, body, called)
    typed_list = split_types(typed[typed.index("(") + 1:typed.rindex(")")])
    body = fill_from_code(header, typed_list, body, called, read_argument)
    body = fill_from_registers(header, body)
    return typed_params(header, body, called) + body


def add_addresses(header, body, targets):
    """Encima del header y del primer uso de cada nombre, su dirección: pdg @ 0x52a10 la muestra en
    radare."""
    if not targets:
        return header[0] + body
    names = "|".join(re.escape(name) for name in sorted(targets, key=len, reverse=True))
    pattern = re.compile(rf"(?<![\w:.>])({names})(?![\w:])")
    shown, lines = set(), []
    for address, label in targets.get(header["name"], {}).items():
        shown.add(address)
        lines.append(f"// @0x{address:x}  {label}")
    lines.append(header[0] + body.split("\n", 1)[0])
    body = body.split("\n", 1)[1] if "\n" in body else ""
    for line in body.split("\n"):
        if not line.lstrip().startswith("//"):
            indent = line[:len(line) - len(line.lstrip())]
            for match in pattern.finditer(line):
                for address, label in targets[match.group(1)].items():
                    if address not in shown:
                        shown.add(address)
                        lines.append(f"{indent}// @0x{address:x}  {label}")
        lines.append(line)
    return "\n".join(lines)


def drop_if_unused(body, name):
    """Quita la declaración de una variable que ya nadie usa, también si es un array."""
    if len(re.findall(rf"\b{name}\b", body)) == 1:
        return re.sub(rf"^(?![ \t]*return\b)[ \t]*[\w ]+\*?\b{name}(?: \[\w+\])?;\n", "", body, flags=re.M)
    return body


def remove_aliases(body):
    """p = &g; *p = 1 es g = 1, si p no se usa de otra forma."""
    for variable, target in set(re.findall(r"^[ \t]*(\w+Var\d+) = &([\w:]+);$", body, re.M)):
        rest = remove_declaration(body, variable)
        if len(re.findall(rf"(?<![*\w]){variable} = ", rest)) != 1 or (
                len(re.findall(rf"\b{variable}\b", rest)) != 1 + len(re.findall(rf"\*{variable}\b", rest))):
            continue
        rest = re.sub(rf"^[ \t]*{variable} = &[\w:]+;\n", "", rest, flags=re.M)
        body = re.sub(rf"\*{variable}\b", target, rest)
    return body


def remove_stack_guard(body):
    """El compilador guarda __stack_chk_guard al entrar y lo compara al salir. No es lógica del juego."""
    guard = re.search(r"^[ \t]*(\w+) = __stack_chk_guard;\n", body, re.M)
    if not guard:
        return body
    check = re.compile(rf"^[ \t]*if \({guard[1]} != __stack_chk_guard\) \{{\n(?:[ \t]*//[^\n]*\n)*"
                       r"[ \t]*__stack_chk_fail\(\);\n[ \t]*\}\n", re.M)
    # También al revés: if (guarda == copia) { return; } __stack_chk_fail();
    inverted = re.compile(rf"^[ \t]*if \((?:__stack_chk_guard == {guard[1]}|{guard[1]} == __stack_chk_guard)\) \{{\n"
                          r"[ \t]*return;\n[ \t]*\}\n(?:[ \t]*//[^\n]*\n)*[ \t]*__stack_chk_fail\(\);\n", re.M)
    if not check.search(body) and not inverted.search(body):
        return body
    return drop_if_unused(inverted.sub("", check.sub("", body.replace(guard[0], "", 1))), guard[1])


def inline_copied_strings(body):
    """memcpy(&t, "texto", 0x34); f(&t): el texto se copia al stack solo para pasárselo a f."""
    for match in re.finditer(r'^[ \t]*memcpy\(&?(\w+),\s*("[^"\n]*"),\s*(0x[0-9a-f]+|\d+)\);\n', body, re.M):
        rest = body.replace(match[0], "", 1)
        if len(re.findall(rf"\b{match[1]}\b", rest)) == 2 and int(match[3], 0) > len(match[2]) - 2:
            rest = re.sub(rf"^(?![ \t]*return\b)[ \t]*[\w ]+\*?\b{match[1]}(?: \[\w+\])?;\n", "", rest, flags=re.M)
            return inline_copied_strings(re.sub(rf"&?\b{match[1]}\b", lambda _: match[2], rest))
    return body


def collapse_temporaries(body):
    """v = f(); g = v; if (v != 0) es g = f(); if (g != 0), si g no cambia mientras se usa v."""
    lines, i, names = body.split("\n"), 0, set()
    while i < len(lines) - 1:
        first = re.fullmatch(r"([ \t]*)(\w+Var\d+) = (.+);", lines[i])
        second = re.fullmatch(r"[ \t]*([\w:]+|\*\(\w+ \+ (?:0x[0-9a-f]+|\d+)\)) = (\w+Var\d+);", lines[i + 1])
        i += 1
        if (not first or not second or second[2] != first[2] or re.fullmatch(r"\w+Var\d+", second[1])
                or re.search(rf"\b{first[2]}\b", first[3])):
            continue
        temporary, target, end, safe = first[2], second[1], i + 1, True
        uses, reassigned = [], None
        while end < len(lines):
            # v = f(v): la asignación siguiente todavía lee el valor anterior, así que también cuenta.
            reassigned = re.match(rf"([ \t]*{temporary} = )(.*)", lines[end])
            if reassigned:
                break
            if re.search(rf"\b{temporary}\b", lines[end]):
                uses.append(end)
            if re.fullmatch(r"\w+:", lines[end].strip()) or re.search(rf"(?<![\w:]){re.escape(target)} = ", lines[end]):
                safe = not any(re.search(rf"\b{temporary}\b", line) for line in lines[end:])
                break
            end += 1
        # Dentro de un loop, v puede volver a leerse en la vuelta siguiente con otro valor: si se reasigna
        # dentro de un loop abierto después, o si el loop que la contiene la lee antes de la asignación.
        if reassigned and any(LOOP_HEAD.match(lines[k]) and indent_of(lines[end]) > indent_of(lines[k])
                              for k in range(i, end)):
            safe = False
        for k in range(i - 2, -1, -1):
            if (lines[k].strip() and indent_of(lines[k]) < indent_of(lines[i - 1]) and LOOP_HEAD.match(lines[k])
                    and all(not lines[j].strip() or LABEL.fullmatch(lines[j]) or indent_of(lines[j]) > indent_of(lines[k])
                            for j in range(k + 1, i - 1))):
                if any(re.search(rf"\b{temporary}\b", line) for line in lines[k:i - 1]):
                    safe = False
        if not safe or (uses and target.startswith("*")):   # un campo podría cambiar entre medias
            continue
        for use in uses:
            lines[use] = re.sub(rf"\b{temporary}\b", lambda _: target, lines[use])
        if reassigned:
            lines[end] = reassigned[1] + re.sub(rf"\b{temporary}\b", lambda _: target, reassigned[2])
        lines[i - 1] = f"{first[1]}{target} = {first[3]};"
        del lines[i]
        names.add(temporary)
    body = "\n".join(lines)
    for name in names:
        body = drop_if_unused(body, name)
    return body


def forward_strings(body):
    """v = "(I)V"; f(v): se sustituye donde se usa, si dentro de su bloque y hasta la siguiente asignación."""
    lines = body.split("\n")
    for name in set(re.findall(r'^[ \t]*(\w+Var\d+) = "[^"\n]*";$', body, re.M)):
        assigned = [i for i, line in enumerate(lines) if re.match(rf"[ \t]*{name} = ", line)]
        if not all(re.fullmatch(rf'[ \t]*{name} = "[^"\n]*";', lines[i]) for i in assigned):
            continue
        new, reached = list(lines), set()
        for start in assigned:
            text = re.search(r'"[^"\n]*"', lines[start])[0]
            indent = len(lines[start]) - len(lines[start].lstrip())
            for i in range(start + 1, len(lines)):
                if i in assigned or (lines[i].strip() and len(lines[i]) - len(lines[i].lstrip()) < indent):
                    break
                if re.search(rf"\b{name}\b", lines[i]):
                    new[i] = re.sub(rf"\b{name}\b", lambda _: text, lines[i])
                    reached.add(i)
        uses = {i for i, line in enumerate(lines) if re.search(rf"\b{name}\b", line) and i not in assigned
                and not re.fullmatch(rf"(?![ \t]*return\b)[ \t]*[\w ]+\*?\b{name};", line)}
        if uses == reached:
            lines = [line for i, line in enumerate(new) if i not in assigned]
            lines = drop_if_unused("\n".join(lines), name).split("\n")
    return "\n".join(lines)


def forward_globals(body):
    """v = g; ... *(v + 0x20): v es una copia de la global, si no hay ninguna llamada que pueda cambiarla
    mientras se usa, y nadie vuelve a leer v después."""
    lines, i = body.split("\n"), 0
    own_names = r"\w+Var\d+|local_\w+|arg\d+(?:_\d+)?|param_\d+|this|env|vm"
    while i < len(lines):
        match = re.fullmatch(r"([ \t]*)(\w+Var\d+) = ([A-Za-z_]\w*(?:::\w+)*);", lines[i])
        i += 1
        if not match or re.fullmatch(own_names, match[3]):
            continue
        variable, value = match[2], match[3]
        assignment = re.compile(rf"[ \t]*{variable} = (?!.*\b{variable}\b)")
        uses, end, safe = [], i, True
        for end in range(i, len(lines)):
            line = lines[end]
            if line.lstrip().startswith("//"):
                continue
            if assignment.match(line):
                safe = len(line) - len(line.lstrip()) <= len(match[1])   # si es más adentro, no vale siempre
                break
            if re.fullmatch(r"\w+:", line.strip()) or re.match(r"\s*(?:while|for|do|switch)\b|.*\bgoto\b", line):
                safe = False
                break
            if re.search(r"\w\(", line):
                # Una llamada puede cambiar la global: desde aquí, nadie debe leer v.
                for later in lines[end:]:
                    if assignment.match(later):
                        safe = len(later) - len(later.lstrip()) <= len(match[1])
                        break
                    if re.search(rf"\b{variable}\b", later):
                        safe = False
                        break
                break
            if re.search(rf"\b{variable}\b", line):
                uses.append(end)
        if not safe or not uses:
            continue
        for use in uses:
            lines[use] = re.sub(rf"\b{variable}\b", lambda _: value, lines[use])
        del lines[i - 1]
        i -= 1
    body = "\n".join(lines)
    for name in set(re.findall(r"^[ \t]*[\w ]+\*?\b(\w+Var\d+);$", body, re.M)):
        body = drop_if_unused(body, name)
    return body


def forward_constants(body):
    """v = 60.0f; *(this + 0xc) = v: una constante que solo se asigna una vez se sustituye donde se usa."""
    number = r"-?(?:0x[0-9a-f]+|\d+(?:\.\d+)?f?)"
    for name, value in re.findall(rf"^    (\w+Var\d+) = ({number});$", body, re.M):
        if len(re.findall(rf"(?<![*\w]){name} = ", body)) == 1 and f"&{name}" not in body:
            rest = re.sub(rf"^[ \t]*{name} = {re.escape(value)};\n", "", body, flags=re.M)
            rest = remove_declaration(rest, name)
            body = re.sub(rf"\b{name}\b", lambda _: value, rest)
    return body


def name_timeb(body):
    """ftime rellena un struct timeb: time en +0, millitm en +4, timezone en +6 y dstflag en +8. Cada campo
    está más arriba en el stack, así que Ghidra lo ve como otra variable."""
    call = re.search(r"\bftime\(&?(local_([0-9a-f]+))\);", body)
    if not call:
        return body
    for offset, field in ((4, "millitm"), (6, "timezone"), (8, "dstflag")):
        name = f"local_{int(call[2], 16) - offset:x}"
        body = re.sub(rf"^(?![ \t]*return\b)[ \t]*[\w ]+\*?\b{name}(?: \[\w+\])?;\n", "", body, flags=re.M)
        body = re.sub(rf"\b{name}\b", f"{call[1]}.{field}", body)
    declaration = rf"^(?![ \t]*return\b)([ \t]*)[\w ]+\*?\b{call[1]}(?: \[\w+\])?;$"
    return re.sub(declaration, rf"\1struct timeb {call[1]};", body, flags=re.M)


def negate(condition):
    match = re.fullmatch(r"([^&|]+?) (==|!=|<=|>=|<|>) ([^&|]+)", condition)
    opposite = {"==": "!=", "!=": "==", "<": ">=", ">=": "<", ">": "<=", "<=": ">"}
    return f"{match[1]} {opposite[match[2]]} {match[3]}" if match else f"!({condition})"


def block_end(lines, start):
    """La línea del } que cierra el bloque abierto en start."""
    closing = " " * (len(lines[start]) - len(lines[start].lstrip())) + "}"
    return next((i for i in range(start + 1, len(lines)) if lines[i] == closing), None)


def chain_length(lines, start):
    """Cuántos if hay uno dentro de otro, cada uno como lo último del anterior y sin else."""
    end = block_end(lines, start)
    if end is None or not re.fullmatch(r"[ \t]*if \(.*\) \{", lines[start]):
        return 0
    if lines[end + 1].strip().startswith("else"):
        return 0
    inner = " " * (len(lines[start]) - len(lines[start].lstrip()) + 4)
    if lines[end - 1] != inner + "}":
        return 1
    opening = next(i for i in range(end - 2, start, -1) if lines[i].startswith(inner) and lines[i][len(inner)] != " ")
    return 1 + chain_length(lines, opening)


def array_loop(body):
    """p = this; q = this + 2; do { *p = 0; *q = 1; p += 5; q += 5; } while (p != this + 0x23); recorre un array:
    5 palabras son 0x14 bytes por elemento, y 0x23 / 5 son 7 elementos. Se escribe con un índice y campos en bytes."""
    lines = body.split("\n")
    for start, line in enumerate(lines):
        if line.strip() != "do {" or re.search(r"\bi\b", body):
            continue
        indent = line[:len(line) - len(line.lstrip())]
        end = next((i for i in range(start + 1, len(lines)) if lines[i].startswith(indent + "}")), None)
        test = re.fullmatch(rf"{indent}\}} while \((\w+) != ([\w:]+) \+ (0x[0-9a-f]+|\d+)\);", lines[end]) if end else None
        if not test:
            continue
        inner = lines[start + 1:end]
        steps = dict(re.findall(r"^[ \t]*(\w+) = \1 \+ (\d+);$", "\n".join(inner), re.M))
        if test[1] not in steps or len(set(steps.values())) != 1 or any(re.search(r"\bgoto\b|^\w+:$", l) for l in inner):
            continue
        stride, base, offsets, first = int(steps[test[1]]), test[2], {}, start
        for variable in steps:
            # La inicialización tiene que estar justo antes del loop.
            found = next((i for i in range(start - 1, max(start - len(steps) - 1, -1), -1)
                          if re.fullmatch(rf"[ \t]*{variable} = {re.escape(base)}(?: \+ (\d+))?;", lines[i])), None)
            if found is None:
                break
            offsets[variable] = int(re.search(r"\+ (\d+);", lines[found])[1]) if " + " in lines[found] else 0
            first = min(first, found)
        declared = [re.search(rf"^[ \t]*(\w+) \*{variable};$", body, re.M) for variable in steps]
        sizes = {ELEMENT_SIZES.get(kind[1]) if kind else None for kind in declared}
        total = int(test[3], 0) - offsets.get(test[1], 0)
        if len(offsets) != len(steps) or len(sizes) != 1 or None in sizes or total % stride or total <= 0:
            continue
        size, kept = sizes.pop(), []
        for text in inner:
            if re.fullmatch(r"[ \t]*\w+ = \w+ \+ \d+;", text) and text.split()[0] in steps:
                continue
            for variable, offset in offsets.items():
                def field(match, variable=variable, offset=offset):
                    position = (offset + int(match[1] or 0)) * size
                    return f"{base}->array_{min(offsets.values()) * size:#x}[i].field_{position:#x}"
                text = re.sub(rf"\*{variable}\b|\b{variable}\[(\d+)\]", field, text)
                text = re.sub(rf"\b{variable}\b", lambda _: f"&{base}->array_{min(offsets.values()) * size:#x}[i]"
                              + (f".field_{offset * size:#x}" if offset else ""), text)
            kept.append(text)
        rest = lines[:first] + [f"{indent}for (int i = 0; i < {total // stride}; i++) {{"] + kept + [f"{indent}}}"] + lines[end + 1:]
        rest = [text for text in rest if not any(re.fullmatch(rf"[ \t]*{variable} = .*;", text) for variable in steps)]
        result = "\n".join(rest)
        if any(re.search(rf"\b{variable}\b", remove_declaration(result, variable)) for variable in steps):
            continue
        for variable in steps:
            result = remove_declaration(result, variable)
        return array_loop(result)
    return body


def flatten_guards(body):
    """if (a) { x; if (b) { y; if (c) {...} } } es lo mismo que if (!a) goto L_done; x; if (!b) goto L_done; y;...
    Solo en cadenas de 3 o más, que es cuando el código se sale de la pantalla."""
    lines, i, label = body.split("\n"), 0, None
    while i < len(lines):
        match = re.fullmatch(r"    if \((.*)\) \{", lines[i])
        end = block_end(lines, i) if match else None
        if end is not None and not lines[end + 1].strip().startswith("else"):
            continues = label and lines[end + 1] == f"{label}:"
            if continues or chain_length(lines, i) >= 3:
                if not continues:
                    count = sum(1 for line in lines if re.fullmatch(r"L_done\d*:", line))
                    label = f"L_done{count + 1 if count else ''}"
                    lines.insert(end + 1, f"{label}:")
                lines[i] = f"    if ({negate(match[1])}) goto {label};"
                lines[i + 1:end] = [line[4:] if line.startswith("    ") else line for line in lines[i + 1:end]]
                del lines[end]
        i += 1
    return "\n".join(lines)


def single_element_arrays(body):
    """int local_1c [2]; del que solo se usa local_1c[0] es una variable normal: local_1c[0] es su valor y
    local_1c, sin índice, su dirección."""
    for name in re.findall(r"^[ \t]*[\w ]+\*?\b(local_\w+) \[\d+\];$", body, re.M):
        indexes = set(re.findall(rf"\b{name}\[(\w+)\]", body))
        if indexes != {"0"}:
            continue
        body = re.sub(rf"^([ \t]*[\w ]+\*?\b{name}) \[\d+\];$", r"\1;", body, flags=re.M)
        body = re.sub(rf"(?<![&\w]){name}\b(?!\[|;$)", f"&{name}", body, flags=re.M)
        body = body.replace(f"&{name};", f"{name};").replace(f"{name}[0]", name)
    return body


def forward_addresses(body):
    """v = &g; f(v); ... v = &h; f(v);: una dirección no cambia, así que cada uso es &g o &h. Si v se usa con
    índices, como v[1], son campos de g: g.field_0x4."""
    if re.search(r"\bgoto\b|^\s*\w+:$|\b(?:while|for|switch)\b|\bdo \{", body, re.M):
        return body
    lines = body.split("\n")
    for variable in sorted(set(re.findall(r"^    (\w+Var\d+|arg\d+_\d+) = &[\w:]+;$", body, re.M))):
        writes = [i for i, line in enumerate(lines) if re.search(rf"(?<![\w*]){variable} = ", line)]
        if (any(not re.fullmatch(rf"    {variable} = &[\w:]+;", lines[i]) for i in writes) or f"&{variable}" in body
                or any(re.search(rf"\b{variable}\b", line) for line in lines[:writes[0]] if not re.match(r"\s*[\w ]+\*?\w+;$", line))):
            continue
        declared = re.search(rf"^[ \t]*([\w ]+?) \*{variable};$", body, re.M)
        size = ELEMENT_SIZES.get(declared[1]) if declared else None
        for k, start in enumerate(writes):
            target = lines[start].split("&", 1)[1][:-1]
            end = writes[k + 1] if k + 1 < len(writes) else len(lines)
            segment = "\n".join(lines[start + 1:end])
            if re.search(rf"\b{variable}\[", segment):
                if not size:
                    break
                segment = re.sub(rf"\b{variable}\[(\d+)\]", lambda m: f"{target}.field_{int(m[1]) * size:#x}", segment)
                segment = re.sub(rf"\*{variable}\b", f"{target}.field_0x0", segment)
            segment = re.sub(rf"\*\({variable} \+ (0x[0-9a-f]+|\d+)\)", lambda m: f"{target}.field_{int(m[1], 0):#x}", segment)
            segment = re.sub(rf"\*{variable}\b", target, segment)
            segment = re.sub(rf"\b{variable}\b", f"&{target}", segment)
            lines[start + 1:end] = segment.split("\n")
            lines[start] = None
        else:
            lines = [line for line in lines if line is not None]
            lines = [line for line in lines if not re.fullmatch(rf"[ \t]*[\w ]+\*?\b{variable};", line)]
            continue
        lines = body.split("\n")
    return "\n".join(lines)


def indent_of(line):
    return len(line) - len(line.lstrip())


def enclosing_blocks(lines, index):
    """Las líneas que abren los bloques donde está lines[index], de dentro hacia afuera."""
    found, level = [], indent_of(lines[index])
    for line in reversed(lines[:index]):
        if line.strip() and indent_of(line) < level and not LABEL.fullmatch(line):
            level = indent_of(line)
            found.append(line)
    return found


def overwritten_after(lines, start, variable):
    """1 si, a partir de lines[start], lo siguiente que le pasa a la variable en cualquier camino es una nueva
    asignación: su valor anterior ya no lo lee nadie. Sin saltos ni etiquetas por medio, y sin llegar al final
    de un loop, que volvería a empezar más arriba."""
    loops = [indent_of(line) for line in enclosing_blocks(lines, start) if LOOP_HEAD.match(line)]
    floor, inner = indent_of(lines[start]), []   # inner: los loops abiertos después, donde break no sale
    for line in lines[start + 1:]:
        text = line.strip()
        if not text or text.startswith("//"):
            continue
        while inner and indent_of(line) <= inner[-1] and not text.startswith("} while"):
            inner.pop()
        if text.startswith("} while") and inner and indent_of(line) == inner[-1]:
            inner.pop()
        if LABEL.fullmatch(line) or re.search(r"\bgoto\b", text) or (
                re.search(r"\b(?:break|continue)\b", text) and not inner):
            return False
        if any(indent_of(line) <= loop for loop in loops):
            return False
        floor = min(floor, indent_of(line))
        if LOOP_HEAD.match(line):
            inner.append(indent_of(line))
        if re.search(rf"\b{variable}\b", line):
            # Más adentro que algo abierto entre medias, la asignación podría no ejecutarse.
            return (indent_of(line) <= floor
                    and bool(re.fullmatch(rf"[ \t]*{variable} = (?!=)(?!.*\b{variable}\b).*;", line)))
    return True


def address_span(lines, start, writes):
    """Las líneas en las que la variable asignada en lines[start] sigue valiendo lo mismo: hasta la siguiente
    asignación o el final del bloque. Una etiqueta no corta si todos sus goto están dentro: se llega a ella
    con el mismo valor. Devuelve las líneas y si se cortó en un loop que vuelve a asignarla."""
    later = [w for w in writes if w > start]
    end = start + 1
    while end < len(lines) and end not in writes and (LABEL.fullmatch(lines[end]) or not (
            lines[end].strip() and indent_of(lines[end]) < indent_of(lines[start]))):
        end += 1
    looped = False
    if end < len(lines) and end in writes:
        loop = next((i for i in range(start + 1, end) if LOOP_HEAD.match(lines[i])
                     and indent_of(lines[end]) > indent_of(lines[i])), None)
        if loop is not None:
            end, looped = loop, True
    changed = True
    while changed:
        changed = False
        for i in range(start + 1, end):
            if LABEL.fullmatch(lines[i]):
                name = lines[i].strip()[:-1]
                sources = [j for j, line in enumerate(lines) if re.search(rf"\bgoto {name};", line)]
                if not all(start < j < end for j in sources):
                    end, changed = i, True
                    break
    return list(range(start + 1, end)), looped and (not later or end <= later[0])


def forward_global_addresses(body):
    """v = &g; ... v[3]: lo mismo que forward_addresses, también con saltos y loops. v vale &g desde la
    asignación hasta la siguiente (address_span). Si v se copia o recorre g, g es un array y v[3] es g[3]. Si
    no, g es una estructura: g.field_0xc. Con v = &g + i * 0x24, v apunta al elemento i: *(v + 4) es
    *(&g + i * 0x24 + 4), que global_fields escribe g[i].field_0x4."""
    lines = body.split("\n")
    pointer_use = r"\*{0}\b|\*\({0}\b|\b{0}\[|\b{0}->|(?<![*\w]){0} [-+] |(?<![\w*.>])(?:[(,] ?|= |return ){0}(?=[,);])"
    address = rf"&([A-Za-z_][\w:]*)(?: \+ ({INDEX}) \* (0x[0-9a-f]+|\d+))?"
    for variable in sorted({match[1] for match in re.finditer(rf"^[ \t]*(\w+Var\d+) = {address};$", body, re.M)}):
        kind = declared_type(body, variable)
        if not kind or f"&{variable}" in body or kind.count("*") > 1:
            continue
        size = ELEMENT_SIZES.get(kind.rstrip("* ")) if kind.endswith("*") else 1
        if not size:
            continue
        writes = [i for i, line in enumerate(lines) if re.search(rf"(?<![\w*.>]){variable} = (?!=)", line)]
        starts = [i for i in writes if re.fullmatch(rf"[ \t]*{variable} = {address};", lines[i])]
        # Un cursor: siempre la misma global, o la anterior más algo. Entonces recorre la global: es un array.
        targets = {re.fullmatch(rf"[ \t]*{variable} = {address};", lines[i])[1] for i in starts}
        steps = [i for i in writes if re.fullmatch(rf"[ \t]*{variable} = {variable} [-+] .+;", lines[i])]
        cursor = bool(steps) and len(targets) == 1 and len(starts) + len(steps) == len(writes)
        done = []
        for start in starts:
            match = re.fullmatch(rf"[ \t]*{variable} = {address};", lines[start])
            target, index, step = match[1], match[2], match[3]
            if len(writes) == 1:      # siempre vale lo mismo, también en los usos de más arriba
                span, looped = [i for i in range(len(lines)) if i != start], False
            else:
                span, looped = address_span(lines, start, writes)
            if index:
                # El elemento vale mientras no cambie lo que lee el índice.
                names = set(re.findall(r"[A-Za-z_]\w*", index))
                for k, i in enumerate(span):
                    if any(re.search(rf"(?<![\w.>]){name}\b[^=;]*? = (?!=)", lines[i]) for name in names):
                        span = span[:k]
                        break
            uses = [i for i in span if re.search(rf"\b{variable}\b", lines[i]) and not lines[i].lstrip().startswith("//")
                    and not re.fullmatch(rf"(?![ \t]*return\b)[ \t]*[\w ]+\*?\b{variable};", lines[i])]
            if looped:   # p = &g; e = p + 0x40; do {...}: el final de un recorrido se deja como está
                uses = [i for i in uses if not re.fullmatch(rf"[ \t]*\w+ = {variable} [-+] (?:0x[0-9a-f]+|\d+);", lines[i])]
            text = "\n".join(lines[i] for i in uses)
            if index:
                element = f"&{target} + {index} * {step}"
                rest = re.sub(rf"\*\({variable} \+ (?:0x[0-9a-f]+|\d+)\)|\*{variable}\b", "", text)
                if re.search(rf"\b{variable}\b", rest):
                    continue
                for i in uses:
                    line = re.sub(rf"\*\({variable} \+ (0x[0-9a-f]+|\d+)\)", rf"*({element} + \1)", lines[i])
                    lines[i] = re.sub(rf"\*{variable}\b", f"*({element})", line)
                done.append(start)
                continue
            array = kind.endswith("*") and (cursor or bool(re.search(
                rf"(?<![\w*.>])\w+ = {variable};|(?<![*\w]){variable} [-+] |\b{variable}\[(?!(?:0x[0-9a-f]+|\d+)\])", text)))
            alone = not re.search(rf"\b{variable}\b", re.sub(rf"\*{variable}\b", "", text))   # solo *v

            def field(offset):
                return f"{target}.field_{offset:#x}"
            for i in uses:
                line = lines[i]
                if array:
                    line = re.sub(rf"\*\({variable} \+ (0x[0-9a-f]+|\d+)\)", rf"{target}[\1]", line)
                    line = re.sub(rf"\*{variable}\b", f"{target}[0]", line)
                    line = re.sub(rf"\b{variable}\b", target, line)
                else:
                    line = re.sub(rf"\*\({variable} \+ (0x[0-9a-f]+|\d+)\)", lambda m: field(int(m[1], 0) * size), line)
                    line = re.sub(rf"\b{variable}\[(0x[0-9a-f]+|\d+)\]", lambda m: field(int(m[1], 0) * size), line)
                    line = re.sub(rf"\*{variable}\b", target if alone else f"{target}.field_0x0", line)
                    line = re.sub(rf"(?<![*\w])(?<![-+*/%] ){variable} \+ (0x[0-9a-f]+|\d+)(?=[;,)])",
                                  lambda m: f"&{field(int(m[1], 0) * size)}", line)
                    line = re.sub(rf"\b{variable}\b", f"&{target}", line)
                lines[i] = line
            done.append(start)
        # La asignación sobra si nadie más puede leer la dirección.
        rest = "\n".join(line for i, line in enumerate(lines) if i not in done and not line.lstrip().startswith("//"))
        unused = not re.search(pointer_use.format(variable), rest)
        for start in sorted(done, reverse=True):
            if len(writes) == 1 or unused or overwritten_after(lines, start, variable):
                del lines[start]
        lines = drop_if_unused("\n".join(lines), variable).split("\n")
    return "\n".join(lines)


WIDE_COPY = re.compile(r"^([ \t]*)(\w+Var\d+) = \(&([\w:]+)\)\[1\];\n[ \t]*([\w:]+)\.field_0x0 = \3;\n"
                       r"[ \t]*\4\.field_0x4 = \2;$", re.M)


def forward_scaled_indexes(body):
    """v = i * 8; ... *(&g + v): v es el offset del elemento i. Se escribe dentro, para que se vea
    g[i], si v solo se asigna ahí y nada de lo que lee cambia antes de usarla."""
    lines = body.split("\n")
    for index, line in enumerate(lines):
        match = re.fullmatch(rf"([ \t]*)(\w+Var\d+) = ({INDEX}) \* (0x[0-9a-f]+|\d+);", line)
        if not match or len(re.findall(rf"(?<![\w*.>]){match[2]} = (?!=)", body)) != 1:
            continue
        variable, value = match[2], f"{match[3]} * {match[4]}"
        names = set(re.findall(r"[A-Za-z_]\w*", match[3]))
        uses = [i for i, other in enumerate(lines) if i != index and re.search(rf"\b{variable}\b", other)
                and not re.fullmatch(rf"(?![ \t]*return\b)[ \t]*[\w ]+\*?\b{variable};", other)]
        if not uses or uses[0] < index:
            continue
        safe = True
        for other in lines[index + 1:uses[-1] + 1]:
            text = other.strip()
            if (LABEL.fullmatch(other) or (other.strip() and indent_of(other) < len(match[1]))
                    or re.search(r"(?<![\w:.>])[A-Za-z_][\w:.]*\(", text.split("=", 1)[-1])
                    or any(re.search(rf"(?<![\w.>]){name}\b[^=;]*? = (?!=)", text) for name in names)):
                safe = False
                break
        if safe and all(re.search(rf"[(+] ?{variable}(?=\)| \+)", lines[i]) for i in uses):
            for i in uses:
                lines[i] = re.sub(rf"\b{variable}\b", value, lines[i])
            lines[index] = None
    body = "\n".join(line for line in lines if line is not None)
    for name in set(re.findall(r"^[ \t]*[\w ]+\*?\b(\w+Var\d+);$", body, re.M)):
        body = drop_if_unused(body, name)
    return body


def inline_stack_copies(body):
    """local_38 = arg1; m_iventMode = local_38;: Ghidra ve el paso por el stack. Si nadie toma su dirección y
    solo se usa una vez, es m_iventMode = arg1."""
    lines = body.split("\n")
    for name in sorted(set(re.findall(r"^(?![ \t]*return\b)[ \t]*\w[\w ]*? (local_\w+);$", body, re.M))):
        if f"&{name}" in "\n".join(lines):
            continue
        writes = [i for i, line in enumerate(lines) if re.match(rf"[ \t]*{name} = (?!=)", line)]
        new = inline_once(lines, writes[0], name) if len(writes) == 1 else None
        if new:
            lines = drop_if_unused("\n".join(new), name).split("\n")
    return "\n".join(lines)


def join_wide_copies(body):
    """v = (&g)[1]; h.field_0x0 = g; h.field_0x4 = v: copia los 8 bytes de g en h, mitad a mitad, como hace
    el compilador con un long long. Es h = g."""
    for name in set(match[2] for match in WIDE_COPY.finditer(body)):
        copies = [match for match in WIDE_COPY.finditer(body) if match[2] == name]
        if len(re.findall(rf"\b{name}\b", body)) == 2 * len(copies) + 1:   # y la declaración
            body = WIDE_COPY.sub(lambda m: f"{m[1]}{m[4]} = {m[3]};" if m[2] == name else m[0], body)
            body = remove_declaration(body, name)
    return body


def drop_dead_stores(body):
    """iVar3 = &DAT_bc6fc; sin que nadie lea iVar3: queda de algo que ya se ha resuelto, como la base de la
    GOT o la cadena vacía de los string. Solo se quitan valores sin efectos: direcciones y números."""
    for name in set(re.findall(r"^[ \t]*[\w ]+\*?\b(\w+Var\d+);$", body, re.M)):
        pure = rf"^[ \t]*{name} = (?:&[\w:]+|-?0x[0-9a-f]+|-?\d+);\n"
        rest = re.sub(pure, "", body, flags=re.M)
        if rest != body and len(re.findall(rf"\b{name}\b", rest)) == 1:
            body = remove_declaration(rest, name)
    return body


def recognize_idioms(header, body):
    """Lo que el compilador expande y se reconoce por su forma."""
    body = reindent(body)
    body = join_wide_copies(forward_global_addresses(forward_addresses(single_element_arrays(body))))
    body = forward_scaled_indexes(body)
    body = remove_stack_guard(remove_aliases(body))
    body = inline_stack_copies(forward_strings(collapse_temporaries(inline_copied_strings(body))))
    body = name_timeb(forward_constants(forward_globals(body)))
    body = inline_calls(body)

    def string_destructor(match):
        leftovers.extend(match.group(3, 4))
        return f"{match[1]}std::string::~string(&{match[2]});\n"

    def inlined_destructor(match):
        name = match[3].split("::")[-1]
        return f"{match[1]}{name}::~{name}(&{match[2]});\n"

    def pointer_loop(match):
        indent, pointer, start, end, count, value, inner = match.groups()
        rest = body[:match.start()] + body[match.end():]
        if (re.search(rf"\b{pointer}\b", inner) or re.search(rf"\b{value} = ", inner)
                or any(len(re.findall(rf"\b{name}\b", rest)) > 1 for name in (pointer, end, value))):
            return match[0]
        leftovers.extend((pointer, end, value))
        inner = re.sub(rf"\b{value}\b", lambda _: f"{start.lstrip('&')}[i]", inner)
        return f"{indent}for (int i = 0; i < {count}; i++) {{\n{inner}{indent}}}\n"

    leftovers = []
    body = STRING_DESTRUCTOR.sub(string_destructor, body)
    body = INLINED_DESTRUCTOR.sub(inlined_destructor, body)
    if not re.search(r"\bi\b", body):
        body = POINTER_LOOP.sub(pointer_loop, body)
    for name in leftovers:
        body = drop_if_unused(body, name)
    return header[0] + flatten_guards(array_loop(drop_dead_stores(body)))


def constructor_result(body):
    """v = A::A(local_14, x); A::m(v): en ARM un constructor devuelve this, así que v es el propio objeto."""
    lines = body.split("\n")
    for index, line in enumerate(lines):
        match = re.fullmatch(r"([ \t]*)(\w+Var\d+) = (\w+)::\3\((&?local_\w+)(.*)\);", line)
        if not match:
            continue
        lines[index] = f"{match[1]}{match[3]}::{match[3]}({match[4]}{match[5]});"
        for later in range(index + 1, len(lines)):
            if re.match(rf"[ \t]*{match[2]} = ", lines[later]):
                break
            lines[later] = re.sub(rf"\b{match[2]}\b", match[4], lines[later])
    return "\n".join(lines)


def use_objects(header, body, called, read_argument):
    """Si se sabe de qué clase es una variable local, por su constructor o su destructor, las llamadas se
    escriben como en C++. También se quita el nombre de la clase dentro de sus propios métodos."""
    classes, declared, seen = {}, set(), set()
    body = constructor_result(body)
    for match in re.finditer(r"(\w+)::~?\1\(&?(local_\w+)\b", body):
        classes.setdefault(match[2], match[1])

    def rewrite(line, number):
        position = 0
        while True:
            match = OBJECT_CALL.search(line, position)
            if not match:
                return line
            position = match.end()
            owner, method, local = match.groups()
            known = classes.get(local)
            args, end = split_call(line, line.index("(", match.start(2)) + 1)
            statement = line.strip() == line[match.start():end + 1] + ";"
            whole = statement and (line[4] != " " or len(re.findall(rf"\b{local}\b", body)) == 1)
            types = called.get(f"{owner}::{method}")
            order = call_order(original, f"{owner}::{method}", number, match.start()) if line == originals[number] else None
            if (known and types is not None and owner != known and local not in seen and whole
                    and len(args) == len(types) + 2 and owner.split("::")[-1] != method):
                # Un método que devuelve un objeto: &local, this y sus argumentos. a + b, si es operator+.
                this = args[1].strip()
                this = this[1:] if this.startswith("&") else f"*{this}"
                rest = [arg.strip()[1:] if kind.endswith("&") and arg.strip().startswith("&") else arg.strip()
                        for arg, kind in zip(args[2:], types)]
                if method.startswith("operator") and len(rest) == 1:
                    new = f"{known} {local} = {this} {method[8:]} {rest[0]}"
                else:
                    new = f"{known} {local} = {this}.{method}({', '.join(rest)})"
                declared.add(local)
                line = line[:match.start()] + new + line[end + 1:]
                position = match.start() + len(new)
                continue
            if not known or (types is None and not method.startswith("~")) or len(args) - 1 > len(types or ()):
                continue
            rest = [arg[1:] if kind.endswith("&") and arg.startswith("&") else arg
                    for arg, kind in zip(args[1:], types or ())]
            # Un hueco como /* char const* */ también se intenta leer de las instrucciones.
            rest = [(argument_text(header, read_argument(header["name"], f"{owner}::{method}", 1 + i, order)) or arg)
                    if re.fullmatch(r"\s*/\* .* \*/\s*", arg) else arg for i, arg in enumerate(rest)]
            missing = [argument_text(header, read_argument(header["name"], f"{owner}::{method}", 1 + i, order))
                       or f"/* {kind} */" for i, kind in enumerate(types or ()) if i >= len(rest)]
            text = ",".join(rest + missing)
            if method.startswith("~"):
                new = f"{local}.~{known}()"
            elif owner == method == known and whole:
                new = f"{known} {local}({text})" if text else f"{known} {local}\0"
                declared.add(local)
            elif owner != known and local not in seen and whole:
                new = f"{known} {local} = {owner}::{method}({text})"   # devuelve el objeto a través de &local
                declared.add(local)
            elif owner == method:
                continue
            else:
                new = f"{local}.{method}({text})"
            line = line[:match.start()] + new + line[end + 1:]
            position = match.start() + len(new)

    lines, original = [], body
    originals = body.split("\n")
    is_declaration = re.compile(r"(?![ \t]*return\b)[ \t]*[\w ]+\*?\blocal_\w+(?: \[\w+\])?;$")
    prefixes = {header["name"].rpartition("::")[0], header["name"].rpartition("::")[0].split("::")[-1]} - {""}
    for number, line in enumerate(body.split("\n")):
        if not line.lstrip().startswith("//") and not is_declaration.fullmatch(line):
            line = rewrite(line, number)
            for prefix in prefixes:
                # Dentro de la clase, A::f(this, x) es f(x): sobra el this y el nombre de la clase.
                def own_call(match, prefix=prefix):
                    types = called.get(f"{prefix.split('::')[-1]}::{match[1]}")
                    args = split_call(line, match.start(2))[0]
                    if types is None or len(args) != len(types) + 1:
                        return match[0]
                    return f"{match[1]}("
                line = re.sub(rf"(?<![\w:]){re.escape(prefix)}::(\w+)\((this,\s*|this(?=\)))", own_call, line)
                line = re.sub(rf"(?<![\w:]){re.escape(prefix)}::", "", line)
            seen.update(re.findall(r"\blocal_\w+", line))
        lines.append(line)
    body = "\n".join(lines)
    for local, known in classes.items():
        declaration = rf"^(?![ \t]*return\b)([ \t]*)[\w ]+\*?\b{local}(?: \[\w+\])?;\n"
        replacement = "" if local in declared else rf"\g<1>{known} {local};\n"
        body = re.sub(declaration, replacement, body, flags=re.M)
    return header[0] + body.replace("\0", "")


def use_pointers(header, body, called):
    """Camera::f(x, a) con f(float) lleva un argumento de más: es el this, así que se escribe x->f(a). Si x
    solo se asigna una vez y siempre es de esa clase, también recibe su tipo."""
    classes = {}

    def rewrite(line):
        position = 0
        while True:
            match = re.compile(r"(?<![\w:.>])((?:\w+::)*\w+)::(\w+)\(").search(line, position)
            if not match:
                return line
            position = match.end()
            owner, method = match.groups()
            types = called.get(f"{owner}::{method}")
            args, end = split_call(line, match.end())
            if (types is None or len(args) != len(types) + 1 or owner.split("::")[-1] == method
                    or not re.fullmatch(r"&?[A-Za-z_][\w:]*", args[0]) or re.match(r"&(?:local_|\w+Stack_)", args[0])):
                continue
            rest = [arg[1:] if kind.endswith("&") and arg.startswith("&") else arg
                    for arg, kind in zip(args[1:], types)]
            if args[0].startswith("&"):          # &g->f() es g.f(): el objeto es la propia global
                new = f"{args[0][1:]}.{method}({','.join(rest)})"
                line = line[:match.start()] + new + line[end + 1:]
                position = match.start() + len(new)
                continue
            new = f"{args[0]}->{method}({','.join(rest)})"
            classes.setdefault(args[0], set()).add(owner.split("::")[-1])
            line = line[:match.start()] + new + line[end + 1:]
            position = match.start() + len(new)
    lines = [line if line.lstrip().startswith("//") else rewrite(line) for line in body.split("\n")]
    body = "\n".join(lines)
    for variable, owners in classes.items():
        if len(owners) == 1 and len(re.findall(rf"(?<![*\w]){variable} = ", body)) == 1:
            kind = next(iter(owners))
            declaration = rf"^(?![ \t]*return\b)([ \t]*)[\w ]+\*?\b{variable};$"
            body = re.sub(declaration, rf"\g<1>{kind} *{variable};", body, flags=re.M)
            name = kind[0].lower() + kind[1:]
            if re.fullmatch(r"arg\d+_\d+|\w+Var\d+", variable) and not re.search(rf"\b{name}\b", body):
                body = re.sub(rf"\b{variable}\b", name, body)
    return header[0] + body


def split_float_variables(header, body):
    """Ghidra usa una variable por register. Si un register guarda un float y luego un int, son dos variables:
    iVar3 sigue siendo el int y fVar3 pasa a ser el float. Se sigue línea a línea, con cada rama del if por
    separado. Con saltos o loops no se puede saber qué camino llegó, así que no se toca."""
    if re.search(r"^\s*(?:\w+:$|.*\bgoto\b|while\b|for\b|do\b|switch\b)", body, re.M):
        return header[0] + body
    # Solo las variables que reciben los dos tipos, y siempre con una asignación clara.
    def is_float(value, state):
        if "(int)" in value or re.search(r" (?:[<>]=?|[=!]=) ", value):   # una comparación da 0 o 1
            return False
        return value.startswith("(float)") or bool(re.search(r"\d\.\d*f\b", value)) or any(
            state.get(name) == "f" and re.search(rf"\b{name}\b", value) for name in state)

    candidates = set(re.findall(r"^[ \t]*(\w+Var\d+) = (?:\(float\)|[^;\n]*\d\.\d*f\b)", body, re.M))
    if not candidates:
        return header[0] + body
    original = body.split("\n")

    def rename(text, state):
        for name in candidates:
            if state.get(name) == "f":
                text = re.sub(rf"\b{name}\b", "f" + name[1:], text)
        return text

    def follow():
        """Devuelve las líneas nuevas y las variables que se leen sin saber de qué tipo son."""
        lines, state, stack, unclear = list(original), {}, [], set()

        def check(text):
            unclear.update(name for name in candidates
                           if state.get(name) == "?" and re.search(rf"\b{name}\b", text))

        for index, line in enumerate(original):
            stripped = line.strip()
            if not stripped or stripped.startswith("//") or stripped == "else {":
                continue
            if stripped == "}":
                following = next((other.strip() for other in original[index + 1:] if other.strip()), "")
                if following == "else {":            # la otra rama empieza como estaba antes del if
                    stack[-1][1], state = dict(state), dict(stack[-1][0])
                    continue
                saved, branch = stack.pop()
                other = branch if branch is not None else saved
                state = {name: state.get(name) if state.get(name) == other.get(name) else "?"
                         for name in set(state) | set(other)}
                continue
            if stripped.endswith("{"):
                check(line)
                lines[index] = rename(line, state) if stripped.startswith("if") else line
                stack.append([dict(state), None])
                continue
            assigned = re.fullmatch(r"([ \t]*)(\w+) = (.+);", line)
            if assigned and assigned[2] in candidates:
                check(assigned[3])
                value = rename(assigned[3], state)
                floating = is_float(assigned[3], state)
                state[assigned[2]] = "f" if floating else None
                lines[index] = f"{assigned[1]}{'f' + assigned[2][1:] if floating else assigned[2]} = {value};"
            else:
                check(line)
                lines[index] = rename(line, state)
        return lines, unclear

    while True:
        lines, unclear = follow()
        if not unclear:
            break
        candidates -= unclear
    body = "\n".join(lines)
    for name in candidates:
        if re.search(rf"\bf{name[1:]}\b", body):
            declaration = rf"^(?![ \t]*return\b)([ \t]*)([\w ]+\*?\b{name};)$"
            body = re.sub(declaration, rf"\1\2\n\1float f{name[1:]};", body, flags=re.M)
            body = drop_if_unused(body, name)
    return header[0] + body


def type_floats(body):
    """Una variable a la que solo se le asignan valores float es float, aunque Ghidra diga otra cosa."""
    known = set(re.findall(r"^[ \t]*float (\w+);$", body, re.M))
    for _ in range(3):
        for kind, name in re.findall(r"^(?![ \t]*return\b)[ \t]*([\w ]+?) (\w+);$", body, re.M):
            values = re.findall(rf"^[ \t]*{name} = (.+);$", body, re.M)
            def is_float(value):
                return (value.startswith("(float)") or re.search(r"\d\.\d*f\b", value)
                        or any(re.search(rf"\b{other}\b", value) for other in known)) and "(int)" not in value \
                        and not re.search(r" (?:[<>]=?|[=!]=) ", value)
            if values and kind != "float" and all(is_float(value) for value in values):
                known.add(name)
                body = re.sub(rf"^(?![ \t]*return\b)([ \t]*)[\w ]+ {name};$", rf"\1float {name};", body, flags=re.M)
    return body


ELEMENT_ADDRESS = re.compile(r"(?<!\*\()(?<![-+*/] )\b(?:this \+ (\w+) \* (?:0x[0-9a-f]+|\d+)|(\w+) \* (?:0x[0-9a-f]+|\d+) \+ this)"
                             r"(?=\s*[;),])")


def array_element(text):
    """this + i * 0x14 es la dirección del elemento i de un array al principio del objeto. Escrito así parecería
    aritmética de pointers de C++, que contaría objetos enteros. El índice es el mismo cuente lo que cuente this."""
    return ELEMENT_ADDRESS.sub(lambda m: f"&this->array_0x0[{m[1] or m[2]}]", text)


GLOBAL = r"(?!local_|\w*[sS]tack)[A-Za-z_][\w:]*"
INDEX = r"[\w.:]+(?:\[[^\[\]\n]*\]|->\w+)*"
GLOBAL_FIELD = re.compile(rf"\*\(&({GLOBAL}) \+ (0x[0-9a-f]+|\d+)\)")
GLOBAL_ELEMENT = re.compile(rf"\*\((?:&({GLOBAL}) \+ ({INDEX}) \* (0x[0-9a-f]+|\d+)|({INDEX}) \* (0x[0-9a-f]+|\d+) \+ &({GLOBAL}))"
                            r"(?: \+ (0x[0-9a-f]+|\d+))?\)")


def global_fields(header, body):
    """*(&g + 0xc) lee el byte 0xc de g: g.field_0xc. *(&g + i * 8 + 4) es el campo 4 del elemento i de un
    array de elementos de 8 bytes: g[i].field_0x4. Escrito así parecería aritmética de pointers de C, que
    contaría elementos enteros. Si en la función se ve que g tiene elementos de 0x24 bytes, g.field_0x44 es
    g[1].field_0x20: el compilador ya hizo la cuenta con un índice fijo."""
    sizes = {}
    for match in GLOBAL_ELEMENT.finditer(body):
        name, size = (match[1], match[3]) if match[1] else (match[6], match[5])
        sizes[name] = int(size, 0) if sizes.get(name, int(size, 0)) == int(size, 0) else None

    def element(match):
        name, index, size = (match[1], match[2], match[3]) if match[1] else (match[6], match[4], match[5])
        size, offset = int(size, 0), int(match[7] or "0", 0)
        if offset >= size:
            return match[0]
        return f"{name}[{index}]" if size == 4 and not match[7] else f"{name}[{index}].field_{offset:#x}"

    def field(match):
        size, offset = sizes.get(match[1]), int(match[2], 16)
        if not size or size == 4:
            return match[0]
        return f"{match[1]}[{offset // size}].field_{offset % size:#x}"
    body = GLOBAL_ELEMENT.sub(element, body)
    body = GLOBAL_FIELD.sub(lambda m: f"{m[1]}.field_{int(m[2], 0):#x}", body)
    # p = &g + 0x18 es la dirección del byte 0x18 de g.
    body = re.sub(rf"(?<![*\w(])(?<![-+*/%] )&({GLOBAL}) \+ (0x[0-9a-f]+|\d+)(?=[;,)])",
                  lambda m: f"&{m[1]}.field_{int(m[2], 0):#x}", body)
    return header[0] + re.sub(rf"(?<![\w.>])({GLOBAL})\.field_0x([0-9a-f]+)\b", field, body)


def rename_register_locals(header, body):
    """arg1_01 sin ser un parámetro es una variable más: Ghidra la llama así porque usa el register de arg1.
    Pasa a llamarse como las demás, por su tipo: uVar18."""
    for name in re.findall(r"^(?![ \t]*return\b)[ \t]*[\w ]+?\**\b(arg\d+(?:_\d+)?);$", body, re.M):
        kind = declared_type(body, name)
        if kind and not re.search(rf"\b{name}\b", header["params"]):
            body = re.sub(rf"\b{name}\b", ghidra_name(body, kind), body)
    return header[0] + body


def float_constants(text):
    """0x3f800000 es 1.0f si como float es un número redondo. Con un solo bit a 1, como 0x40000000, puede ser una marca."""
    def replace(match):
        value = int(match[0], 16)
        text = constant_text(value)
        return match[0] if value & (value - 1) == 0 or text.startswith("0x") else text
    return re.sub(r'"[^"\n]*"|\b0x[0-9a-f]{8}\b', lambda m: m[0] if m[0][0] == '"' else replace(m), text)


def tidy(text):
    """Solo cambia cómo se ve el texto, no lo que dice."""
    text = float_constants(text)
    swapped = {"<": ">", ">": "<", "<=": ">=", ">=": "<=", "==": "==", "!=": "!="}
    text = re.sub(r"\((-?(?:0x[0-9a-f]+|\d+)) (<=|>=|==|!=|<|>) ([^()&|\n]+)\)",
                  lambda m: f"({m[3]} {swapped[m[2]]} {m[1]})", text)
    text = re.sub(r"((?<![<>])[=!]?= |return )0xffffffff\b", r"\g<1>-1", text)
    text = re.sub(r"\b(?:code_r0x|joined_r0x|LAB_)0*([0-9a-f]+)\b", r"L_\1", text)
    text = re.sub(r"^([ \t]*)([^\s=][^=\n]*?) = \2 ([-+*/&|^]) ([^\s;]+);$", r"\1\2 \3= \4;", text, flags=re.M)
    text = re.sub(r"(= |return )\(([^()]*)\);$", r"\1\2;", text, flags=re.M)
    text = re.sub(r"\((int|uint|float|double)\)\(([\w:.>-]+)\)", r"(\1)\2", text)
    text = re.sub(r"^([ \t]*[^\s=][^=\n]*?) \+= -(\S+;)$", r"\1 -= \2", text, flags=re.M)
    text = re.sub(r" \+ -(0x[0-9a-f]+|\d+)\b", r" - \1", text)
    # 0x41c64e6d * x + 0x3039 pasa a x * 0x41c64e6d + 0x3039: la constante detrás, como se escribe a mano.
    operand = r"\*?[A-Za-z_][\w:.]*(?:\[[^\[\]\n]*\]|->\w+)*"
    text = re.sub(rf'"[^"\n]*"|(^|[(,] ?|[=!<>]=? |[-+&|^] |return )(0x[0-9a-f]+|\d+) \* ({operand})(?![\w(\[.>])',
                  lambda m: m[0] if m[0].startswith('"') else f"{m[1]}{m[3]} * {m[2]}", text, flags=re.M)
    # 0xfff0fff7 & x pasa a x & 0xfff0fff7. Solo tras =, ( o una coma, y con algo que no ligue más detrás:
    # a == 0xff & x es (a == 0xff) & x, y no se puede cambiar.
    text = re.sub(rf'"[^"\n]*"|((?:^|(?<![=!<>])= |[(,] ?|return |&& |\|\| ))(0x[0-9a-f]+|\d+) ([&|^]) ({operand})'
                  r"(?=;|\)|,| [&|^] | && | \|\| )",
                  lambda m: m[0] if m[0].startswith('"') else f"{m[1]}{m[4]} {m[3]} {m[2]}", text, flags=re.M)
    # -(c) | 0xb4 vale -1 si c se cumple, y 0xb4 si no: el compilador evita así un salto.
    text = re.sub(r"(= |return )-\(([^()\n]+)\) \| (0x[0-9a-f]+|\d+);", r"\1(\2) ? -1 : \3;", text)
    # ((x >> 0x1f) >> 0x11) + x >> 0xf es x / 0x8000 con signo: el compilador suma 0x7fff a los negativos
    # para que la división redondee hacia 0, como en C.
    def signed_division(match):
        shift, right = int(match[2], 0), int(match[3], 0)
        return f"{match[1]} / {1 << right:#x}" if shift + right == 32 else match[0]
    text = re.sub(r"(?:(?<== )|(?<=\()|(?<=, )|(?<=return )|(?<=[<>] )|(?<=[<>=!]= )|(?<=&& )|(?<=\|\| ))"
                  r"\(\((\w+) >> 0x1f\) >> (0x[0-9a-f]+|\d+)\) \+ \1 >> (0x[0-9a-f]+|\d+)"
                  r"(?=;|\)|,| [=!<>]=? | && | \|\| | [&|^] )", signed_division, text)
    text = re.sub(r'"[^"\n]*"|,(?=\S)', lambda m: m[0] if m[0] != "," else ", ", text)
    text = re.sub(r"[ \t]+\n", "\n", text)
    text = re.sub(r"^(\w[^\n]*\))\n+\{\n", r"\1 {\n", text, flags=re.M)
    text = re.sub(r"\n    return;\n\}", "\n}", text)
    text = re.sub(r"\{\n\n", "{\n", text)
    # Un constructor no tiene tipo, y el return this solo existe por la convención de ARM.
    text = re.sub(r"^((?:\w+::)*(\w+)::\2\([^()\n]*\) \{\n)", r"\1", text, flags=re.M)
    text = re.sub(r"^\w[\w *]*[ *]((?:\w+::)*(\w+)::~?\2\([^()\n]*\) \{)", r"\1", text, flags=re.M)
    text = re.sub(r"^((?:\w+::)*(\w+)::\2\([^()\n]*\) \{\n(?:.*\n)*?)    return this;\n\}", r"\1}", text, flags=re.M)
    return re.sub(r"\n{3,}", "\n\n", reindent(text))


def clean(text, names, library, own, force_jni):
    text = re.sub(r"\(\*\*\*([\w\[\]]+)\)\(", r"(**(*\1 + 0x0))(", unwrap_lines(text))
    text = re.sub(r"^([ \t]*(?:if|while|for|else if)\b.*\))\n[ \t]*\{$", r"\1 {", text, flags=re.M)   # if (...)\n{
    # Ghidra escribe en decimal los offsets menores de 10: (**(*x + 4))() es la entrada 0x4.
    text = re.sub(r"\*\(\*([\w\[\]]+) \+ (\d+)\)", lambda m: f"*(*{m[1]} + {int(m[2]):#x})", text)
    if library:
        text = resolve_kuser_helpers(text, library)
        text = resolve_literals(text, library)
        text = resolve_got(text, library)
        text = resolve_split_literals(text, library)
        text = resolve_got(text, library)     # la base de la GOT puede venir de un literal partido
        text = resolve_words(text, library)
        text = resolve_got(text, library)     # y su offset, de un literal ya leído
    text = drop_leftovers(text)
    text, renames = name_calls(text, names, force_jni)
    text, called, targets = name_methods(text, library)
    text = apply_renames(text, renames)
    text = clean_switches(name_jni_variables(clean_stack(join_doubles(format_arguments(text)))))
    text = each_function(text, fix_signature)
    text = each_function(text, use_entry_registers)
    text = each_function(text, fake_registers)

    def read_argument(caller, callee, register, order=None):
        """Un argumento según las instrucciones del .so, si caller y callee tienen una sola dirección."""
        callers, callees = targets.get(caller, {}), targets.get(callee, {})
        if not library or len(callers) != 1 or len(callees) != 1:
            return None
        return library.call_argument(next(iter(callers)), next(iter(callees)), register, order)
    text = each_function(text, lambda header, body: simplify_function(header, body, called, read_argument))
    text = each_function(text, lambda header, body: header[0] + fill_jni_calls(header, body, read_argument))

    def read_helper(caller, helper, register, order):
        """Como read_argument, para las funciones del compilador: no llevan comentario con su dirección."""
        callers, found = targets.get(caller, {}), library.function(f"sym.{helper}") if library else None
        if not found or found[2] is None or len(callers) != 1:
            return None
        return library.call_argument(next(iter(callers)), found[2], register, order)
    text = each_function(text, lambda header, body: use_division(header, body, read_helper))
    vm_global = re.search(r"\bvm = DAT_([0-9a-f]+);|\bGetJavaVM\(&DAT_([0-9a-f]+)\)", text)
    if vm_global and int(vm_global[1] or vm_global[2], 16) not in own:
        own = {**own, int(vm_global[1] or vm_global[2], 16): "g_vm"}
    text, named = name_addresses(text, own, library)
    text = fill_tested_method(text)
    text = fold_comparisons("\n".join(use_operators(line) for line in text.split("\n")))
    text = re.sub(r"\bu?int(?:8|16)_t (\w+)(?=[,)]|;)", r"int \1", text)
    for name, found in named.items():
        targets.setdefault(name, {}).update(found)
    text = each_function(text, recognize_idioms)
    text = each_function(text, lambda header, body: add_addresses(header, body, targets))
    text = each_function(text, lambda header, body: use_objects(header, body, called, read_argument))
    text = each_function(text, lambda header, body: use_pointers(header, body, called))
    text = each_function(text, split_float_variables)
    text = re.sub(r"\*this\b(?! = &vtable)", "this->field_0x0", type_floats(text))
    found = True
    while found:   # *(*(x + 0x8c) + 8) es x->field_0x8c->field_0x8: de dentro hacia afuera
        text, found = FIELD.subn(lambda m: f"{m[1]}->field_{int(m[2], 0):#x}", text)
    text = each_function(array_element(text), global_fields)
    text = each_function(text, rename_register_locals)
    return tidy(text)


def main():
    parser = argparse.ArgumentParser(description="Clean r2ghidra pseudocode read from standard input")
    parser.add_argument("so", nargs="?", help="the .so, to resolve strings and globals")
    parser.add_argument("--names", default=DEFAULT_NAMES, help="the JNI names file (default: %(default)s)")
    parser.add_argument("--jni", action="store_true", help="treat every call through a table as JNI")
    parser.add_argument("--rules", help=f"the rules file with your symbols (default: {DEFAULT_RULES}, if it exists)")
    options = parser.parse_args()
    text = re.sub(r"\x1b\[[0-9;]*m", "", sys.stdin.read())   # radare2 puede añadir colores
    library = Library(options.so) if options.so else None
    rules = options.rules or (DEFAULT_RULES if os.path.exists(DEFAULT_RULES) else None)
    try:
        own = read_rule_symbols(rules) if rules else {}
    except OSError as problem:
        sys.exit(f"cannot read the rules file: {problem}")
    names = read_names(options.names)
    # Cada función por separado: Ghidra repite nombres como iVar1, y cada una puede usar JNI o no.
    starts = [match.start() for match in HEADER.finditer(text)]
    pieces = [text[a:b] for a, b in zip([0] + starts, starts + [len(text)])]
    sys.stdout.write("".join(clean(piece, names, library, own, options.jni) for piece in pieces))


if __name__ == "__main__":
    main()
