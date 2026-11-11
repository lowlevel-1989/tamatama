/* transfer/receive.c: las pantallas de Receive Save (PC y consolas):
   esperar una oferta, preguntar Accept o Decline y esperar la partida. La
   recepción en sí está en receiver.c. */
#include "options.h"
#include "receiver.h"
#include "screens.h"

static int receive_tick(Screen *screen, void *context) {
    Receiver *r = context;
    int event = receiver_poll(r);
    screen->notice = r->notice;
    return event;
}

/* Accept / Decline: de quién, qué y cuándo. 1 con Accept. */
static int ask(Receiver *r) {
    Offer *o = &r->offer;
    char date[32];
    save_date_text((time_t)o->date, date, sizeof date);
    Screen screen;
    screens_ask(&screen, o->name, o->signature, r->choices[receiver_version(r)].name, date, o->summary);
    int answer = screen_run(&screen, receive_tick, r);
    if (answer == SCREEN_EVENT + RECEIVER_CANCELLED) return 0;
    if (answer != ASK_ACCEPT) {
        receiver_decline(r);
        return 0;
    }
    receiver_accept(r);
    return 1;
}

/* Tras Accept, hasta que llega la partida (1) o se vuelve. */
static int wait_data(Receiver *r) {
    Screen screen;
    screens_receiving(&screen);
    int event = screen_run(&screen, receive_tick, r);
    if (event == SCREEN_EVENT + RECEIVER_DONE) return 1;
    receiver_cancel(r);
    return 0;
}

void receive_save(ApkChoice *choices, int count) {
    static Receiver r;
    if (!receiver_open(&r, choices, count)) {
        screen_message("receive save", "cannot use the network", NULL, NULL, NULL);
        return;
    }
    for (;;) {
        Screen screen;
        screens_receive(&screen, r.name, r.signature);
        r.listening = 1;
        int event = screen_run(&screen, receive_tick, &r);
        r.listening = 0;
        if (event < SCREEN_EVENT) break;
        if (event != SCREEN_EVENT + RECEIVER_OFFER || !ask(&r) || !wait_data(&r)) continue;
        int received = !strcmp(r.result, "save received");
        screen_message("receive save", r.result, received ? choices[receiver_version(&r)].name : NULL, receive_tick,
                       &r);
        if (received) break;
        r.notice = NULL;
    }
    receiver_close(&r);
}
