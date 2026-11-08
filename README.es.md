# arena: tamatama

[English](README.md) | Español

**En una fase muy temprana: apenas se está empezando a desarrollar y todavía no es jugable.**

Tamagotchi Classic (Android) migrado poco a poco a código nativo con arena.

**arena** es el proyecto de migración: un loader que ejecuta el `.so` original de un juego
de Android sin Android (en ARM, bajo QEMU en el PC) y deja sustituir sus funciones, una a una,
por código nativo. Lo que ya está migrado corre en nativo y lo demás sigue en el `.so`, hasta
que no quede nada de él. Es todo `src/` salvo `src/port/`, que es tamatama: el código del
juego.

## Mapa de memoria

Lo que ve el código ARM del juego. De `0x60000000` a `0x70000000` está la arena (aquí, la región de
memoria, no el proyecto): 256 MiB que comparten loader y loader-arm, aunque loader
la ve en otra dirección (`to_local` convierte entre las dos).

| Dirección | Tamaño | Qué hay |
|---|---|---|
| `0x60000000` | 8 MiB | stack del código ARM cuando hay loader; su primera palabra guarda cuánta arena está en uso |
| `0x60800000` | lo que ocupe | el `.so` del juego |
| después | | las tablas de JNIEnv y JavaVM |
| después, hasta `0x70000000` | el resto | el heap del juego (`malloc`, `new`) y el stack de 1 MiB de cada thread |
| `0x70000000` | 64 KiB | el código de los hooks: 16 bytes por hook, hasta 4096 |
| `0x70010000` | 128 KiB | los datos de los hooks |

Las direcciones se piden sin `MAP_FIXED`: si alguna está ocupada, se usa la que dé el sistema y
se avisa en el log. Sin loader (todo en la CPU, como en la Pi), el heap y los stacks de los
threads salen de la libc, y la zona del stack queda reservada pero sin usar. Con `NATIVE=1`
no hay `.so`, y la arena es memoria normal de loader que conserva las mismas direcciones.

## Cómo se comunican los procesos

Solo cuando el código ARM corre bajo QEMU (en el PC, o en una Pi de 64 bits sin soporte de 32
bits) hay dos procesos: loader (tus natives, el backend y `app_main`) y loader-arm (el `.so`,
bionic, la JVM falsa, los hooks y los threads). En los demás casos, y con `NATIVE=1`, todo va
en un solo proceso y no hay nada de esto.

Antes de lanzar QEMU, loader abre tres canales y se los deja a loader-arm en file descriptors
fijos (`src/shared/bridge.h`):

| fd | Sentido | Qué |
|---|---|---|
| 3 | loader → loader-arm | pipe de órdenes `TO_ARM_*`: llamar a un método por nombre o por dirección, pedir un símbolo, reservar memoria, el resultado de una native, salir |
| 4 | loader-arm → loader | pipe de respuestas (`TO_HOST_RETURN`) y de llamadas a natives (`TO_HOST_NATIVE_CALL`) |
| 5 | los dos | la arena: un memfd que los dos proyectan; los datos (objetos, vértices, el stack) se leen ahí sin copiarlos |

Por los pipes solo viajan palabras de 32 bits: `[operación, bytes]` y después los datos. El
primer mensaje de loader-arm es `HELLO` (sus bytes forman "OKOK") con la dirección de la arena,
la base del `.so` y el JNIEnv.

Cuando el juego llama a una función que tiene native, su hook en loader-arm envía
`TO_HOST_NATIVE_CALL` con el índice de la native, r0-r3 y el stack pointer. loader la ejecuta,
leyendo en la arena los argumentos que no caben en registers, y responde con
`TO_ARM_NATIVE_DONE` y el resultado. Si esa native vuelve a llamar al juego (`call_method`),
loader envía `TO_ARM_CALL_NAME` mientras loader-arm espera: las llamadas se anidan. Solo hay
una llamada en curso a la vez; por eso los threads del juego van por turnos.

## El juego no se incluye

Copia el `.apk` original en `apk/`, junto al programa. La primera vez se comprueba su md5 y se
extrae en `resources/`; después ya no hace falta. Las versiones compatibles están en
`src/port/apk_table.h`.

## Compilar

```
make
```

Dependencias en Fedora:

```
sudo dnf install make gcc gcc-c++ clang lld qemu-user curl xz binutils tar SDL2-devel mesa-libGL-devel
```

La primera vez descarga una libc ARM de Debian en `sysroot/`. Opciones:

| Opción | Valores | Para qué |
|---|---|---|
| `BACKEND` | `sdl` (por defecto), `dummy` | ventana, entrada y GL |
| `PLATFORM` | `linux` (por defecto) | carpetas y dónde se busca el `.apk` |
| `DEBUG_CONSOLE` | `1` (por defecto), `0` | la consola de debug (`--debug`) |
| `NATIVE` | `0` (por defecto), `1` | `1`: sin `.so` ni QEMU, todo nativo |

## Ejecutar

**`make` (por defecto, `NATIVE=0`)**

| Máquina | Qué ejecutas | Qué pasa |
|---|---|---|
| PC x86_64 | `./loader` | loader arranca `loader-arm` bajo QEMU; las natives corren en loader |
| Pi 64 bits (aarch64) con soporte de 32 bits | `./loader` | `./loader` → execv → `loader-arm`, que lo hace todo en la CPU |
| Pi 64 bits sin soporte de 32 bits | `./loader` | como en el PC: `loader-arm` bajo QEMU |
| Pi 32 bits (armv7) | `./loader-arm` | solo existe `loader-arm`, que lo hace todo en la CPU |

**`make NATIVE=1`**

| Máquina | Qué ejecutas | Qué pasa |
|---|---|---|
| PC x86_64 y Pi 64 bits | `./loader` | todo nativo en loader; no hay execv ni QEMU |
| Pi 32 bits | `./loader-arm` | todo nativo en loader-arm; no carga el `.so` |

En todos los casos el juego se busca igual (versión, recursos y reglas): el `.apk` siempre hace
falta por sus recursos. `./loader --help` muestra todas las opciones.

## Reglas

`--rules=FICHERO` carga un fichero de reglas, relativo al directorio actual. Sin él, si existe
`rules/{md5}.txt` junto al programa, se carga el de esa versión.

## Debug

`./loader --debug` abre la consola de debug; para conectarte:

```
python3 tools/console.py
```

Todo lo demás (buscar y vigilar variables, pausas, `--trace`, `--callers`) está en
[doc/debug.es.md](doc/debug.es.md).
