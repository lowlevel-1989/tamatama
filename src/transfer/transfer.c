/* transfer/transfer.c: la pantalla de Transfer Save, con sus opciones:
     Send Save       se elige la versión, luego a quién, y se envía
                     (send.c)
     Receive Save    mientras está abierta, espera una partida (receive.c)
     Restore Backup  vuelve a la partida que había antes de la última
                     recibida (aquí) */
#include "transfer.h"
#include "backend.h"
#include "options.h"
#include "screens.h"

#define WIDTH 320
#define HEIGHT 480
#define MAX_VERSIONS 16

int choose_version(const char *title, ApkChoice *choices, int count, int backup, Save *save) {
    /* las versiones que tienen partida (o backup) */
    const char *names[MAX_VERSIONS];
    int index[MAX_VERSIONS], found = 0;
    for (int i = 0; i < count && found < MAX_VERSIONS; i++) {
        Save any;
        if (!save_find(choices[i].md5, backup, &any)) continue;
        index[found] = i;
        names[found++] = choices[i].name;
    }
    if (!found) {
        screen_message(title, backup ? "there is no backup yet" : "there are no saves to send", NULL, NULL, NULL);
        return -1;
    }
    Screen s;
    screens_versions(&s, title, names, found);
    for (;;) {
        int chosen = screen_run(&s, NULL, NULL);
        if (chosen < 0 || chosen >= found) return -1;
        if (save_find(choices[index[chosen]].md5, backup, save)) return index[chosen];
    }
}

static void restore(ApkChoice *choices, int count) {
    Save backup;
    int version = choose_version("restore backup", choices, count, 1, &backup);
    if (version < 0) return;
    char date[32];
    save_date_text(backup.date, date, sizeof date);
    Screen s;
    screens_restore(&s, choices[version].name, date, backup.summary);
    if (screen_run(&s, NULL, NULL) != RESTORE_YES) return;
    int ok = save_restore_backup(choices[version].md5);
    log_msg("[transfer] restore backup of %s: %s\n", choices[version].md5, ok ? "done" : "failed");
    screen_message("restore backup", ok ? "backup restored" : "cannot restore the backup", NULL, NULL, NULL);
}

void transfer_screen(void) {
    static ApkChoice choices[MAX_VERSIONS];
    int count = apk_choices(choices, MAX_VERSIONS);
    backend_set_background(CREAM);
    Screen s;
    screens_home(&s);
    for (;;) {
        int chosen = screen_run(&s, NULL, NULL);
        if (chosen == HOME_SEND) send_save(choices, count);
        else if (chosen == HOME_RECEIVE) receive_save(choices, count);
        else if (chosen == HOME_RESTORE) restore(choices, count);
        else break;
    }
}

int transfer_main(void) {
    backend_init(WIDTH, HEIGHT);
    transfer_screen();
    backend_cleanup();
    return 0;
}
