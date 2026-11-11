/* transfer/receiver.c: la recepción. Solo mientras está abierta, este
   dispositivo se anuncia y atiende ofertas: una a la vez, de una versión
   instalada y con el mismo fichero. Con Accept espera la partida,
   comprueba su CRC32 y la guarda (la de antes queda como backup). A lo que
   el otro repite se le repite la última respuesta. */
#include "receiver.h"
#include "identity.h"
#include "platform.h"
#include "saves.h"

#define DATA_MS 10000 /* tras Accept, para que llegue la partida */

static uint64_t now_ms(void) { return clock_ns() / 1000000u; }

int receiver_open(Receiver *r, ApkChoice *choices, int count) {
    memset(r, 0, sizeof *r);
    r->choices = choices;
    r->count = count;
    identity_name(r->name, sizeof r->name);
    identity_new_signature(r->signature);
    return net_open(&r->net, 'R', r->name, r->signature);
}

void receiver_close(Receiver *r) { net_close(&r->net); }

int receiver_version(const Receiver *r) {
    for (int i = 0; i < r->count; i++)
        if (!strcmp(r->choices[i].md5, r->offer.game)) return i;
    return -1;
}

/* Contesta al de la oferta, y lo recuerda para repetirlo. */
static void reply(Receiver *r, const char *kind, const char *words) {
    snprintf(r->reply_kind, sizeof r->reply_kind, "%s", kind);
    snprintf(r->reply_words, sizeof r->reply_words, "%s", words ? words : "");
    net_send(&r->net, &r->from, kind, words, NULL, 0);
}

static void reply_again(Receiver *r) {
    net_send(&r->net, &r->from, r->reply_kind, r->reply_words[0] ? r->reply_words : NULL, NULL, 0);
}

/* La oferta: NULL si está bien, o el motivo para rechazarla. */
static const char *parse_offer(Receiver *r, char *text) {
    Offer *o = &r->offer;
    char signature[5];
    snprintf(signature, sizeof signature, "%s", o->signature);
    memset(o, 0, sizeof *o);
    snprintf(o->signature, sizeof o->signature, "%s", signature);
    o->size = -1;
    for (char *line = strtok(text, "\n"); line; line = strtok(NULL, "\n")) {
        char *value = strchr(line, ' ');
        if (!value) continue;
        *value++ = 0;
        if (!strcmp(line, "name")) snprintf(o->name, sizeof o->name, "%s", value);
        else if (!strcmp(line, "game")) snprintf(o->game, sizeof o->game, "%s", value);
        else if (!strcmp(line, "file")) snprintf(o->file, sizeof o->file, "%s", value);
        else if (!strcmp(line, "summary")) snprintf(o->summary, sizeof o->summary, "%s", value);
        else if (!strcmp(line, "size")) o->size = strtol(value, NULL, 10);
        else if (!strcmp(line, "date")) o->date = strtoll(value, NULL, 10);
    }
    identity_clean(o->name, sizeof o->name);
    identity_clean(o->summary, sizeof o->summary);
    if (!o->name[0] || strlen(o->game) != 32 || strspn(o->game, "0123456789abcdef") != 32) return "format";
    if (o->size < 1 || o->size > SAVE_MAX || !save_known_file(o->file)) return "format";
    if (receiver_version(r) < 0) return "version";
    /* el mismo formato: si ya hay partida aquí, el mismo fichero y tamaño */
    Save mine;
    if (save_find(o->game, 0, &mine) && (strcmp(mine.file, o->file) || mine.size != o->size)) return "format";
    return NULL;
}

/* Una oferta nueva: 1 si hay que preguntar. */
static int new_offer(Receiver *r, const Packet *p) {
    if (!r->listening || r->stage == RECEIVER_ASKING || r->stage == RECEIVER_ACCEPTED) {
        net_send(&r->net, &p->from, "NO", "busy", NULL, 0);
        return 0;
    }
    r->from = p->from;
    snprintf(r->offer.signature, sizeof r->offer.signature, "%s", p->signature);
    snprintf(r->offer_number, sizeof r->offer_number, "%s", p->words);
    const char *problem = parse_offer(r, (char *)p->body);
    if (problem) {
        r->stage = RECEIVER_FINISHED;
        reply(r, "NO", problem);
        log_msg("[transfer] offer rejected: %s\n", problem);
        r->notice = !strcmp(problem, "version") ? "an offer for a version not installed here"
                                                 : "an offer with another save format";
        return 0;
    }
    r->stage = RECEIVER_ASKING;
    reply(r, "WAIT", NULL);
    return 1;
}

/* La partida: DATA <bytes> <crc32 en hex>, y los bytes. */
static const char *take_data(Receiver *r, const Packet *p) {
    long size;
    unsigned crc;
    char extra;
    if (sscanf(p->words, "%ld %x%c", &size, &crc, &extra) != 2 || size != r->offer.size ||
        p->body_size != (size_t)size) {
        reply(r, "FAIL", "format");
        return "the save did not arrive well";
    }
    if (save_crc32(p->body, p->body_size) != crc) {
        reply(r, "FAIL", "crc");
        return "the save arrived damaged";
    }
    if (!save_store(r->offer.game, r->offer.file, p->body, size)) {
        reply(r, "FAIL", "write");
        return "cannot write the save";
    }
    reply(r, "DONE", NULL);
    return "save received";
}

static int done(Receiver *r, const char *result) {
    r->stage = RECEIVER_FINISHED;
    r->result = result;
    log_msg("[transfer] save from %s %s for %s: %s\n", r->offer.name, r->offer.signature, r->offer.game, result);
    return RECEIVER_DONE;
}

int receiver_poll(Receiver *r) {
    Packet p;
    while (net_next(&r->net, &p)) {
        int current = r->stage != RECEIVER_IDLE && net_same(&p.from, &r->from) &&
                      !strcmp(p.signature, r->offer.signature);
        if (current && !strcmp(p.kind, "CANCEL") &&
            (r->stage == RECEIVER_ASKING || r->stage == RECEIVER_ACCEPTED)) {
            r->stage = RECEIVER_IDLE;
            r->notice = "the other device cancelled";
            return RECEIVER_CANCELLED;
        }
        if (current && !strcmp(p.kind, "DATA") && r->stage == RECEIVER_ACCEPTED) return done(r, take_data(r, &p));
        int offer = !strcmp(p.kind, "OFFER");
        if (current && (offer ? !strcmp(p.words, r->offer_number) : !strcmp(p.kind, "DATA"))) reply_again(r);
        else if (offer && new_offer(r, &p)) return RECEIVER_OFFER;
    }
    if (r->stage == RECEIVER_ACCEPTED && now_ms() > r->deadline) return done(r, "the save did not arrive");
    return 0;
}

void receiver_accept(Receiver *r) {
    if (r->stage != RECEIVER_ASKING) return;
    r->stage = RECEIVER_ACCEPTED;
    r->deadline = now_ms() + DATA_MS;
    reply(r, "OK", NULL);
}

void receiver_decline(Receiver *r) {
    if (r->stage != RECEIVER_ASKING) return;
    r->stage = RECEIVER_FINISHED;
    reply(r, "NO", "declined");
    r->notice = "declined";
}

void receiver_cancel(Receiver *r) {
    if (r->stage != RECEIVER_ACCEPTED) return;
    r->stage = RECEIVER_FINISHED;
    reply(r, "NO", "cancelled");
}
