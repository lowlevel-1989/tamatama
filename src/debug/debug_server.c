/* debug/debug_server.c: una consola de debug por socket. Con la opción --debug,
   el programa abre loader.sock en data_dir() (en Linux, resources/ junto al
   programa) y atiende comandos de texto, como la búsqueda de memoria de un
   emulador:
     nc -U resources/loader.sock

   Los comandos se atienden al volver de cada call_method, en el mismo thread.
   Así no hace falta ningún lock, el código ARM está parado mientras se lee su
   memoria y se puede preguntar a loader-arm. A cambio, si el juego se queda
   bloqueado dentro de un método, la consola no responde hasta que vuelva.

   La memoria se lee directamente de la arena: desde el .so hasta lo último que
   se ha reservado.
   Quedan fuera el stack y, sin loader, lo que reserve la libc. */
#include <errno.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include "memory.h"
#include "platform.h"
#include <sys/stat.h>

#define MAX_LINE 512
#define MAX_HITS 8000000u
#define MAX_TEXT 64

static const char *socket_path;
static int listen_fd = -1, client_fd = -1;
static char line[MAX_LINE];
static size_t line_bytes;
static int paused;
static uint32_t run_left; /* cuántas vueltas faltan para pausar con run */
static uint32_t polls;    /* cuántas veces ha vuelto call_method */
static const char *last_method; /* el último método que ha vuelto */

/* Los métodos en los que hay que pausar al volver: pause NOMBRE. */
#define MAX_PAUSES 32
static const char *pause_methods[MAX_PAUSES];
static int pause_count;

/* Una coincidencia de la búsqueda: dónde está y qué valor tenía la última
   vez. */
typedef struct {
    arm_ptr address;
    uint32_t raw;
} Hit;
static Hit *hits;
static size_t hit_count;
static ValueType hit_type;

/* Envía texto solo a la consola, si hay alguien conectado. */
static void send_text(const char *text, size_t bytes) {
    if (client_fd >= 0) send(client_fd, text, bytes, MSG_NOSIGNAL);
}
void debug_send(const char *format, ...) {
    char text[512];
    va_list args;
    va_start(args, format);
    int n = vsnprintf(text, sizeof text, format, args);
    va_end(args);
    if (n > 0) send_text(text, (size_t)n < sizeof text ? (size_t)n : sizeof text - 1);
}
/* La respuesta de error de un comando. Sustituye a su línea ok. */
static int failed;
static void fail(const char *format, ...) {
    char text[512];
    va_list args;
    va_start(args, format);
    int n = vsnprintf(text, sizeof text, format, args);
    va_end(args);
    failed = 1;
    if (n > 0) send_text(text, (size_t)n < sizeof text ? (size_t)n : sizeof text - 1);
}
/* Mensaje para la terminal del programa y para la consola. */
void debug_report(const char *format, ...) {
    char text[512];
    va_list args;
    va_start(args, format);
    int n = vsnprintf(text, sizeof text, format, args);
    va_end(args);
    if (n <= 0) return;
    size_t bytes = (size_t)n < sizeof text ? (size_t)n : sizeof text - 1;
    log_msg("%s", text);
    send_text(text, bytes);
}
/* ---------- Búsqueda por valor ---------- */

static uint32_t read_raw(arm_ptr address, int bytes) {
    uint32_t raw = 0;
    memcpy(&raw, to_local(address), (size_t)bytes);
    return raw;
}
/* Interpreta unos bits según el tipo. double representa exactamente cualquier
   valor de 32 bits. */
static double raw_number(ValueType type, uint32_t raw) {
    switch (type) {
    case TYPE_I8: return (int8_t)raw;
    case TYPE_I16: return (int16_t)raw;
    case TYPE_I32: return (int32_t)raw;
    case TYPE_F32: return bits_to_float(raw);
    default: return raw;
    }
}
/* Un float escrito a mano casi nunca coincide bit a bit, así que se admite una
   pequeña diferencia. */
static int same_number(ValueType type, double a, double b) {
    if (type != TYPE_F32) return a == b;
    double difference = a - b;
    return difference < 0.001 && difference > -0.001;
}
static int searchable(ValueType type) { return type != TYPE_PTR && type != TYPE_STR; }

static void show_hits(size_t limit) {
    arm_ptr so_base = arm_so_base();
    for (size_t i = 0; i < hit_count && i < limit; i++) {
        char text[MAX_TEXT];
        value_format(hit_type, hits[i].address, text, sizeof text);
        if (so_base && hits[i].address >= so_base)
            debug_send("0x%08x  @0x%x  %s\n", hits[i].address, hits[i].address - so_base, text);
        else debug_send("0x%08x  %s\n", hits[i].address, text);
    }
    if (hit_count > limit) debug_send("... %zu more\n", hit_count - limit);
}
/* find TIPO VALOR: empieza una búsqueda nueva. Con any guarda todo, para un
   valor que no conoces. */
static void command_find(const char *type_name, const char *value) {
    ValueType type;
    if (!value_type_parse(type_name, &type) || !searchable(type) || !*value) {
        fail("error: find i8|u8|i16|u16|i32|u32|f32 VALUE|any\n");
        return;
    }
    int any = !strcmp(value, "any");
    double wanted = any ? 0 : strtod(value, NULL);
    arm_ptr start;
    uint32_t range;
    arm_memory_range(&start, &range);
    int bytes = value_bytes(type);
    size_t capacity = 1024;
    free(hits);
    hits = malloc(capacity * sizeof *hits);
    hit_count = 0;
    hit_type = type;
    arm_ptr address = (start + (uint32_t)bytes - 1) & ~((uint32_t)bytes - 1);
    for (; address + (uint32_t)bytes <= start + range; address += (uint32_t)bytes) {
        uint32_t raw = read_raw(address, bytes);
        if (!any && !same_number(type, raw_number(type, raw), wanted)) continue;
        if (hit_count == MAX_HITS) {
            fail("error: too many hits, stopped at %u\n", MAX_HITS);
            break;
        }
        if (hit_count == capacity) {
            capacity *= 2;
            hits = realloc(hits, capacity * sizeof *hits);
        }
        hits[hit_count].address = address;
        hits[hit_count].raw = raw;
        hit_count++;
    }
    debug_send("%zu hits in %u bytes\n", hit_count, range);
    if (hit_count <= 20) show_hits(20);
}
/* next VALOR | changed | same | up | down: se queda con las coincidencias que
   cumplan esto. */
static void command_next(const char *condition) {
    if (!hits) {
        fail("error: use find first\n");
        return;
    }
    if (!*condition) {
        fail("error: next VALUE|changed|same|up|down\n");
        return;
    }
    int bytes = value_bytes(hit_type);
    int changed = !strcmp(condition, "changed"), same = !strcmp(condition, "same");
    int up = !strcmp(condition, "up"), down = !strcmp(condition, "down");
    int by_value = !changed && !same && !up && !down;
    double wanted = by_value ? strtod(condition, NULL) : 0;
    size_t kept = 0;
    for (size_t i = 0; i < hit_count; i++) {
        uint32_t raw = read_raw(hits[i].address, bytes);
        double now = raw_number(hit_type, raw), before = raw_number(hit_type, hits[i].raw);
        int keep = by_value ? same_number(hit_type, now, wanted)
                   : changed ? raw != hits[i].raw
                   : same    ? raw == hits[i].raw
                   : up      ? now > before
                             : now < before;
        if (!keep) continue;
        hits[kept].address = hits[i].address;
        hits[kept].raw = raw;
        kept++;
    }
    hit_count = kept;
    debug_send("%zu hits\n", hit_count);
    if (hit_count <= 20) show_hits(20);
}
/* ---------- Leer y escribir ---------- */

static void command_read(const char *place, const char *type_name, const char *count_text) {
    ValueType type = TYPE_I32;
    if (*type_name && !value_type_parse(type_name, &type)) {
        fail("error: unknown type %s\n", type_name);
        return;
    }
    arm_ptr address = place_resolve(place);
    uint32_t count = *count_text ? parse_number(count_text) : 1;
    for (uint32_t i = 0; i < count; i++, address += (uint32_t)value_bytes(type)) {
        if (!arm_readable(address)) {
            debug_send("0x%08x  <invalid>\n", address);
            return;
        }
        char text[MAX_TEXT];
        value_format(type, address, text, sizeof text);
        debug_send("0x%08x  %s\n", address, text);
    }
}
static void command_write(const char *place, const char *type_name, const char *value) {
    ValueType type;
    if (!value_type_parse(type_name, &type) || type == TYPE_STR || !*value) {
        fail("error: write PLACE TYPE VALUE\n");
        return;
    }
    arm_ptr address = place_resolve(place);
    if (!arm_readable(address)) {
        fail("error: invalid address 0x%x\n", address);
        return;
    }
    uint32_t raw = type == TYPE_F32 ? float_to_bits((float)strtod(value, NULL))
                                    : (uint32_t)strtoll(value, NULL, 0);
    memcpy(to_local(address), &raw, (size_t)value_bytes(type));
    char text[MAX_TEXT];
    value_format(type, address, text, sizeof text);
    debug_send("0x%08x  %s\n", address, text);
}
static void command_dump(const char *place, const char *bytes_text) {
    arm_ptr address = place_resolve(place);
    uint32_t bytes = *bytes_text ? parse_number(bytes_text) : 64;
    for (uint32_t row = 0; row < bytes; row += 16, address += 16) {
        char text[100];
        int used = snprintf(text, sizeof text, "0x%08x ", address);
        char ascii[17] = {0};
        for (uint32_t i = 0; i < 16 && row + i < bytes; i++) {
            if (!arm_readable(address + i)) {
                debug_send("%s <invalid>\n", text);
                return;
            }
            uint8_t byte = *((const uint8_t *)to_local(address + i));
            used += snprintf(text + used, sizeof text - (size_t)used, " %02x", byte);
            ascii[i] = byte >= 32 && byte < 127 ? (char)byte : '.';
        }
        debug_send("%-58s %s\n", text, ascii);
    }
}
static void command_info(void) {
    arm_ptr start;
    uint32_t range;
    arm_memory_range(&start, &range);
    debug_send(".so = 0x%x\nsearch range = 0x%x, %u bytes\nJNIEnv = 0x%x\nreturns = %u\n", arm_so_base(),
               start, range, arm_jni_env(), polls);
    if (last_method) debug_send("last method = %s\n", last_method);
    for (int i = 0; i < pause_count; i++) debug_send("pause after %s\n", pause_methods[i]);
    debug_send("%s\n", paused ? "paused" : "running");
}
/* Pausa cada vez que vuelva este método. Vale su nombre completo o su parte
   final:
   com/juego/Natives.OnClose(I)I, Natives.OnClose(I)I u OnClose. */
void debug_pause_at(const char *method) {
    if (pause_count == MAX_PAUSES) {
        debug_report("[debug] too many pauses\n");
        return;
    }
    size_t bytes = strlen(method) + 1;
    char *copy = malloc(bytes);
    memcpy(copy, method, bytes);
    pause_methods[pause_count++] = copy;
}
static int pause_remove(const char *method) {
    for (int i = 0; i < pause_count; i++)
        if (!strcmp(pause_methods[i], method)) {
            pause_methods[i] = pause_methods[--pause_count];
            return 1;
        }
    return 0;
}
/* ¿wanted es el final de este texto, justo detrás de un separador? */
static int ends_with(const char *text, size_t bytes, const char *wanted) {
    size_t n = strlen(wanted);
    return bytes == n ? !strncmp(text, wanted, n)
                      : bytes > n && !strncmp(text + bytes - n, wanted, n) && strchr("/._", text[bytes - n - 1]);
}
static int pause_requested(const char *method) {
    size_t bytes = strlen(method), name = strcspn(method, "(");
    for (int i = 0; i < pause_count; i++)
        if (ends_with(method, bytes, pause_methods[i]) || ends_with(method, name, pause_methods[i])) return 1;
    return 0;
}
static void command_help(void) {
    debug_send("info                        where things are, and whether it is paused\n"
               "read PLACE [TYPE] [COUNT]   values, one after another\n"
               "write PLACE TYPE VALUE      change a value\n"
               "dump PLACE [BYTES]          hexadecimal\n");
    debug_send("find TYPE VALUE|any         start a search\n"
               "next VALUE|changed|same|up|down   keep the hits that match\n"
               "results [COUNT]             show the hits\n");
    debug_send("watch NAME PLACE [TYPE]     report every change\n"
               "unwatch NAME                stop following it\n"
               "watches                     show them\n");
    debug_send("pause                       stop at the next return\n"
               "pause METHOD                stop every time this method returns\n"
               "unpause METHOD              stop doing that\n"
               "run [COUNT]                 continue, and pause after COUNT returns\n"
               "resume                      continue\n"
               "quit                        close this connection\n"
               "PLACE: @0x4AD0 in the .so, 0x60805498 absolute, or a symbol, then +OFFSET or ->OFFSET\n"
               "TYPE: i8 u8 i16 u16 i32 u32 f32 ptr str (i32 by default)\n");
}
/* Ejecuta una línea. Cada respuesta termina con una línea ok o error, para
   poder usarla desde un programa. */
static void run_command(char *text) {
    char *argument = next_word(text);
    if (!*text) return;
    failed = 0;
    if (!strcmp(text, "watch")) {
        char *spec = next_word(argument);
        if (*argument && *spec) watch_add(argument, spec);
        else fail("error: watch NAME PLACE [TYPE]\n");
        if (!failed) debug_send("ok\n");
        return;
    }
    char *a = argument, *b = next_word(a), *c = next_word(b);
    next_word(c);
    if (!strcmp(text, "help")) command_help();
    else if (!strcmp(text, "info")) command_info();
    else if (!strcmp(text, "read")) command_read(a, b, c);
    else if (!strcmp(text, "write")) command_write(a, b, c);
    else if (!strcmp(text, "dump")) command_dump(a, b);
    else if (!strcmp(text, "find")) command_find(a, b);
    else if (!strcmp(text, "next")) command_next(a);
    else if (!strcmp(text, "results")) show_hits(*a ? parse_number(a) : 100);
    else if (!strcmp(text, "unwatch")) {
        if (!watch_remove(a)) fail("error: no such watch\n");
    }
    else if (!strcmp(text, "watches")) watch_list();
    else if (!strcmp(text, "pause")) {
        if (*a) debug_pause_at(a);
        else paused = 1;
    }
    else if (!strcmp(text, "unpause")) {
        if (!pause_remove(a)) fail("error: no such pause\n");
    }
    else if (!strcmp(text, "resume")) {
        paused = 0;
        run_left = 0;
    } else if (!strcmp(text, "run")) {
        paused = 0;
        run_left = *a ? parse_number(a) : 0;
    } else if (!strcmp(text, "quit")) {
        debug_send("bye\n");
        close(client_fd);
        client_fd = -1;
        line_bytes = 0;
        return;
    } else {
        fail("error: unknown command %s, try help\n", text);
    }
    if (!failed) debug_send("ok\n");
}
/* Separa lo recibido en líneas y las ejecuta. */
static void read_client(void) {
    ssize_t got = recv(client_fd, line + line_bytes, sizeof line - line_bytes - 1, MSG_DONTWAIT);
    if (got == 0 || (got < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
        close(client_fd);
        client_fd = -1;
        line_bytes = 0;
        return;
    }
    if (got < 0) return;
    line_bytes += (size_t)got;
    line[line_bytes] = 0;
    char *start = line, *end;
    while (client_fd >= 0 && (end = strchr(start, '\n'))) {
        *end = 0;
        if (end > start && end[-1] == '\r') end[-1] = 0;
        run_command(start);
        start = end + 1;
    }
    if (client_fd < 0) return;
    line_bytes = strlen(start);
    /* una línea demasiado larga se descarta */
    if (line_bytes == sizeof line - 1) line_bytes = 0;
    else memmove(line, start, line_bytes);
}
/* Atiende lo que haya pendiente. Con wait = 1 se bloquea hasta que llegue
   algo. */
static void service(int wait) {
    struct pollfd fds[1];
    fds[0].fd = client_fd >= 0 ? client_fd : listen_fd;
    fds[0].events = POLLIN;
    if (poll(fds, 1, wait ? -1 : 0) <= 0) return;
    if (client_fd >= 0) {
        read_client();
        return;
    }
    client_fd = accept(listen_fd, NULL, NULL);
    if (client_fd >= 0) debug_send(paused ? "connected, paused. Try help\n" : "connected. Try help\n");
}
/* Atiende la consola. Si está en pausa, no vuelve hasta que le digan. */
static void debug_poll(void) {
    if (listen_fd < 0) return;
    polls++;
    if (run_left && --run_left == 0) {
        paused = 1;
        debug_send("paused after %u returns\n", polls);
    }
    do service(paused);
    while (paused);
}
/* Si alguien pidió parar en este método, avisa y lo deja preparado para que se
   pare. */
static void check_pause(const char *method) {
    static int warned;
    last_method = method;
    if (!pause_count || !pause_requested(method)) return;
    if (listen_fd >= 0) {
        paused = 1;
        debug_report("paused after %s\n", method);
    } else if (!warned) {
        warned = 1;
        log_msg("[debug] the pause rule needs --debug\n");
    }
}
void debug_after_call(const char *method) {
    watch_poll(method);
    check_pause(method);
    debug_poll();
}
/* Al volver de una función nativa llamada por el juego, como un método Java de
   tu port. El juego sigue dentro de la llamada, así que no avanza hasta que le
   digas. */
void debug_after_native(const char *method) {
    check_pause(method);
    if (!paused) return;
    watch_poll(method);
    while (paused) service(1);
}
static void remove_socket(void) { unlink(socket_path); }

/* Abre el socket. Con start_paused, se para aquí, antes de ejecutar ningún
   método del juego, hasta que le digas que siga. Ya están hechos los
   constructores y JNI_OnLoad. */
void debug_start(const char *name, int start_paused) {
    static char path[sizeof ((struct sockaddr_un *)0)->sun_path];
    struct sockaddr_un address;
    memset(&address, 0, sizeof address);
    address.sun_family = AF_UNIX;
    if (snprintf(path, sizeof path, "%s/%s", data_dir(), name) >= (int)sizeof path)
        die("the debug socket path is too long");
    mkdir(data_dir(), 0755); /* puede no existir aún si no se ha extraído ningún
                                .apk */
    strcpy(address.sun_path, path);
    unlink(path); /* el que haya quedado de una ejecución anterior */
    listen_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (listen_fd < 0 || bind(listen_fd, (struct sockaddr *)&address, sizeof address) || listen(listen_fd, 1))
        die("cannot open the debug socket");
    socket_path = path;
    atexit(remove_socket);
    paused = start_paused;
    log_msg("[debug] listening on %s%s\n", path, start_paused ? ", paused until you connect" : "");
    while (paused) service(1);
}
