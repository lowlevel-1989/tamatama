/* transfer/screens.c: las pantallas de Transfer Save, una función cada
   una. El orden de los screen_item es el de los enum de screens.h. */
#include "screens.h"
#include "common.h"

void screens_home(Screen *s) {
    screen_reset(s, NULL);
    screen_line(s, BLACK, "send your save to");
    screen_line(s, BLACK, "another device on");
    screen_line(s, BLACK, "the same network");
    screen_item(s, "Send Save");
    screen_item(s, "Receive Save");
    screen_item(s, "Restore Backup");
    screen_item(s, "Back");
    s->fixed = 1;
}

void screens_versions(Screen *s, const char *title, const char *const *names, int count) {
    screen_reset(s, title);
    screen_line(s, BLACK, "choose the version");
    for (int i = 0; i < count; i++) screen_item(s, names[i]);
    screen_item(s, "Back");
    s->fixed = 1;
    s->list_top = 130;
}

void screens_restore(Screen *s, const char *version, const char *date, const char *summary) {
    screen_reset(s, "restore backup");
    screen_line(s, BLACK, "%s", version);
    screen_line(s, BLACK, "back to the save before");
    screen_line(s, BLACK, "the last one received:");
    screen_line(s, PINK, "%s%s%s", date, summary[0] ? "  " : "", summary);
    screen_line(s, CORAL, "the current one will be the backup");
    screen_item(s, "Restore");
    screen_item(s, "Back");
}

void screens_send(Screen *s, const char *version, const char *summary, const char *me, const char *signature) {
    screen_reset(s, "send save");
    screen_line(s, BLACK, "%s", version);
    if (summary[0]) screen_line(s, BLACK, "%s", summary);
    screen_line(s, BLACK, "this device: %s", me);
    screen_signature(s, signature);
    screen_line(s, BLACK, "send it to:");
    /* que quepa la lista con las líneas de encima */
    s->list_top = 230;
    s->rows = 3;
    screens_devices_begin(s);
    screens_devices_end(s, 0);
}

void screens_devices_begin(Screen *s) { s->count = 0; }

void screens_device(Screen *s, const char *signature, const char *name) {
    char item[80];
    snprintf(item, sizeof item, "%s  %s", signature, name);
    screen_item(s, item);
}

void screens_devices_end(Screen *s, int found) {
    screen_item(s, "Back");
    s->fixed = 1;
    s->notice = found ? NULL : "looking for devices in Receive Save...";
}

void screens_waiting_answer(Screen *s, const char *name, const char *signature) {
    screen_reset(s, "send save");
    screen_line(s, BLACK, "waiting for %s", name);
    screen_signature(s, signature);
    screen_line(s, BLACK, "to accept it");
    screen_item(s, "Cancel");
}

void screens_receive(Screen *s, const char *me, const char *signature) {
    screen_reset(s, "receive save");
    screen_line(s, BLACK, "this device: %s", me);
    screen_signature(s, signature);
    screen_line(s, BLACK, "waiting for a save...");
    screen_line(s, BLACK, "choose this device");
    screen_line(s, BLACK, "in Send Save");
    screen_item(s, "Back");
}

void screens_ask(Screen *s, const char *name, const char *signature, const char *version, const char *date,
                 const char *summary) {
    screen_reset(s, "receive save");
    screen_line(s, BLACK, "a save from %s", name);
    screen_signature(s, signature);
    screen_line(s, BLACK, "%s", version);
    screen_line(s, BLACK, "%s%s%s", date, summary[0] ? "  " : "", summary);
    screen_line(s, CORAL, "your save will be kept as backup");
    screen_item(s, "Accept");
    screen_item(s, "Decline");
}

void screens_receiving(Screen *s) {
    screen_reset(s, "receive save");
    screen_line(s, BLACK, "receiving...");
}
