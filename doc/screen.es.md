# Pantallas

[English](screen.md) | Español

Todas las pantallas propias, las que no son del juego (el menú de `--menu` y
Transfer Save), se hacen con `src/shared/screen.h`. Una pantalla es un
`Screen` que se rellena y se pasa a `screen_run`, que la dibuja y espera a
que se elija algo.

## Lo que tiene

```
┌──────────────────────┐
│         LOGO         │  logo: rosa, grande
│        título        │  title: verde (opcional)
│       línea 1        │  lines: texto, hasta 7 (opcional)
│       línea 2        │
│  ┌────────────────┐  │
│  │ opción 1       │  │  items que se deslizan: se ven
│  │ opción 2       │  │  "rows" a la vez; si hay más,
│  └────────────────┘  │  flechas y barra
│     opción fija      │  items fijos: las "fixed" últimas
│        aviso         │  notice: coral (opcional)
│   ayuda de teclas    │  y "back, B" si can_back
└──────────────────────┘
```

Cada pantalla usa lo que necesita:

| | Menú | Transfer Save (Receive) |
|---|---|---|
| logo | ARENA | Transfer Save |
| title | choose a game | receive save |
| lines | ninguna | el nombre, la firma y "waiting for a save..." |
| se deslizan | las versiones | ninguna |
| fijas | Transfer Save, Configure controls, Exit | Back |
| can_back | no: para salir está Exit | sí |

La firma es una línea más grande y en rosa: `screen_signature`.

## Hacer una

```c
Screen s;
screen_reset(&s, "send save");        // vacía, con el logo "Transfer Save"
screen_line(&s, BLACK, "this device: %s", me);  // líneas, como printf
screen_signature(&s, "AXJ4");         // línea grande en rosa
screen_item(&s, "Accept");            // opciones
screen_item(&s, "Back");
s.fixed = 1;                          // "Back" queda quieto debajo
int chosen = screen_run(&s, NULL, NULL);
```

`screen_reset` deja los valores de Transfer Save; quien quiera otra cosa los
cambia. El menú, por ejemplo, pone el logo "ARENA" grande
(`logo_scale = 5`), mueve la lista (`list_top = 150`) y quita volver
(`can_back = 0`), porque para salir está Exit.

## Ejemplo: Transfer Save

`src/transfer/screens.c` rellena la pantalla, `screen_run` la enseña y
devuelve la opción elegida, y la lógica decide qué hacer. La pantalla de
inicio (`transfer/transfer.c`):

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
