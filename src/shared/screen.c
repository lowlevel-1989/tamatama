/* shared/screen.c: dibujar las pantallas propias y elegir en ellas. */
#include "screen.h"
#include "common.h"
#include "backend.h"
#include <math.h>
#include <stdarg.h>

#define WIDTH 320
#define HEIGHT 480

/* Dónde va la lista: una opción cada ROW píxeles desde list_top. De las
   que se deslizan se ven rows a la vez; si hay más, unas flechas y una
   barra lo dicen. Las fijas van debajo, 20 píxeles más separadas. El texto
   (14 de alto) empieza en la y de su opción, y su recuadro va de 6 píxeles
   más arriba a 26 de alto. */
#define ROW 30
#define LINE 22
/* Arrastrar más que esto (en píxeles) desliza la lista en vez de elegir. */
#define DRAG_SLOP 8

static int sliding(const Screen *s) { return s->count - s->fixed; }
static int visible(const Screen *s) { return sliding(s) < s->rows ? sliding(s) : s->rows; }
static float max_scroll(const Screen *s) { return (float)(sliding(s) - visible(s)); }
static int window_top(const Screen *s) { return s->list_top - 6; }
static int window_bottom(const Screen *s) { return window_top(s) + visible(s) * ROW; }
static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

static int item_y(const Screen *s, int i) {
    int n = sliding(s);
    if (i < n) return s->list_top + (int)((i - s->scroll) * ROW + (i >= s->scroll ? 0.5f : -0.5f));
    return s->list_top + (visible(s) + i - n) * ROW + (n ? 20 : 0);
}

/* La opción bajo el punto, o -1. Una de las que se deslizan, solo dentro
   de la ventana. */
static int item_at(const Screen *s, int x, int y) {
    if (x < 10 || x >= WIDTH - 10) return -1;
    if (y >= window_top(s) && y < window_bottom(s)) {
        int i = (int)((y - window_top(s)) / (float)ROW + s->scroll);
        return i < sliding(s) ? i : -1;
    }
    for (int i = sliding(s); i < s->count; i++)
        if (y >= item_y(s, i) - 6 && y < item_y(s, i) + 20) return i;
    return -1;
}

/* Las flechas, en el hueco de encima y de debajo de la ventana: -1 arriba,
   1 abajo, 0 fuera. */
static int arrow_at(const Screen *s, int y) {
    if (sliding(s) <= s->rows) return 0;
    if (y >= window_top(s) - 22 && y < window_top(s)) return -1;
    if (y >= window_bottom(s) && y < window_bottom(s) + 20) return 1;
    return 0;
}

void screen_reset(Screen *s, const char *title) {
    memset(s, 0, sizeof *s);
    s->logo = "Transfer Save";
    s->logo_scale = 3;
    s->logo_y = 30;
    s->title = title;
    s->title_y = 64;
    s->lines_top = 92;
    s->list_top = 268;
    s->rows = 5;
    s->can_back = 1;
}

void screen_line(Screen *s, unsigned rgb, const char *format, ...) {
    if (s->line_count == SCREEN_LINES) return;
    va_list args;
    va_start(args, format);
    vsnprintf(s->lines[s->line_count], sizeof s->lines[0], format, args);
    va_end(args);
    s->line_scale[s->line_count] = 2;
    s->line_rgb[s->line_count++] = rgb;
}

void screen_signature(Screen *s, const char *signature) {
    screen_line(s, PINK, "%s", signature);
    s->line_scale[s->line_count - 1] = 3;
}

void screen_item(Screen *s, const char *text) {
    if (s->count < SCREEN_ITEMS) snprintf(s->items[s->count++], sizeof s->items[0], "%s", text);
}

void screen_centered(int y, const char *text, int scale, unsigned rgb) {
    if (scale == 2 && backend_text_width(text, 2) > WIDTH - 24) scale = 1, y += 4;
    backend_draw_text((WIDTH - backend_text_width(text, scale)) / 2, y, text, scale, rgb);
}

static void draw_item(const Screen *s, int i) {
    int y = item_y(s, i);
    if (i == s->selected) backend_fill_rect(10, y - 6, WIDTH - 20, 26, BLUE);
    screen_centered(y, s->items[i], 2, BLACK);
}

void screen_draw(const Screen *s) {
    backend_begin_frame();
    backend_fill_rect(0, 0, WIDTH, HEIGHT, CREAM);

    /* Las que se deslizan; lo que se sale de la ventana lo tapa el fondo
       de después. */
    for (int i = 0; i < sliding(s); i++) {
        int y = item_y(s, i);
        if (y >= window_top(s) - ROW && y <= window_bottom(s) + ROW) draw_item(s, i);
    }
    backend_fill_rect(0, 0, WIDTH, window_top(s), CREAM);
    backend_fill_rect(0, window_bottom(s), WIDTH, HEIGHT - window_bottom(s), CREAM);

    if (s->logo) screen_centered(s->logo_y, s->logo, s->logo_scale, PINK);
    if (s->title) screen_centered(s->title_y, s->title, 2, GREEN);
    for (int i = 0, y = s->lines_top; i < s->line_count; y += s->line_scale[i] == 3 ? LINE + 8 : LINE, i++)
        screen_centered(y, s->lines[i], s->line_scale[i], s->line_rgb[i]);

    /* Si hay más de las que caben: una flecha rosa hacia donde quedan y, a
       la derecha, la barra con la parte que se ve. */
    if (sliding(s) > s->rows) {
        float c = WIDTH / 2.0f;
        if (s->scroll > 0.01f) {
            float y = (float)window_top(s) - 8;
            backend_fill_triangle(c - 9, y, c + 9, y, c, y - 9, PINK);
        }
        if (s->scroll < max_scroll(s) - 0.01f) {
            float y = (float)window_bottom(s) + 4;
            backend_fill_triangle(c - 9, y, c + 9, y, c, y + 9, PINK);
        }
        int track = window_bottom(s) - window_top(s);
        int thumb = track * s->rows / sliding(s);
        int at = window_top(s) + (int)((track - thumb) * s->scroll / max_scroll(s) + 0.5f);
        backend_fill_rect(WIDTH - 7, window_top(s), 3, track, BLUE);
        backend_fill_rect(WIDTH - 7, at, 3, thumb, PINK);
    }

    for (int i = sliding(s); i < s->count; i++) draw_item(s, i);

    if (s->notice) screen_centered(HEIGHT - 70, s->notice, 1, CORAL);
    screen_centered(HEIGHT - 40, "arrows, d-pad: move   enter, A, touch: choose", 1, BLACK);
    if (s->can_back) screen_centered(HEIGHT - 26, "back, B: go back", 1, BLACK);
    backend_swap_buffers();
}

int screen_run(Screen *s, ScreenTick tick, void *context) {
    float drag_from = 0;
    int was_down = 0, pressed = -1, press_y = 0, dragging = 0;
    /* lo pulsado antes no cuenta */
    while (backend_input_next() != INPUT_NONE) {}
    for (;;) {
        backend_process_events();
        if (backend_should_close()) {
            backend_cleanup();
            exit(0);
        }
        if (tick) {
            int event = tick(s, context);
            if (event) return SCREEN_EVENT + event;
        }
        /* tick puede haber cambiado las opciones */
        if (s->selected >= s->count) s->selected = s->count ? s->count - 1 : 0;
        s->target = clampf(s->target, 0, max_scroll(s));

        /* El ratón o el touch: soltar sobre la opción pulsada la elige, como
           aceptar; arrastrar en la lista la desliza, y tocar una flecha la
           mueve una fila. */
        int x, y, down = backend_pointer(&x, &y), tapped = 0;
        if (down && !was_down) {
            pressed = item_at(s, x, y);
            press_y = y;
            drag_from = s->scroll;
            dragging = 0;
            int arrow = arrow_at(s, y);
            if (arrow) s->target = clampf(roundf(s->target) + arrow, 0, max_scroll(s));
        }
        if (down && sliding(s) > s->rows && pressed >= 0 && pressed < sliding(s) && abs(y - press_y) > DRAG_SLOP)
            dragging = 1, pressed = -1;
        if (down && dragging)
            s->scroll = s->target = clampf(drag_from - (y - press_y) / (float)ROW, 0, max_scroll(s));
        if (!down && was_down && dragging) s->target = roundf(s->scroll), dragging = 0;
        if (down && pressed >= 0) s->selected = pressed;
        if (!down && was_down && pressed >= 0 && item_at(s, x, y) == pressed) tapped = 1;
        was_down = down;

        for (int input; tapped || (input = backend_input_next()) != INPUT_NONE;) {
            if (tapped) input = INPUT_ACCEPT, tapped = 0;
            if (input == INPUT_BACK && s->can_back) return SCREEN_BACK;
            if (!s->count) continue;
            /* Cualquier botón del mando acepta (A, 1, 2 y 3): los de muchas
               consolas vienen mal asignados hasta configurarlos. */
            if (input == INPUT_1 || input == INPUT_2 || input == INPUT_3 || input == INPUT_ACCEPT3)
                input = INPUT_ACCEPT;
            if (input == INPUT_LEFT || input == INPUT_UP) s->selected = (s->selected + s->count - 1) % s->count;
            if (input == INPUT_RIGHT || input == INPUT_DOWN) s->selected = (s->selected + 1) % s->count;
            /* la lista sigue a la elegida */
            if (s->selected < sliding(s) && s->selected < s->target) s->target = (float)s->selected;
            if (s->selected < sliding(s) && s->selected >= s->target + s->rows)
                s->target = (float)(s->selected - s->rows + 1);
            if (input == INPUT_ACCEPT) return s->selected;
        }
        /* scroll va hacia target poco a poco, para que se vea deslizarse */
        if (!dragging) {
            s->scroll += (s->target - s->scroll) * 0.25f;
            if (fabsf(s->target - s->scroll) < 0.01f) s->scroll = s->target;
        }
        screen_draw(s);
    }
}

void screen_message(const char *title, const char *line1, const char *line2, ScreenTick tick, void *context) {
    Screen s;
    screen_reset(&s, title);
    if (line1) screen_line(&s, BLACK, "%s", line1);
    if (line2) screen_line(&s, BLACK, "%s", line2);
    screen_item(&s, "OK");
    /* un evento de tick no cierra el aviso */
    while (screen_run(&s, tick, context) >= SCREEN_EVENT) {}
}
