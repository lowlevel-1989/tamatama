/* transfer/identity.c: el nombre y la firma de este dispositivo. */
#include "identity.h"
#include "common.h"
#include <fcntl.h>
#include <unistd.h>
#ifdef __ANDROID__
#include <sys/system_properties.h>
#endif

void identity_clean(char *text, size_t room) {
    size_t n = strlen(text);
    if (n >= room) text[n = room - 1] = 0;
    for (size_t i = 0; i < n; i++)
        if (text[i] < 32 || text[i] > 126) text[i] = '?';
}

#ifndef __ANDROID__
/* El usuario de /etc/passwd con el uid de este proceso (getpwuid no vale en
   un programa estático como loader-arm), o $USER. */
static void user_name(char *out, size_t room) {
    snprintf(out, room, "%s", getenv("USER") ? getenv("USER") : "user");
    FILE *file = fopen("/etc/passwd", "r");
    if (!file) return;
    char line[256];
    while (fgets(line, sizeof line, file)) {
        char *name = strtok(line, ":"), *password = strtok(NULL, ":"), *uid = strtok(NULL, ":");
        if (name && password && uid && (uid_t)atoi(uid) == getuid()) {
            snprintf(out, room, "%.31s", name);
            break;
        }
    }
    fclose(file);
}

/* El NAME de /etc/os-release, sin comillas. */
static void system_name(char *out, size_t room) {
    snprintf(out, room, "Linux");
    FILE *file = fopen("/etc/os-release", "r");
    if (!file) return;
    char line[256];
    while (fgets(line, sizeof line, file)) {
        if (strncmp(line, "NAME=", 5)) continue;
        char *value = line + 5;
        value[strcspn(value, "\r\n")] = 0;
        size_t n = strlen(value);
        if (n >= 2 && (value[0] == '"' || value[0] == '\'') && value[n - 1] == value[0]) {
            value[n - 1] = 0;
            value++;
        }
        if (*value) snprintf(out, room, "%.31s", value);
        break;
    }
    fclose(file);
}
#endif

void identity_name(char *out, size_t room) {
#ifdef __ANDROID__
    /* Android no tiene un usuario ni un hostname que digan algo: el modelo
       del teléfono, por ejemplo Pixel 10a (Android) */
    char model[PROP_VALUE_MAX] = "";
    if (__system_property_get("ro.product.model", model) <= 0) snprintf(model, sizeof model, "phone");
    snprintf(out, room, "%s (Android)", model);
#else
    char user[32], host[64] = "", system[32];
    user_name(user, sizeof user);
    if (gethostname(host, sizeof host - 1) || !host[0]) snprintf(host, sizeof host, "device");
    system_name(system, sizeof system);
    /* si no cabe, se corta: identity_clean lo deja bien terminado */
    if (snprintf(out, room, "%s@%s (%s)", user, host, system) >= (int)room) out[room - 1] = 0;
#endif
    identity_clean(out, room);
}

/* Sin los que se confunden: 0 y O, 1, I y L. */
static const char SIGNATURE_CHARS[] = "23456789ABCDEFGHJKMNPQRSTUVWXYZ";

void identity_new_signature(char out[5]) {
    unsigned char bytes[4];
    int file = open("/dev/urandom", O_RDONLY);
    if (file < 0 || read(file, bytes, 4) != 4) {
        uint64_t seed = clock_ns() ^ ((uint64_t)getpid() << 16);
        for (int i = 0; i < 4; i++) bytes[i] = (unsigned char)(seed >> (i * 8 + 3));
    }
    if (file >= 0) close(file);
    for (int i = 0; i < 4; i++) out[i] = SIGNATURE_CHARS[bytes[i] % (sizeof SIGNATURE_CHARS - 1)];
    out[4] = 0;
}

int identity_valid_signature(const char *text) {
    if (strlen(text) != 4) return 0;
    for (int i = 0; i < 4; i++)
        if (!strchr(SIGNATURE_CHARS, text[i])) return 0;
    return 1;
}
