# Debug

[English](debug.md) | Español

## La consola

```
./loader --debug          # el juego corre y puedes conectarte cuando quieras
./loader --debug pause    # el juego te espera antes de su primer método (o --debug=pause)
python3 tools/console.py  # en otra terminal
```

La consola escucha en `resources/loader.sock`. Los comandos se atienden en los
puntos seguros, cuando el juego está parado: mientras lees o escribes su
memoria, no corre nada. Hay dos:

- cuando vuelve un método del juego que llamó `app_main` (`init`, `usleep`...);
- al final de cada turno de un thread del juego, aunque `app_main` esté en un
  `usleep` largo. Su nombre es `thread`: `pause thread` para en cada turno, y
  `run 1` avanza uno.

Si el juego se queda bloqueado sin llegar a ninguno, la consola no responde
hasta que llegue.

### El punto seguro de cada turno

Así llega la consola al final de cada turno de un thread del juego:

```
thread del juego: run() -> Thread::sleep -> usleep
  -> context_switch al scheduler
scheduler: run_thread -> safe_point()
  PC (dos procesos):
    loader-arm -> TO_HOST_SAFE_POINT por el fd 4 -> se queda esperando
    loader (en wait_for_arm_result) -> debug_after_call("thread")
      -> watch, pausas y comandos de la consola
      -> si está en pausa, espera aquí hasta resume o run
    loader -> TO_ARM_NATIVE_DONE por el fd 3
    loader-arm sigue: el siguiente thread, o vuelve al principal
  Pi o NATIVE=1 (un proceso):
    debug_after_call("thread") directamente
```

Mientras loader atiende la consola, loader-arm espera en `serve_host_requests` y
puede contestarle, por ejemplo para buscar un símbolo. Con el loop de tamatama
son unos 32 puntos seguros por segundo: dos `usleep` por cada vuelta de ~62 ms.

Si el juego no crea threads, no hay turnos y solo queda el primer punto seguro.

Es opcional: `make DEBUG_CONSOLE=0` compila sin ella, para sistemas sin sockets
Unix. Entonces `--debug` se ignora, y los watch siguen funcionando por el log.

## Comandos

`help` los muestra y `help COMANDO` explica uno. `guide` explica paso a paso
cómo encontrar una variable.

| Grupo | Comandos |
|---|---|
| Búsqueda | `find TIPO VALOR\|any`, `next VALOR\|changed\|same\|up\|down`, `results [CUÁNTOS]` |
| Memoria | `read LUGAR [TIPO] [CUÁNTOS]`, `write LUGAR [TIPO] VALOR`, `dump LUGAR [BYTES]` |
| Seguimiento | `watch NOMBRE LUGAR [TIPO]`, `unwatch NOMBRE`, `watches` |
| Ejecución | `pause [MÉTODO]`, `unpause MÉTODO`, `run [CUÁNTOS]`, `resume`, `info` |
| Nombres (solo console.py) | `save NOMBRE LUGAR [TIPO]`, `forget NOMBRE`, `names` |

Atajos: `f` find, `n` next, `l` results, `r` read, `w` write, `d` dump, `c`
resume, `p` pause, `i` info, `q` exit.

**LUGAR**: `@0x4AD0` una posición en el `.so`, `0x6080549c` una dirección
absoluta, `g_score` un símbolo, `#1` un resultado de la búsqueda, o un nombre
guardado con `save`. Detrás, `+0x18` suma y `->0x10` lee un pointer y le suma,
como en C: `g_world->0x10->4`.

**TIPO**: `i8 u8 i16 u16 i32 u32 f32 ptr str`; `i32` si no se indica.

Los nombres guardados quedan en `.loader_console.json`, también para la próxima
vez.

## Reglas para debug

En un fichero de reglas (`--rules FICHERO` o `rules/{md5}.txt`):

```
symbol g_rand  @0xc01c4           # un nombre para un lugar que el .so no exporta
watch  rand    g_rand  u32        # avisa de cada cambio de esta variable
trap   rand    g_rand  u32 0      # qué instrucción la pone a 0
pause  init                       # para cada vez que vuelve este método (necesita --debug)
trace  on                         # registra cada llamada a una función con hook
```

Sin la consola, los cambios de los watch van al log; una regla `pause` solo
avisa.

## Quién escribe una variable: trap

Un watch dice cuándo cambia una variable; un trap dice además qué instrucción
del .so la escribió. Solo existe como regla (no en la consola) y avisa en el
log:

```
trap NOMBRE LUGAR [TIPO] [VALOR]
```

LUGAR y TIPO son los de `watch`. Con VALOR, escrito como lo muestra un watch,
solo avisa cuando la variable toma ese valor; sin él, de cada cambio. Por
ejemplo:

```
trap m_endDraw  m_endDraw  u32 0
```

```
[trap] m_endDraw at 0x608c1f88 = 0, waiting for 0
[trap] m_endDraw 2 -> 0, written at 0x60838882 (@0x38882 Clase::metodo+0x3e) (lr 0x6083a54c (@0x3a54c Clase::otro+0xd8)) by step
```

`written at` es la instrucción que escribió el valor: mírala en radare, con
`e asm.bits=16` si el código es Thumb. `lr` es su registro lr en ese momento:
si la escritura está en una función hoja (un setter), es quien la llamó. `by` dice quién corría: la llamada de
app_main (`init`, `step`...) o `thread N`, el número que sale en `[thread] N:`
al crearse el thread.

Cómo funciona (src/arm/trap.c): con la dirección el loader solo puede leer la
variable, así que su página (4096 bytes) se deja de solo lectura. Leer sigue
funcionando, también para un watch de la misma variable; escribir detiene el
juego con SIGSEGV:

```
safe point -> ¿la variable tiene otro valor que VALOR?
  -> no: no puede pasar a él, así que la página queda abierta
  -> sí: la página pasa a solo lectura
una instrucción escribe en la página -> SIGSEGV
  -> se abre la página y se pone un udf en la instrucción siguiente
  -> la escritura se repite y se hace
  -> se ejecuta el udf -> SIGILL: ¿cambió la variable? -> apunta pc y quién
  -> se restaura la instrucción y se vuelve a cerrar la página
siguiente safe point -> ¿tomó VALOR? -> muestra quién lo escribió
```

`udf` es una instrucción que ARM reserva como no válida: ejecutarla lanza
SIGILL. El trap la usa como breakpoint justo después de la escritura, así que
la página solo está abierta durante una instrucción y se ve cada escritura,
también varias en un mismo turno.

Límites:

- Solo ve lo que escribe el código ARM. Bajo QEMU, lo que escriben las
  funciones nativas pasa por loader (x86_64) y no se ve.
- Si la instrucción siguiente a la escritura está en un bloque IT de Thumb y su
  condición no se cumple, el udf no se ejecuta y la página se queda abierta
  hasta el safe point. Entonces avisa con `not caught` en vez de dar un pc
  equivocado.

## Opciones del programa

- `--trace`: registra cada llamada a una función con hook, con sus argumentos.
- `--callers`: muestra, como un árbol, desde dónde se llama cada método de Java.
- `--debug`, `--debug pause` (o `--debug=pause`): la consola, como arriba.
