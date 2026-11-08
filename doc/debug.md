# Debugging

English | [Español](debug.es.md)

## The console

```
./loader --debug          # the game runs, and you can connect at any time
./loader --debug pause    # the game waits for you before its first method (or --debug=pause)
python3 tools/console.py  # in another terminal
```

The console listens on `resources/loader.sock`. Commands are handled each time a method of the
game returns, in the same thread: while you read memory, the game is stopped. If the game gets
stuck inside a method, the console does not answer until it returns.

It is optional: `make DEBUG_CONSOLE=0` builds without it, for systems without Unix sockets.
Then `--debug` is ignored, and watches still work through the log.

## Commands

`help` lists them and `help COMMAND` explains one. `guide` explains step by step how to find a
variable.

| Group | Commands |
|---|---|
| Search | `find TYPE VALUE\|any`, `next VALUE\|changed\|same\|up\|down`, `results [COUNT]` |
| Memory | `read PLACE [TYPE] [COUNT]`, `write PLACE [TYPE] VALUE`, `dump PLACE [BYTES]` |
| Watch | `watch NAME PLACE [TYPE]`, `unwatch NAME`, `watches` |
| Execution | `pause [METHOD]`, `unpause METHOD`, `run [COUNT]`, `resume`, `info` |
| Names (console.py only) | `save NAME PLACE [TYPE]`, `forget NAME`, `names` |

Short forms: `f` find, `n` next, `l` results, `r` read, `w` write, `d` dump, `c` resume,
`p` pause, `i` info, `q` exit.

**PLACE**: `@0x4AD0` a position in the `.so`, `0x6080549c` an absolute address, `g_score` a
symbol, `#1` a search hit, or a name saved with `save`. After it, `+0x18` adds and `->0x10`
reads a pointer and adds to it, as in C: `g_world->0x10->4`.

**TYPE**: `i8 u8 i16 u16 i32 u32 f32 ptr str`; `i32` by default.

Saved names are kept in `.loader_console.json`, also for the next time.

## Rules for debugging

In a rules file (`--rules FILE` or `rules/{md5}.txt`):

```
symbol g_rand  @0xc01c4           # a name for a place the .so does not export
watch  rand    g_rand  u32        # report every change of this variable
pause  init                       # stop every time this method returns (needs --debug)
trace  on                         # log every call to a hooked function
```

Without the console, watch changes go to the log; a `pause` rule only warns.

## Program options

- `--trace`: logs every call to a hooked function, with its arguments.
- `--callers`: shows, as a tree, where each Java method is called from.
- `--debug`, `--debug pause` (or `--debug=pause`): the console, as above.
