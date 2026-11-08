#!/usr/bin/env python3
"""tools/vtable.py: a qué puede llegar una llamada virtual: la función que hay
en una posición de cada vtable de un .so.

  python3 tools/vtable.py game.so 0x24
"""
# El código compilado solo dice la posición: 0x24. Qué función es depende de la
# clase real del objeto, así que este programa muestra esa posición en todas las
# clases que tienen vtable. Una vtable empieza con dos palabras antes de la
# primera entrada, y por eso se suma 8.
import argparse
import bisect
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from clean_pseudo import Library, demangle, read_symbols


def main():
    parser = argparse.ArgumentParser(description="Show the function at one position of every vtable")
    parser.add_argument("so", help="the .so")
    parser.add_argument("position", help="the position in the vtable, as in vcall_0x24: 0x24")
    options = parser.parse_args()
    position = int(options.position, 0)
    library = Library(options.so)
    symbols = read_symbols(library)
    functions = sorted((value & ~1, size, name) for name, (value, size, kind) in symbols.items() if kind == 2)
    starts = [f[0] for f in functions]

    def function_at(address):
        i = bisect.bisect_right(starts, address) - 1
        if i >= 0 and (not functions[i][1] or address < functions[i][0] + functions[i][1]):
            return demangle(functions[i][2])
        return "?"

    shown = 0
    for name, (value, size, kind) in sorted(symbols.items()):
        if not name.startswith("_ZTV"):
            continue
        entry = value + 8 + position
        if size and entry + 4 > value + size:
            continue   # esta clase tiene menos métodos
        target = library.slots.get(entry)
        if target and not target.startswith("DAT_"):
            if target in symbols:
                result = f"@0x{symbols[target][0] & ~1:x}  {demangle(target)}"
            else:
                result = f"{demangle(target)}  (imported)"
        else:
            address = int(target[4:], 16) if target else library.word(entry)
            if not address:
                result = "0 (no function)"
            else:
                result = f"@0x{address & ~1:x}  {function_at(address & ~1)}"
        print(f"{demangle(name)[len('vtable for '):]}: {result}")
        shown += 1
    if not shown:
        print("no vtable symbols found: use the running game (--trace, --callers or gdb) instead")


if __name__ == "__main__":
    main()
