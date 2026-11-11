# Screens

English | [Español](screen.es.md)

All our own screens, the ones that are not the game's (the `--menu` menu
and Transfer Save), are made with `src/shared/screen.h`. A screen is a
`Screen` that you fill in and pass to `screen_run`, which draws it and waits
until something is chosen.

## What it has

```
┌──────────────────────┐
│         LOGO         │  logo: pink, big
│        title         │  title: green (optional)
│        line 1        │  lines: text, up to 7 (optional)
│        line 2        │
│  ┌────────────────┐  │
│  │ option 1       │  │  sliding items: "rows" of them
│  │ option 2       │  │  are seen at once; if there are
│  └────────────────┘  │  more, arrows and a bar
│     fixed option     │  fixed items: the last "fixed"
│        notice        │  notice: coral (optional)
│     keys help        │  and "back, B" if can_back
└──────────────────────┘
```

Each screen uses what it needs:

| | Menu | Transfer Save (Receive) |
|---|---|---|
| logo | ARENA | Transfer Save |
| title | choose a game | receive save |
| lines | none | the name, the signature and "waiting for a save..." |
| sliding | the versions | none |
| fixed | Transfer Save, Configure controls, Exit | Back |
| can_back | no: Exit is there to leave | yes |

The signature is a bigger pink line: `screen_signature`.

## Making one

```c
Screen s;
screen_reset(&s, "send save");        // empty, with the "Transfer Save" logo
screen_line(&s, BLACK, "this device: %s", me);  // lines, like printf
screen_signature(&s, "AXJ4");         // a big pink line
screen_item(&s, "Accept");            // options
screen_item(&s, "Back");
s.fixed = 1;                          // "Back" stays still below
int chosen = screen_run(&s, NULL, NULL);
```

`screen_reset` sets the Transfer Save values; change them for something
else. The menu, for example, sets a big "ARENA" logo (`logo_scale = 5`),
moves the list (`list_top = 150`) and turns off going back
(`can_back = 0`), because Exit is there to leave.

## Example: Transfer Save

`src/transfer/screens.c` fills in the screen, `screen_run` shows it and
returns the chosen option, and the logic decides what to do. The first
screen (`transfer/transfer.c`):

```c
Screen s;
screens_home(&s);                      // Send Save, Receive Save...
for (;;) {
    int chosen = screen_run(&s, NULL, NULL);
    if (chosen == HOME_SEND) send_save(choices, count);
    else if (chosen == HOME_RECEIVE) receive_save(choices, count);
    else if (chosen == HOME_RESTORE) restore(choices, count);
    else break;                        // Back
}
```
