/* transfer/send.c: las pantallas de Send Save (PC y consolas): se elige la
   versión, luego a quién, y se espera el resultado. El envío en sí está en
   sender.c. */
#include "options.h"
#include "screens.h"
#include "sender.h"

/* La lista de dispositivos, mientras se elige. */
static int list_tick(Screen *screen, void *context) {
    Sender *s = context;
    sender_poll(s);
    screens_devices_begin(screen);
    for (int i = 0; i < s->count; i++) screens_device(screen, s->devices[i].signature, s->devices[i].name);
    screens_devices_end(screen, s->count);
    return 0;
}

/* El envío: 1 cuando termina. */
static int send_tick(Screen *screen, void *context) {
    (void)screen;
    return sender_poll(context);
}

/* Mientras se ve un aviso: seguir oyendo la red. */
static int quiet_tick(Screen *screen, void *context) {
    (void)screen;
    sender_poll(context);
    return 0;
}

/* Manda la partida a devices[device]. Devuelve el resultado. */
static const char *send_to(Sender *s, int device, const ApkChoice *choice, const Save *save) {
    const char *problem = sender_start(s, device, choice->md5, save);
    if (problem) return problem;
    Screen screen;
    screens_waiting_answer(&screen, s->target.name, s->target.signature);
    if (screen_run(&screen, send_tick, s) < SCREEN_EVENT) sender_cancel(s);
    return s->result;
}

void send_save(ApkChoice *choices, int count) {
    Save save;
    int version = choose_version("send save", choices, count, 0, &save);
    if (version < 0) return;
    static Sender s;
    if (!sender_open(&s)) {
        screen_message("send save", "cannot use the network", NULL, NULL, NULL);
        return;
    }
    for (;;) {
        Screen screen;
        screens_send(&screen, choices[version].name, save.summary, s.name, s.signature);
        int chosen = screen_run(&screen, list_tick, &s);
        if (chosen < 0 || chosen >= s.count) break;
        const char *result = send_to(&s, chosen, &choices[version], &save);
        screen_message("send save", result, NULL, quiet_tick, &s);
        if (!strcmp(result, "save sent")) break;
    }
    sender_close(&s);
}
