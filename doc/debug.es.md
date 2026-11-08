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
pause  init                       # para cada vez que vuelve este método (necesita --debug)
trace  on                         # registra cada llamada a una función con hook
```

Sin la consola, los cambios de los watch van al log; una regla `pause` solo
avisa.

## Opciones del programa

- `--trace`: registra cada llamada a una función con hook, con sus argumentos.
- `--callers`: muestra, como un árbol, desde dónde se llama cada método de Java.
- `--debug`, `--debug pause` (o `--debug=pause`): la consola, como arriba.
