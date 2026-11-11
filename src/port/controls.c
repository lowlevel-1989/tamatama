/* port/controls.c: el mando y el teclado dentro del juego. El juego solo
   entiende toques, así que cada acción acaba en un toque (touch_tap) sobre
   uno de sus botones.

   En qué pantalla estamos se sabe por los CSV de botones que lee el juego
   (tc_*btn*.csv, por OnReadFile: controls_csv_read). Cada pantalla crea un
   botón por línea, en orden, a continuación de los que ya había: los
   iconos de arriba (tc_common_icon_btn.csv) son los números 0 a 5 y los de
   la galería (tc_library_top_btn.csv), del 6 al 9. Al acabar cada imagen,
   los CSV leídos en ella se reparten entre los dos managers:
     ButtonManager   la pantalla; si empieza en 0, es una pantalla nueva
     DialogManager   los diálogos (Original Manual, Meal/Snack...): tiene sus
                     propios botones, y mientras se ve, el juego solo
                     atiende esos
   Cómo se navega cada CSV lo dice una tabla escrita a mano, con los nombres
   de sus botones (controls_table.h). Las posiciones se leen del juego, con
   sus funciones (doc/code/ButtonManager.c):
     su centro y tamaño, en la vista de su cámara (GetSpriteViewParam)
     pasados a la pantalla con la inversa de Input::GetTouch_CameraView

   El puntero (un triángulo) aparece con la primera dirección y ya se queda:
   al cambiar de pantalla va a su botón de inicio, y al cerrar un diálogo
   vuelve al que tenía. Sin puntero, aceptar toca el centro. */
#include "backend.h"
#include "controls.h"
#include "touch.h"

#define MAX_BUTTONS 100
#define REFRESH_FRAMES 6 /* cada cuántas imágenes se releen los botones */
#define MAX_IDS 48       /* líneas de un CSV */
#define MAX_LOADED 8     /* CSV a la vez en un manager */
#define MAX_PENDING 8    /* CSV leídos en una imagen */
#define CENTER_SLOT -2   /* el destino "*": el centro, que no es un botón */
#define POINTER_PINK 0xeb008b /* el rosa del logo de Tamagotchi */
#define POINTER_WHITE 0xffffff
#define POINTER_REACH 18.0f /* lo que ocupa desde el botón: separación, alto
                               y vaivén */

/* Un CSV de botones: su nombre y el de cada línea. */
typedef struct {
    char name[48];
    int count, manager; /* BUTTONS o DIALOG */
    char ids[MAX_IDS][40];
} Csv;

/* Un CSV ya creado en un manager: desde qué número empiezan sus botones. */
typedef struct {
    Csv csv;
    int base;
} Loaded;

/* Los managers: el de la pantalla, el de los diálogos y el del aviso de
   Login Bonus de Angel (login_bonus). */
enum { BUTTONS, DIALOG, LOGIN, MANAGERS };
static Loaded loaded[MANAGERS][MAX_LOADED];
static int loaded_count[MANAGERS];
static int layout[MANAGERS]; /* sube cada vez que cambia lo de un manager */
static Csv login_csv;        /* tc_login_btn.csv */
static Csv pending[MAX_PENDING];
static int pending_count;
/* Los CSV de diálogo leídos hasta ahora, uno por nombre: el juego los lee
   antes de abrirlos (el de elegir generación de L.i.f.e. 2, al cargar el
   título o el tutorial), así que el abierto se busca aquí (dialog_csv). */
#define MAX_DIALOGS 16
static Csv dialogs[MAX_DIALOGS];
static int dialog_count;

/* La tabla de controls_table.h. */
typedef struct {
    const char *csv, *rows;
} Screen;
#define SCREEN(csv, rows) {csv, rows},
static const Screen screens[] = {
#include "controls_table.h"
};
#undef SCREEN

/* Un botón de la pantalla actual, en coordenadas de pantalla (las de los
   toques). */
enum { NAV, PREV, NEXT, DEVICE };
typedef struct {
    int slot;          /* su número en el manager, o CENTER_SLOT */
    int role, row;     /* row: fila de la tabla (se ordenan por altura) */
    int device;        /* DEVICE: 0, 1 o 2 */
    float x, y, w, h;  /* centro y tamaño */
} Target;

static Target targets[MAX_BUTTONS];
static int target_count;
static int rows[MAX_BUTTONS][MAX_BUTTONS / 4]; /* índices en targets */
static int row_size[MAX_BUTTONS], row_count;
static int start_slot;   /* el "@" de la tabla, o el primero */
static int can_swipe;    /* "~" */
static int active;       /* BUTTONS o DIALOG */
static int from_left;    /* el lado del puntero en esta pantalla */

/* El puntero, por manager: así, al cerrar un diálogo, vuelve donde estaba. */
#define NO_POINTER -1
static int pointer_on;
static struct {
    int slot, layout;
    float x, y;
} selected[MANAGERS] = {{NO_POINTER, -1, 0, 0}, {NO_POINTER, -1, 0, 0}, {NO_POINTER, -1, 0, 0}};
static int frame;

/* Las funciones del juego que se usan, buscadas una vez. */
static struct {
    int ready;
    arm_ptr manager, camera;        /* las variables con sus instancias */
    arm_ptr dialog, dialog_visible; /* DialogManager: su instancia y si se ve */
    arm_ptr count, visible, enabled, view, sprite, calc_pos;
    arm_ptr camera_param, real_size;
    /* El Login Bonus (solo Angel): la variable con la pantalla actual, las
       vtables de las dos que lo tienen y si se ve. */
    arm_ptr executor, title_vtable, option_vtable, login_running;
    int camera_field; /* dónde guarda el sprite su cámara */
} game;

static void find_executor(void);

static int find_functions(void) {
    if (game.ready) return game.ready > 0;
    game.manager = arm_symbol("_ZN13ButtonManager8instanceE");
    game.camera = arm_symbol("_ZN6Camera8instanceE");
    game.dialog = arm_symbol("_ZN13DialogManager8instanceE");
    game.dialog_visible = arm_symbol("_ZN13DialogManager13IsVisibletDlgEv");
    game.count = arm_symbol("_ZN13ButtonManager12GetButtonNumEv");
    game.visible = arm_symbol("_ZN13ButtonManager9IsVisibleEi");
    game.enabled = arm_symbol("_ZN13ButtonManager9IsEnabledEi");
    game.view = arm_symbol("_ZN13ButtonManager18GetSpriteViewParamEiiPfS0_S0_S0_");
    game.sprite = arm_symbol("_ZN13ButtonManager9GetSpriteEii");
    game.camera_param = arm_symbol("_ZN6Camera18GetCameraParameterEi");
    game.real_size = arm_symbol("_ZN6Camera17GetRealScreenSizeEPfS0_");
    /* Solo Angel tiene Sprite::GetCalcPos, y con él otra forma de Sprite: la
       cámara en +0x40 y la posición calculada con sus padres. Las demás, la
       cámara en +0x30 (ButtonManager::Update de cada versión). */
    game.calc_pos = arm_symbol("_ZN6Sprite10GetCalcPosEPfS0_");
    game.camera_field = game.calc_pos ? 0x40 : 0x30;
    find_executor();
    int ok = game.manager && game.camera && game.dialog && game.dialog_visible && game.count && game.visible &&
             game.enabled && game.view && game.sprite && game.camera_param && game.real_size;
    if (!ok) log_msg("[controls] this game has no ButtonManager: only accept as a tap\n");
    game.ready = ok ? 1 : -1;
    return ok;
}

static uint32_t call(arm_ptr function, uint32_t *words, int n) {
    return (uint32_t)call_arm_at(function, words, n);
}

/* El aviso de Login Bonus de Angel tiene su propio ButtonManager, dentro
   del objeto LoginBouns, que es un miembro de la pantalla del título
   (CTcTitlePart, +0x16ac) y de la de Options (CTcOptionPart, +0x1278). La
   pantalla actual está en una variable sin nombre, la que lee
   mvsp::exec::update:
     ldr r3, [pc, #imm]   el literal: la distancia a la variable
     add r3, pc
     ldr r0, [r3]         la pantalla actual
   Las cinco versiones tienen ese mismo código. */
static void find_executor(void) {
    game.login_running = arm_symbol("_ZN10LoginBouns9IsRunningEv");
    arm_ptr update = arm_symbol("_ZN4mvsp4exec6updateEv") & ~1u;
    if (!game.login_running || !update) return;
    uint16_t *code = to_local(update);
    if ((code[1] & 0xff00) != 0x4b00 || code[2] != 0x447b) return; /* ldr r3; add r3, pc */
    arm_ptr literal = ((update + 2 + 4) & ~3u) + (code[1] & 0xffu) * 4;
    game.executor = *(uint32_t *)to_local(literal) + update + 4 + 4;
    game.title_vtable = arm_symbol("_ZTV12CTcTitlePart") + 8;
    game.option_vtable = arm_symbol("_ZTV13CTcOptionPart") + 8;
}

/* El LoginBouns que se ve ahora, o 0. Su ButtonManager está en +4, y sus
   botones empiezan en el número de +0x508 (LoginBouns::ConvLocalToGloalBtnId). */
static uint32_t login_bonus(void) {
    if (!game.executor) return 0;
    uint32_t part = *(uint32_t *)to_local(game.executor);
    if (!part) return 0;
    uint32_t vtable = *(uint32_t *)to_local(part);
    uint32_t offset = vtable == game.title_vtable ? 0x16ac : vtable == game.option_vtable ? 0x1278 : 0;
    if (!offset) return 0;
    uint32_t login = part + offset;
    return call(game.login_running, &login, 1) & 0xff ? login : 0;
}

/* La instancia de cada manager, o 0. */
static uint32_t manager_object(int which) {
    if (which == LOGIN) {
        uint32_t login = login_bonus();
        return login ? login + 4 : 0;
    }
    return *(uint32_t *)to_local(which == DIALOG ? game.dialog : game.manager);
}

/* ---- Qué CSV hay en cada manager ---- */

/* Los diálogos van a DialogManager; el resto, a ButtonManager. El aviso de
   términos (tc_agreement_dialog_btn.csv) se llama diálogo pero sus botones
   van con los de la pantalla. */
static int csv_manager(const char *name) {
    if (strstr(name, "agreement")) return BUTTONS;
    return strstr(name, "dialog") || strstr(name, "dlg") ? DIALOG : BUTTONS;
}

void controls_csv_read(const char *name, const char *text, int size) {
    if (!strstr(name, "btn") || !strstr(name, ".csv")) return;
    Csv *csv;
    if (!strcmp(name, "tc_login_btn.csv")) {
        csv = &login_csv; /* va aparte: su manager no es ninguno de los dos */
    } else {
        /* El juego lee cada CSV dos veces seguidas. */
        if (pending_count && !strcmp(pending[pending_count - 1].name, name)) return;
        if (pending_count == MAX_PENDING) return;
        csv = &pending[pending_count++];
    }
    snprintf(csv->name, sizeof csv->name, "%s", name);
    csv->count = 0;
    csv->manager = csv_manager(name);
    /* El nombre de cada botón: la primera columna, sin espacios. */
    for (int i = 0; i < size && csv->count < MAX_IDS;) {
        int n = 0;
        while (i < size && text[i] != ',' && text[i] != '\n') {
            char c = text[i++];
            if (c != ' ' && c != '\t' && c != '\r' && n < 39) csv->ids[csv->count][n++] = c;
        }
        csv->ids[csv->count][n] = 0;
        while (i < size && text[i] != '\n') i++;
        i++;
        if (n) csv->count++;
    }
    /* Elegir generación (tc_msel_btn.csv) no se llama diálogo, pero el
       juego lo abre como uno: sus botones son TBTNID_DLG_OLD... */
    for (int i = 0; i < csv->count && csv != &login_csv; i++)
        if (strstr(csv->ids[i], "_DLG_")) csv->manager = DIALOG;
    if (csv->manager != DIALOG || csv == &login_csv) return;
    int k = 0;
    while (k < dialog_count && strcmp(dialogs[k].name, csv->name)) k++;
    if (k == dialog_count && dialog_count < MAX_DIALOGS) dialog_count++;
    if (k < MAX_DIALOGS) dialogs[k] = *csv;
}

/* El CSV del diálogo abierto, con count botones: el leído que tiene esos
   mismos, o si no el más corto que los cubre (de elegir generación solo se
   crean los tres primeros; Yes y No son de su confirmación). NULL si
   ninguno. */
static const Csv *dialog_csv(int count) {
    const Csv *best = NULL;
    for (int i = dialog_count - 1; i >= 0; i--) { /* los últimos, antes */
        if (dialogs[i].count < count) continue;
        if (dialogs[i].count == count) return &dialogs[i];
        if (!best || dialogs[i].count < best->count) best = &dialogs[i];
    }
    return best;
}

static int button_count(int which) {
    uint32_t object = manager_object(which);
    return object ? (int)call(game.count, &object, 1) : 0;
}

/* Al acabar cada imagen, los CSV leídos se reparten entre los managers.
   Sus botones van seguidos, en el orden de lectura. Un diálogo es un solo
   CSV, desde 0. Para la pantalla, dónde empiezan lo dice el cambio de
   GetButtonNum (su valor no: cuenta botones que no salen de ningún CSV, y
   alguno del CSV puede no crearse):
     no cambia                los botones aún no existen: se espera, hasta
                              PENDING_FRAMES (el aviso de Login Bonus de
                              Angel se lee pero no se crea si no hay bonus)
     crece lo que suman       se añaden a los que había
     otro cambio              una pantalla nueva: desde 0. En Shells, los
                              iconos de arriba del 0 al 5, las flechas 6 y
                              7, Back 8 y las miniaturas del 9 al 12 */
#define PENDING_FRAMES 30
static void place_csvs(void) {
    static int last_count[2], waited;
    int now[2] = {button_count(BUTTONS), button_count(DIALOG)};
    int left = 0;
    for (int which = BUTTONS; which <= DIALOG; which++) {
        int rows = 0;
        for (int i = 0; i < pending_count; i++)
            if (pending[i].manager == which) rows += pending[i].count;
        if (!rows) {
            last_count[which] = now[which];
            continue;
        }
        /* Un diálogo es un solo CSV, con sus botones desde 0: DialogManager
           los crea de nuevo, a veces con la misma cuenta. */
        int dialog = which == DIALOG;
        if (!dialog && now[which] == last_count[which] && waited < PENDING_FRAMES) {
            left = 1; /* a la espera */
            continue;
        }
        int grew = now[which] - last_count[which];
        int base = !dialog && grew == rows && loaded_count[which] ? last_count[which] : 0;
        if (dialog || now[which] != last_count[which]) {
            if (!base) loaded_count[which] = 0;
            for (int i = 0; i < pending_count; i++) {
                if (pending[i].manager != which || loaded_count[which] == MAX_LOADED) continue;
                Loaded *l = &loaded[which][loaded_count[which]++];
                l->csv = pending[i];
                l->base = base;
                base += pending[i].count;
                log_msg("[controls] %s: buttons %d to %d\n", l->csv.name, l->base, base - 1);
            }
            layout[which]++;
        }
        last_count[which] = now[which];
        /* Ya repartidos (o descartados): fuera de la lista. */
        int kept = 0;
        for (int i = 0; i < pending_count; i++)
            if (pending[i].manager != which) pending[kept++] = pending[i];
        pending_count = kept;
    }
    waited = left ? waited + 1 : 0;
}

/* ---- Los botones de la pantalla actual ---- */

/* De la vista de una cámara a la pantalla: la inversa de
   Input::GetTouch_CameraView, vista = pos + toque * tamaño * escala / real
   (la escala solo existe en Angel; en las demás vale 1). */
static void to_screen(Target *t, int camera, arm_ptr scratch, uint32_t camera_object) {
    if (camera < 0) return; /* sin cámara: ya está en la pantalla */
    float *f = to_local(scratch);
    uint32_t size_words[3] = {camera_object, scratch + 0x80, scratch + 0x84};
    call(game.real_size, size_words, 3);
    float real_w = f[0x80 / 4], real_h = f[0x84 / 4];
    uint32_t param_words[3] = {scratch, camera_object, (uint32_t)camera};
    call(game.camera_param, param_words, 3); /* devuelve la estructura en
                                                scratch */
    float scale_x = game.calc_pos ? f[9] : 1.0f, scale_y = game.calc_pos ? f[10] : 1.0f;
    float view_w = f[2] * scale_x, view_h = f[3] * scale_y;
    if (view_w == 0 || view_h == 0) return;
    t->x = (t->x - f[0]) * real_w / view_w;
    t->y = (t->y - f[1]) * real_h / view_h;
    t->w = t->w * real_w / view_w;
    t->h = t->h * real_h / view_h;
}

/* El botón slot del manager, si se ve, está habilitado y cae en la
   pantalla (el tutorial deja fuera los de otras páginas). */
static int read_button(uint32_t manager, int slot, Target *t) {
    static arm_ptr scratch; /* para lo que devuelven las funciones del juego */
    if (!scratch) scratch = shared_alloc(0x100);
    float *f = to_local(scratch);
    uint32_t words[7] = {manager, (uint32_t)slot, 0, scratch, scratch + 4, scratch + 8, scratch + 12};
    if (!(call(game.visible, words, 2) & 0xff) || !(call(game.enabled, words, 2) & 0xff)) return 0;
    if (!(call(game.view, words, 7) & 0xff)) return 0;
    t->slot = slot;
    t->x = f[0], t->y = f[1], t->w = f[2], t->h = f[3];
    int camera = -1;
    uint32_t sprite = call(game.sprite, words, 3);
    if (sprite) {
        camera = *(int32_t *)to_local(sprite + (uint32_t)game.camera_field);
        if (game.calc_pos) {
            uint32_t calc_words[3] = {sprite, scratch + 16, scratch + 20};
            call(game.calc_pos, calc_words, 3);
            t->x = f[4], t->y = f[5];
        }
    }
    to_screen(t, camera, scratch, *(uint32_t *)to_local(game.camera));
    return t->w > 0 && t->h > 0 && t->x >= 0 && t->x <= 320 && t->y >= 0 && t->y <= 480;
}

static const char *table_rows(const char *csv) {
    for (size_t i = 0; i < sizeof screens / sizeof screens[0]; i++)
        if (!strcmp(screens[i].csv, csv)) return screens[i].rows;
    return NULL;
}

static int id_index(const Csv *csv, const char *id) {
    for (int i = 0; i < csv->count; i++)
        if (!strcmp(csv->ids[i], id)) return i;
    return -1;
}

static void add_target(Target *t, int role, int row) {
    if (target_count == MAX_BUTTONS) return;
    t->role = role;
    t->row = row;
    targets[target_count++] = *t;
}

/* Los botones de un CSV, según su línea de la tabla. */
static void add_csv(uint32_t manager, const Loaded *l, int *row, int *start) {
    const char *spec = table_rows(l->csv.name);
    char unknown[MAX_IDS * 41];
    if (!spec) {
        /* Sin tabla: una columna, en el orden del fichero. */
        static char warned[64][48];
        static int warned_count;
        int seen = 0;
        for (int i = 0; i < warned_count; i++) seen |= !strcmp(warned[i], l->csv.name);
        if (!seen && warned_count < 64) {
            snprintf(warned[warned_count++], 48, "%s", l->csv.name);
            log_msg("[controls] %s is not in controls_table.h: a column\n", l->csv.name);
        }
        unknown[0] = 0;
        for (int i = 0; i < l->csv.count; i++) {
            strcat(unknown, l->csv.ids[i]);
            strcat(unknown, i + 1 < l->csv.count ? " / " : "");
        }
        spec = unknown;
    }
    (*row)++;
    for (const char *p = spec; *p;) {
        while (*p == ' ') p++;
        if (!*p) break;
        char token[48];
        int n = 0;
        while (*p && *p != ' ' && n < 47) token[n++] = *p++;
        token[n] = 0;
        if (!strcmp(token, "/")) {
            (*row)++;
            continue;
        }
        if (!strcmp(token, "~")) {
            can_swipe = 1;
            continue;
        }
        const char *id = token;
        int role = NAV, device = 0, first = *id == '@';
        if (first) id++;
        if (*id == '<' || *id == '>') role = *id++ == '<' ? PREV : NEXT;
        if (id[0] >= '1' && id[0] <= '3' && id[1] == '=') role = DEVICE, device = id[0] - '1', id += 2;
        Target t = {CENTER_SLOT, NAV, 0, 0, 160.0f, 240.0f, 0.0f, 0.0f};
        if (strcmp(id, "*")) {
            int index = id_index(&l->csv, id);
            if (index < 0 || !read_button(manager, l->base + index, &t)) continue;
        }
        t.device = device;
        add_target(&t, role, *row);
        if (first && role == NAV) *start = t.slot;
    }
}

static int by_y(const void *a, const void *b) {
    float p = targets[*(const int *)a].y, q = targets[*(const int *)b].y;
    return p < q ? -1 : p > q;
}
static int by_x(const void *a, const void *b) {
    float p = targets[*(const int *)a].x, q = targets[*(const int *)b].x;
    return p < q ? -1 : p > q;
}

/* 1 si a todos los botones les cabe el puntero a su izquierda sin salirse
   de la pantalla ni tapar otro botón (Help, el tutorial). Si no (el título,
   Options, la barra de iconos), va desde arriba en toda la pantalla: así no
   cambia de lado al moverse. */
static int pointer_side(void) {
    for (int i = 0; i < target_count; i++) {
        const Target *t = &targets[i];
        if (t->role != NAV || t->slot == CENTER_SLOT) continue;
        float left = t->x - t->w / 2;
        if (left - POINTER_REACH < 0) return 0;
        for (int j = 0; j < target_count; j++) {
            const Target *o = &targets[j];
            if (j == i || o->role != NAV || o->y + o->h / 2 < t->y - 9 || o->y - o->h / 2 > t->y + 9) continue;
            if (o->x + o->w / 2 > left - POINTER_REACH && o->x - o->w / 2 < left) return 0;
        }
    }
    return 1;
}

/* Relee los botones de la pantalla: los del diálogo si se ve uno. */
static void refresh(void) {
    target_count = row_count = can_swipe = 0;
    start_slot = NO_POINTER;
    if (!find_functions()) return;
    uint32_t dialog = manager_object(DIALOG), login = login_bonus();
    int was = active;
    active = dialog && (call(game.dialog_visible, &dialog, 1) & 0xff) ? DIALOG : login ? LOGIN : BUTTONS;
    if (active == LOGIN) {
        /* Sus 9 botones salen de las 2 líneas del CSV: los 8 sellos con la
           primera (TBTNID_BOUNSE) y close, el 8, con la segunda. */
        Csv *csv = &loaded[LOGIN][0].csv;
        *csv = login_csv;
        csv->count = 0;
        for (int i = 0; i < 9 && login_csv.count == 2; i++, csv->count++)
            memcpy(csv->ids[i], login_csv.ids[i < 8 ? 0 : 1], sizeof csv->ids[i]);
        loaded[LOGIN][0].base = *(int32_t *)to_local(login + 0x508);
        loaded_count[LOGIN] = 1;
    }
    /* Un diálogo empieza siempre en su inicio, aunque el juego no relea su
       CSV al volver a abrirlo. */
    if (active != BUTTONS && was != active) selected[active].layout = -1;
    uint32_t manager = manager_object(active);
    if (!manager || !*(uint32_t *)to_local(game.camera)) return;
    /* Un diálogo puede no ser el último CSV de diálogo leído: si el que hay
       no encaja con sus botones, el que encaje. */
    int dialog_buttons = active == DIALOG ? button_count(DIALOG) : 0;
    if (active == DIALOG && !(loaded_count[DIALOG] == 1 && loaded[DIALOG][0].csv.count == dialog_buttons)) {
        const Csv *csv = dialog_csv(dialog_buttons);
        if (csv && (loaded_count[DIALOG] != 1 || strcmp(loaded[DIALOG][0].csv.name, csv->name))) {
            loaded[DIALOG][0].csv = *csv;
            loaded[DIALOG][0].base = 0;
            loaded_count[DIALOG] = 1;
            layout[DIALOG]++;
            selected[DIALOG].layout = -1;
        }
    }

    int row = 0, start = NO_POINTER, last_start = NO_POINTER;
    for (int i = 0; i < loaded_count[active]; i++) {
        int before = target_count;
        add_csv(manager, &loaded[active][i], &row, &start);
        /* El inicio: el "@", o el primero del último CSV con botones (la
           pantalla, no los iconos de arriba que van antes). */
        for (int j = before; j < target_count; j++)
            if (targets[j].role == NAV) {
                last_start = targets[j].slot;
                break;
            }
    }
    /* El aparato está siempre en ButtonManager, aunque haya un diálogo. */
    if (active == DIALOG)
        for (int i = 0; i < loaded_count[BUTTONS]; i++)
            if (!strcmp(loaded[BUTTONS][i].csv.name, "tc_game_retro_btn.csv"))
                add_csv(manager_object(BUTTONS), &loaded[BUTTONS][i], &row, &start);
    start_slot = start != NO_POINTER ? start : last_start;

    /* Las filas: por altura, y cada una de izquierda a derecha. */
    int order[MAX_BUTTONS], n = 0;
    for (int i = 0; i < target_count; i++)
        if (targets[i].role == NAV || targets[i].role == DEVICE) order[n++] = i;
    for (int i = 0; i < n; i++) {
        int r = 0;
        while (r < row_count && targets[rows[r][0]].row != targets[order[i]].row) r++;
        if (r == row_count) row_size[row_count++] = 0;
        if (row_size[r] < MAX_BUTTONS / 4) rows[r][row_size[r]++] = order[i];
    }
    for (int r = 0; r < row_count; r++) qsort(rows[r], (size_t)row_size[r], sizeof(int), by_x);
    /* las filas por la altura de su primer botón */
    int first[MAX_BUTTONS];
    for (int r = 0; r < row_count; r++) first[r] = rows[r][0];
    qsort(first, (size_t)row_count, sizeof(int), by_y);
    int sorted[MAX_BUTTONS][MAX_BUTTONS / 4], sorted_size[MAX_BUTTONS];
    for (int r = 0; r < row_count; r++)
        for (int s = 0; s < row_count; s++)
            if (rows[s][0] == first[r]) {
                memcpy(sorted[r], rows[s], sizeof rows[s]);
                sorted_size[r] = row_size[s];
            }
    memcpy(rows, sorted, sizeof rows);
    memcpy(row_size, sorted_size, sizeof row_size);
    from_left = pointer_side();
}

/* ---- El puntero ---- */

/* Dónde está el puntero: la fila y la posición en ella, o 0. */
static int find_selected(int *row, int *col) {
    for (int r = 0; r < row_count; r++)
        for (int c = 0; c < row_size[r]; c++)
            if (targets[rows[r][c]].slot == selected[active].slot) {
                *row = r, *col = c;
                return 1;
            }
    return 0;
}

static void select_target(int index) {
    selected[active].slot = targets[index].slot;
    selected[active].layout = layout[active];
    selected[active].x = targets[index].x;
    selected[active].y = targets[index].y;
}

/* Pone el puntero en su sitio si su pantalla ha cambiado (el de inicio) o
   su botón ya no está (el más cercano: en la galería, al pasar página).
   Devuelve el índice en targets, o -1 si no hay ninguno. */
static int current_target(void) {
    int r, c;
    if (selected[active].layout == layout[active] && find_selected(&r, &c)) return rows[r][c];
    int best = -1;
    float best_d = 0;
    for (int i = 0; i < target_count; i++) {
        if (targets[i].role != NAV && targets[i].role != DEVICE) continue;
        float dx = targets[i].x - selected[active].x, dy = targets[i].y - selected[active].y;
        float d = dx * dx + dy * dy;
        if (selected[active].layout != layout[active] ? targets[i].slot == start_slot : best < 0 || d < best_d)
            best = i, best_d = d;
    }
    if (best < 0 && row_count) best = rows[0][0];
    if (best >= 0) select_target(best);
    return best;
}

/* Arrastra la pantalla de lado, como un touch: pasa las páginas del manual
   original. Derecha es la página siguiente: el touch va hacia la izquierda.
   El touch acelera y frena (touch.c) y se para antes de soltar, así que el
   manual se mueve lo mismo que él, sin inercia: unos dos tercios de
   pantalla, para que se lea seguido. */
#define SWIPE_WIDTH 200.0f
#define SWIPE_FRAMES 30 /* medio segundo */
static void swipe(int input) {
    float from = input == INPUT_RIGHT ? 160.0f + SWIPE_WIDTH / 2 : 160.0f - SWIPE_WIDTH / 2;
    touch_drag(from, 240.0f, 320.0f - from, 240.0f, SWIPE_FRAMES);
}

/* Izquierda o derecha al final de una fila: pasar página, con las flechas
   de la tabla o arrastrando. */
static void turn_page(int input) {
    for (int i = 0; i < target_count; i++)
        if (targets[i].role == (input == INPUT_LEFT ? PREV : NEXT)) {
            touch_tap(targets[i].x, targets[i].y);
            return;
        }
    if (can_swipe) swipe(input);
}

void controls_input(int input) {
    refresh();
    if (input == INPUT_1 || input == INPUT_2 || input == INPUT_3) {
        for (int i = 0; i < target_count; i++)
            if (targets[i].role == DEVICE && targets[i].device == input - INPUT_1)
                touch_tap(targets[i].x, targets[i].y);
        return;
    }
    /* Aceptar toca lo que hay bajo el puntero (también un botón del
       aparato). Sin puntero, el centro; con INPUT_ACCEPT3 en la pantalla del
       aparato, su botón C. */
    if (input == INPUT_ACCEPT || input == INPUT_ACCEPT3) {
        int i = pointer_on ? current_target() : -1, device_c = -1;
        for (int k = 0; k < target_count; k++)
            if (targets[k].role == DEVICE && targets[k].device == 2) device_c = k;
        if (i >= 0) touch_tap(targets[i].x, targets[i].y);
        else if (input == INPUT_ACCEPT3 && device_c >= 0) touch_tap(targets[device_c].x, targets[device_c].y);
        else touch_tap(160.0f, 240.0f); /* sin puntero: el centro */
        return;
    }
    if (input != INPUT_LEFT && input != INPUT_RIGHT && input != INPUT_UP && input != INPUT_DOWN) return;
    if (!pointer_on) { /* aparece el puntero */
        pointer_on = 1;
        selected[active].layout = -1;
        current_target();
        return;
    }
    if (current_target() < 0) {
        if (input == INPUT_LEFT || input == INPUT_RIGHT) turn_page(input);
        return;
    }
    int r, c;
    find_selected(&r, &c);
    if (input == INPUT_LEFT || input == INPUT_RIGHT) {
        c += input == INPUT_LEFT ? -1 : 1;
        if (c < 0 || c >= row_size[r]) turn_page(input);
        else select_target(rows[r][c]);
        return;
    }
    /* Arriba y abajo: la fila de al lado, en ella el más cercano en x. */
    float x = targets[rows[r][c]].x;
    r += input == INPUT_UP ? -1 : 1;
    if (r < 0 || r >= row_count) return;
    int best = rows[r][0];
    for (int k = 0; k < row_size[r]; k++) {
        float d = targets[rows[r][k]].x - x, b = targets[best].x - x;
        if (d * d < b * b) best = rows[r][k];
    }
    select_target(best);
}

/* El puntero: un triángulo rosa con borde blanco que apunta al botón, del
   mismo lado en toda la pantalla (pointer_side): desde la izquierda, o
   desde arriba, salvo a los botones pegados al borde de arriba, desde
   abajo. Al centro de la pantalla, desde arriba. Se mueve despacio, por
   tiempo y no por imágenes, para que no dependa de los fps. */
#define BOB_PERIOD_MS 550
#define BOB_PIXELS 3.0f
#define POINTER_HALF 9.0f    /* media base */
#define POINTER_HEIGHT 13.0f
#define POINTER_BORDER 2.5f

/* Un triángulo con la punta en (tip_x, tip_y) y la base hacia (dx, dy), que
   es una dirección de un solo eje: (1, 0), (-1, 0), (0, 1) o (0, -1). */
static void fill_pointer(float tip_x, float tip_y, float dx, float dy, float half, float height, uint32_t rgb) {
    float base_x = tip_x + dx * height, base_y = tip_y + dy * height;
    backend_fill_triangle(tip_x, tip_y, base_x - dy * half, base_y - dx * half, base_x + dy * half,
                          base_y + dx * half, rgb);
}

static void draw_pointer(const Target *t) {
    float phase = (float)(clock_ns() / 1000000u % BOB_PERIOD_MS) / BOB_PERIOD_MS;
    float bob = BOB_PIXELS * (phase < 0.5f ? phase * 2 : 2 - phase * 2); /* 0..3..0 */
    float tip_x, tip_y, dx = 0, dy = 0;
    float left = t->x - t->w / 2;
    if (t->slot != CENTER_SLOT && from_left) {
        dx = -1; /* desde la izquierda */
        tip_x = left - 2 - bob;
        tip_y = t->y;
    } else {
        dy = t->y - t->h / 2 < POINTER_REACH ? 1 : -1; /* desde abajo o arriba */
        tip_x = t->x;
        tip_y = dy > 0 ? t->y + t->h / 2 + 2 + bob : t->y - t->h / 2 - 2 - bob;
    }
    float border = POINTER_BORDER;
    backend_overlay_begin();
    fill_pointer(tip_x - dx * border * 1.6f, tip_y - dy * border * 1.6f, dx, dy, POINTER_HALF + border,
                 POINTER_HEIGHT + border * 2.6f, POINTER_WHITE);
    fill_pointer(tip_x, tip_y, dx, dy, POINTER_HALF, POINTER_HEIGHT, POINTER_PINK);
    backend_overlay_end();
}

void controls_frame(void) {
    frame++;
    /* Los CSV leídos se reparten en cada imagen, aunque no haya puntero:
       hace falta ver cómo cambian las cuentas de una a otra. */
    if (find_functions()) place_csvs();
    if (!pointer_on) return;
    /* Se releen cada REFRESH_FRAMES imágenes, o ya si la pantalla cambia. */
    static int seen_layout = -1;
    if (frame % REFRESH_FRAMES == 0 || seen_layout != layout[BUTTONS] + layout[DIALOG]) {
        refresh();
        seen_layout = layout[BUTTONS] + layout[DIALOG];
    }
    int i = current_target();
    if (i < 0) return;
    selected[active].x = targets[i].x;
    selected[active].y = targets[i].y;
    draw_pointer(&targets[i]);
}
