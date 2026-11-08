herramientas en Python, solo con la library estándar:
- console.py: la consola de debug, para conectarse a `./loader --debug` (doc/debug.es.md).
- clean_pseudo.py: limpia el pseudocode de r2ghidra: pone nombre a las llamadas y a las variables globales, y resuelve los strings y las reglas symbol.
- vtable.py: a qué puede llegar una llamada virtual: la función que hay en una posición de cada vtable de un .so.
