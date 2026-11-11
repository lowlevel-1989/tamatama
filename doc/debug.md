# Debugging

English | [Español](debug.es.md)

## The console

```
./loader --debug          # the game runs, and you can connect at any time
./loader --debug pause    # the game waits for you before its first method (or --debug=pause)
python3 tools/console.py  # in another terminal
```

The console listens on `resources/loader.sock`. Commands are handled at safe
points, when the game is stopped: while you read or write its memory, nothing
runs. There are two:

- when a method of the game called by `app_main` returns (`init`, `usleep`...);
- at the end of every turn of a game thread, even while `app_main` is in a long
  `usleep`. Its name is `thread`: `pause thread` stops at every turn, and
  `run 1` moves one forward.

If the game gets stuck without reaching either, the console does not answer
until it does.

### The safe point of each turn

This is how the console is reached at the end of every turn of a game thread:

```
game thread: run() -> Thread::sleep -> usleep
  -> context_switch to the scheduler
scheduler: run_thread -> safe_point()
  PC (two processes):
    loader-arm -> TO_HOST_SAFE_POINT through fd 4 -> waits
    loader (in wait_for_arm_result) -> debug_after_call("thread")
      -> watches, pauses and console commands
      -> if paused, it waits here until resume or run
    loader -> TO_ARM_NATIVE_DONE through fd 3
    loader-arm goes on: the next thread, or back to the main one
  Pi or NATIVE=1 (one process):
    debug_after_call("thread") directly
```

While loader serves the console, loader-arm waits in `serve_host_requests` and
can answer it, for example to look up a symbol. With the tamatama loop there are
about 32 safe points per second: two `usleep` for every ~62 ms lap.

If the game creates no threads, there are no turns and only the first safe point
is left.

It is optional: `make DEBUG_CONSOLE=0` builds without it, for systems without
Unix sockets. Then `--debug` is ignored, and watches still work through the log.

## Commands

`help` lists them and `help COMMAND` explains one. `guide` explains step by step
how to find a variable.

| Group | Commands |
|---|---|
| Search | `find TYPE VALUE\|any`, `next VALUE\|changed\|same\|up\|down`, `results [COUNT]` |
| Memory | `read PLACE [TYPE] [COUNT]`, `write PLACE [TYPE] VALUE`, `dump PLACE [BYTES]` |
| Watch | `watch NAME PLACE [TYPE]`, `unwatch NAME`, `watches` |
| Execution | `pause [METHOD]`, `unpause METHOD`, `run [COUNT]`, `resume`, `info` |
| Names (console.py only) | `save NAME PLACE [TYPE]`, `forget NAME`, `names` |

Short forms: `f` find, `n` next, `l` results, `r` read, `w` write, `d` dump, `c`
resume, `p` pause, `i` info, `q` exit.

**PLACE**: `@0x4AD0` a position in the `.so`, `0x6080549c` an absolute address,
`g_score` a symbol, `#1` a search hit, or a name saved with `save`. After it,
`+0x18` adds and `->0x10` reads a pointer and adds to it, as in C:
`g_world->0x10->4`.

**TYPE**: `i8 u8 i16 u16 i32 u32 f32 ptr str`; `i32` by default.

Saved names are kept in `.loader_console.json`, also for the next time.

## Rules for debugging

In a rules file (`--rules FILE` or `rules/{md5}.txt`):

```
symbol g_rand  @0xc01c4           # a name for a place the .so does not export
watch  rand    g_rand  u32        # report every change of this variable
trap   rand    g_rand  u32 0      # which instruction sets it to 0
pause  init                       # stop every time this method returns (needs --debug)
trace  on                         # log every call to a hooked function
```

Without the console, watch changes go to the log; a `pause` rule only warns.

## Who writes a variable: trap

A watch tells you when a variable changes; a trap also tells you which
instruction of the .so wrote it. It only exists as a rule (not in the console)
and reports to the log:

```
trap NAME PLACE [TYPE] [VALUE]
```

PLACE and TYPE are those of `watch`. With VALUE, written as a watch shows it,
it only reports when the variable takes that value; without it, every change.
For example:

```
trap m_endDraw  m_endDraw  u32 0
```

```
[trap] m_endDraw at 0x608c1f88 = 0, waiting for 0
[trap] m_endDraw 2 -> 0, written at 0x60838882 (@0x38882 Class::method+0x3e) (lr 0x6083a54c (@0x3a54c Class::other+0xd8)) by step
```

`written at` is the instruction that wrote the value: look at it in radare,
with `e asm.bits=16` if the code is Thumb. `lr` is its lr register at that
moment: if the write is in a leaf function (a setter), it is the caller.
`by` says who was running: the
call of app_main (`init`, `step`...) or `thread N`, the number shown as
`[thread] N:` when the thread was created.

How it works (src/arm/trap.c): having the address only lets the loader read the
variable, so its page (4096 bytes) is made read only. Reading still works, also
for a watch on the same variable; writing stops the game with SIGSEGV:

```
safe point -> does the variable have another value than VALUE?
  -> no: it cannot change to it, so the page stays open
  -> yes: the page becomes read only
an instruction writes to the page -> SIGSEGV
  -> the page is opened and an udf goes on the next instruction
  -> the write is repeated and done
  -> the udf runs -> SIGILL: did the variable change? -> note pc and who
  -> the instruction is restored and the page closed again
next safe point -> did it take VALUE? -> report who wrote it
```

`udf` is an instruction ARM reserves as invalid: running it raises SIGILL. The
trap uses it as a breakpoint right after the write, so the page is open for a
single instruction and every write is seen, even several in one turn.

Limits:

- It only sees what ARM code writes. Under QEMU, what the native functions
  write goes through loader (x86_64) and is not seen.
- If the instruction after the write is in a Thumb IT block whose condition
  fails, the udf does not run and the page stays open until the safe point.
  Then it reports `not caught` instead of a wrong pc.

## Program options

- `--trace`: logs every call to a hooked function, with its arguments.
- `--callers`: shows, as a tree, where each Java method is called from.
- `--debug`, `--debug pause` (or `--debug=pause`): the console, as above.
