/* transfer/sender.c: el envío. La oferta se repite cada segundo hasta que
   el otro contesta, y con su Accept la partida, hasta que dice que la ha
   guardado: así no importa que se pierda un paquete. */
#include "sender.h"
#include "common.h"
#include "identity.h"
#include "platform.h"

#define DEVICE_GONE_MS 4000 /* sin anunciarse, desaparece de la lista */
#define RETRY_MS 1000
#define REACH_MS 5000       /* para que el otro diga que tiene la oferta */
#define ANSWER_MS 60000     /* para que pulse Accept o Decline */
#define DATA_MS 10000       /* para que diga que ha guardado la partida */

static uint64_t now_ms(void) { return clock_ns() / 1000000u; }

int sender_open(Sender *s) {
    memset(s, 0, sizeof *s);
    identity_name(s->name, sizeof s->name);
    identity_new_signature(s->signature);
    return net_open(&s->net, 'S', s->name, s->signature);
}

void sender_close(Sender *s) {
    free(s->data);
    s->data = NULL;
    net_close(&s->net);
}

/* Un anuncio de alguien en Receive Save: R <nombre>. Un dispositivo con
   varias redes (Wi-Fi, una VPN) llega por cada una con otra dirección: se
   distingue por la firma, y se queda la primera dirección. */
static void seen_device(Sender *s, const Packet *p) {
    if (strncmp(p->words, "R ", 2)) return;
    for (int i = 0; i < s->count; i++)
        if (!strcmp(s->devices[i].signature, p->signature)) {
            s->devices[i].seen = now_ms();
            return;
        }
    if (s->count == SENDER_DEVICES) return;
    Device *device = &s->devices[s->count++];
    device->address = p->from;
    snprintf(device->signature, sizeof device->signature, "%s", p->signature);
    snprintf(device->name, sizeof device->name, "%s", p->words + 2);
    identity_clean(device->name, sizeof device->name);
    device->seen = now_ms();
}

static void finish(Sender *s, const char *result) {
    s->result = result;
    s->stage = SENDER_IDLE;
    free(s->data);
    s->data = NULL;
    log_msg("[transfer] send to %s %s: %s\n", s->target.name, s->target.signature, result);
}

/* Lo que contesta el dispositivo al que se manda. */
static void answer(Sender *s, const Packet *p) {
    uint64_t now = now_ms();
    if (!strcmp(p->kind, "WAIT") && s->stage == SENDER_OFFERING) {
        s->stage = SENDER_ASKED;
        s->deadline = now + ANSWER_MS;
    } else if (!strcmp(p->kind, "OK") && s->stage != SENDER_SENDING) {
        s->stage = SENDER_SENDING;
        s->deadline = now + DATA_MS;
        s->next_retry = 0;
    } else if (!strcmp(p->kind, "NO")) {
        finish(s, !strcmp(p->words, "declined")  ? "declined"
                  : !strcmp(p->words, "version") ? "the other device does not have this version"
                  : !strcmp(p->words, "busy")    ? "the other device is busy"
                                                 : "rejected");
    } else if (!strcmp(p->kind, "DONE") && s->stage == SENDER_SENDING) {
        finish(s, "save sent");
    } else if (!strcmp(p->kind, "FAIL")) {
        finish(s, "the other device could not save it");
    }
}

int sender_poll(Sender *s) {
    int sending = s->stage != SENDER_IDLE;
    Packet p;
    while (net_next(&s->net, &p)) {
        if (!strcmp(p.kind, "HELLO") || !strcmp(p.kind, "HERE")) seen_device(s, &p);
        else if (s->stage != SENDER_IDLE && net_same(&p.from, &s->target.address) &&
                 !strcmp(p.signature, s->target.signature))
            answer(s, &p);
    }
    uint64_t now = now_ms();
    for (int i = 0; i < s->count;)
        if (now - s->devices[i].seen > DEVICE_GONE_MS) s->devices[i] = s->devices[--s->count];
        else i++;
    if (s->stage == SENDER_IDLE) return sending;
    if (now >= s->deadline) {
        finish(s, s->stage == SENDER_OFFERING ? "the other device does not answer"
                  : s->stage == SENDER_ASKED  ? "no answer"
                                              : "the save did not arrive");
        return 1;
    }
    if (now >= s->next_retry) {
        if (s->stage == SENDER_SENDING)
            net_send(&s->net, &s->target.address, "DATA", s->data_words, s->data, (size_t)s->size);
        else
            net_send(&s->net, &s->target.address, "OFFER", s->offer_words, s->offer, strlen(s->offer));
        s->next_retry = now + RETRY_MS;
    }
    return 0;
}

const char *sender_start(Sender *s, int device, const char *md5, const Save *save) {
    if (device < 0 || device >= s->count) return "the device is gone";
    free(s->data);
    s->data = save_read(save->path, &s->size);
    if (!s->data || s->size != save->size) {
        free(s->data);
        s->data = NULL;
        return "cannot read the save";
    }
    s->target = s->devices[device];
    snprintf(s->offer, sizeof s->offer, "name %s\ngame %s\nfile %s\nsize %ld\ndate %lld\nsummary %s\n", s->name, md5,
             save->file, save->size, (long long)save->date, save->summary);
    snprintf(s->data_words, sizeof s->data_words, "%ld %08x", s->size, (unsigned)save_crc32(s->data, (size_t)s->size));
    /* el número distingue una oferta nueva de la repetición de la anterior
       (tras un Decline, por ejemplo) */
    snprintf(s->offer_words, sizeof s->offer_words, "%u", ++s->offers);
    s->stage = SENDER_OFFERING;
    s->next_retry = 0;
    s->deadline = now_ms() + REACH_MS;
    s->result = NULL;
    return NULL;
}

void sender_cancel(Sender *s) {
    if (s->stage == SENDER_IDLE) return;
    net_send(&s->net, &s->target.address, "CANCEL", NULL, NULL, 0);
    finish(s, "cancelled");
}
