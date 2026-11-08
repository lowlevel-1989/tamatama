# Arena: tamatama

English | [Español](README.es.md)

**At a very early stage: development has barely started and it is not playable yet.**

Tamagotchi Classic (Android) migrated little by little to native code with Arena.

**Arena** is the migration project: a loader that runs the original `.so` of an Android game
without Android (on ARM, under QEMU on a PC) and lets you replace its functions, one by one,
with native code. What is already migrated runs natively and the rest stays in the `.so`, until
nothing of it is left. It is all of `src/` except `src/port/`, which is tamatama: the game's
code.

## Memory map

What the game's ARM code sees. From `0x60000000` to `0x70000000` is the arena (here, the memory
region, not the project): 256 MiB shared by loader and loader-arm, although
loader sees it at another address (`to_local` converts between the two).

| Address | Size | What is there |
|---|---|---|
| `0x60000000` | 8 MiB | stack of the ARM code when there is a loader; its first word holds how much of the arena is in use |
| `0x60800000` | as big as it is | the game's `.so` |
| after it | | the JNIEnv and JavaVM tables |
| after that, up to `0x70000000` | the rest | the game's heap (`malloc`, `new`) and the 1 MiB stack of each thread |
| `0x70000000` | 64 KiB | the hooks' code: 16 bytes per hook, up to 4096 |
| `0x70010000` | 128 KiB | the hooks' data |

The addresses are requested without `MAP_FIXED`: if one is taken, the one the system gives is
used and the log says so. Without a loader (everything on the CPU, as on the Pi), the heap and
the thread stacks come from libc, and the stack area stays reserved but unused. With
`NATIVE=1` there is no `.so`, and the arena is plain loader memory that keeps the same
addresses.

## How the processes talk

Only when the ARM code runs under QEMU (on a PC, or on a 64-bit Pi without 32-bit support) are
there two processes: loader (your natives, the backend and `app_main`) and loader-arm (the
`.so`, bionic, the fake JVM, the hooks and the threads). In every other case, and with
`NATIVE=1`, everything runs in one process and none of this exists.

Before starting QEMU, loader opens three channels and leaves them to loader-arm on fixed file
descriptors (`src/shared/bridge.h`):

| fd | Direction | What |
|---|---|---|
| 3 | loader → loader-arm | pipe of `TO_ARM_*` commands: call a method by name or by address, look up a symbol, allocate memory, the result of a native, exit |
| 4 | loader-arm → loader | pipe of answers (`TO_HOST_RETURN`) and of native calls (`TO_HOST_NATIVE_CALL`) |
| 5 | both | the arena: a memfd both map; the data (objects, vertices, the stack) is read there without copying |

Only 32-bit words travel through the pipes: `[operation, bytes]` and then the data. The first
message from loader-arm is `HELLO` (its bytes spell "OKOK") with the address of the arena, the
base of the `.so` and the JNIEnv.

When the game calls a function that has a native, its hook in loader-arm sends
`TO_HOST_NATIVE_CALL` with the index of the native, r0-r3 and the stack pointer. loader runs it,
reading from the arena the arguments that do not fit in registers, and answers with
`TO_ARM_NATIVE_DONE` and the result. If that native calls the game again (`call_method`),
loader sends `TO_ARM_CALL_NAME` while loader-arm waits: the calls nest. Only one call is in
progress at a time; that is why the game's threads take turns.

## The game is not included

Copy the original `.apk` into `apk/`, next to the program. The first time, its md5 is checked
and it is extracted into `resources/`; after that it is no longer needed. The supported
versions are listed in `src/port/apk_table.h`.

## Building

```
make
```

Dependencies on Fedora:

```
sudo dnf install make gcc gcc-c++ clang lld qemu-user curl xz binutils tar SDL2-devel mesa-libGL-devel
```

The first time, it downloads an ARM libc from Debian into `sysroot/`. Options:

| Option | Values | What for |
|---|---|---|
| `BACKEND` | `sdl` (default), `dummy` | window, input and GL |
| `PLATFORM` | `linux` (default) | folders, and where the `.apk` is looked for |
| `DEBUG_CONSOLE` | `1` (default), `0` | the debug console (`--debug`) |
| `NATIVE` | `0` (default), `1` | `1`: no `.so` and no QEMU, everything native |

## Running

**`make` (default, `NATIVE=0`)**

| Machine | What you run | What happens |
|---|---|---|
| x86_64 PC | `./loader` | loader starts `loader-arm` under QEMU; the natives run in loader |
| 64-bit Pi (aarch64) with 32-bit support | `./loader` | `./loader` → execv → `loader-arm`, which does everything on the CPU |
| 64-bit Pi without 32-bit support | `./loader` | as on the PC: `loader-arm` under QEMU |
| 32-bit Pi (armv7) | `./loader-arm` | only `loader-arm` exists, and it does everything on the CPU |

**`make NATIVE=1`**

| Machine | What you run | What happens |
|---|---|---|
| x86_64 PC and 64-bit Pi | `./loader` | everything native in loader; no execv and no QEMU |
| 32-bit Pi | `./loader-arm` | everything native in loader-arm; the `.so` is not loaded |

In every case the game is found the same way (version, resources and rules): the `.apk` is
always needed for its resources. `./loader --help` shows every option.

## Rules

`--rules FILE` (or `--rules=FILE`) loads a rules file, relative to the current folder. Without it, if
`rules/{md5}.txt` exists next to the program, the one for that version is loaded.

## Debugging

`./loader --debug` opens the debug console; to connect:

```
python3 tools/console.py
```

Everything else (finding and watching variables, pauses, `--trace`, `--callers`) is in
[doc/debug.md](doc/debug.md).
