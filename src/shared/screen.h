/* shared/screen.h: las pantallas propias, las que no son del juego: el menú
   de --menu (menu.c) y Transfer Save (src/transfer). Todas son iguales:
     un logo y un título arriba
     unas líneas de texto
     una lista de opciones: las primeras van en una ventana que se desliza
     si no caben; las fixed últimas, debajo, quietas
     un aviso y la ayuda abajo
   Se eligen con las direcciones y aceptar (teclado o mando), o con el
   ratón o el touch: pulsar marca la opción y soltar encima la elige, y
   arrastrar desliza la lista. Cómo se usa, con un ejemplo:
   doc/screen.es.md. */
#pragma once
#include "colors.h"

#define SCREEN_LINES 7
#define SCREEN_ITEMS 24

typedef struct {
    /* lo que se ve: lo pone quien usa la pantalla */
    const char *logo;   /* en rosa, grande */
    int logo_scale, logo_y;
    const char *title;  /* en verde, debajo del logo; o NULL */
    int title_y;
    char lines[SCREEN_LINES][96];
    unsigned line_rgb[SCREEN_LINES];
    int line_scale[SCREEN_LINES];
    int line_count, lines_top;
    char items[SCREEN_ITEMS][80];
    int count;
    int fixed;          /* las últimas opciones, quietas debajo */
    int list_top;       /* la y de la primera opción */
    int rows;           /* cuántas de las que se deslizan se ven */
    int can_back;       /* volver (Esc, B) sale de la pantalla */
    const char *notice; /* un aviso abajo, en coral; o NULL */
    /* el estado: se conserva entre llamadas a screen_run */
    int selected;
    float scroll, target; /* la primera fila que se ve, y hacia dónde va */
} Screen;

/* Lo que se hace mientras se ve una pantalla, una vez por imagen (la red).
   Puede cambiar la pantalla. Devuelve 0 para seguir. */
typedef int (*ScreenTick)(Screen *s, void *context);

/* Lo que devuelve screen_run además del número de la opción elegida. */
#define SCREEN_BACK -1   /* se pulsó volver */
#define SCREEN_EVENT 100 /* SCREEN_EVENT + lo que devolvió tick */

/* Una pantalla vacía de Transfer Save: logo "Transfer Save" y title. Las
   demás cambian logo y las y. */
void screen_reset(Screen *s, const char *title);
void screen_line(Screen *s, unsigned rgb, const char *format, ...) __attribute__((format(printf, 3, 4)));
/* Una firma: grande y en rosa, que es lo que se compara. */
void screen_signature(Screen *s, const char *signature);
void screen_item(Screen *s, const char *text);
/* Texto centrado; a escala 2, si no cabe, baja a 1. */
void screen_centered(int y, const char *text, int scale, unsigned rgb);
void screen_draw(const Screen *s);
/* Enseña la pantalla hasta que se elige una opción, se vuelve o tick (si no
   es NULL) devuelve algo. */
int screen_run(Screen *s, ScreenTick tick, void *context);
/* Un aviso con OK. tick sigue funcionando mientras se ve. */
void screen_message(const char *title, const char *line1, const char *line2, ScreenTick tick, void *context);
