/* shared/apk.c: el juego tal como se descarga, en un .apk. Antes de cargar nada, comprueba que es
   una versión conocida: su md5 tiene que estar en src/port/apk_table.h. Después saca de él lo que
   necesita el programa, en una carpeta propia de esa versión:

     resources/{md5}/libs/      el .so del juego, sin las carpetas lib/armeabi/ del .apk
     resources/{md5}/assets/    los assets, para el gestor de resources del port

   Así conviven varias versiones. Solo extrae la primera vez: al terminar deja el fichero
   .complete en su carpeta, y si ya está, lo extraído sigue valiendo. Si una extracción se corta
   a medias, no hay .complete y se repite.

   resources/ es data_dir(), donde la plataforma deja escribir: en Linux, junto al programa y no
   en el directorio actual. Lo que depende de la plataforma (data_dir, base_dir y dónde buscar el
   .apk) está en platform/platform.h. El md5 es
   propio (md5.c) y el .apk se lee con miniz (third_party/miniz): no hace falta ningún programa
   externo. */
/* Offsets de 64 bits en readdir y fopen también en ARM de 32 bits: si no, bajo QEMU
   readdir falla con las carpetas de ext4 y no se abren ficheros de más de 2 GiB. */
#define _FILE_OFFSET_BITS 64
#include "common.h"
#include "platform.h"
#include "md5.h"
#include "miniz.h"
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>

/* La carpeta del .apk que se está usando: resources/{md5}. Sin .apk, resources. */
static char base[PATH_MAX];
/* La versión en uso: su posición en la lista, o -1 si no se sabe (un .so desconocido). */
static int in_use = -1;

/* La lista de versiones compatibles: una línea APK("md5", "nombre") por cada una. */
static const struct {
    const char *md5, *name;
} supported[] = {

#define APK(md5, name) {md5, name},
#if __has_include("apk_table.h")
#include "apk_table.h"
#endif
#undef APK
    {0, 0}};

/* Escribe en out (de PATH_MAX bytes) la ruta a/b. Una ruta cortada no sirve: termina. */
static char *join(char *out, const char *a, const char *b) {
    if (snprintf(out, PATH_MAX, "%s/%s", a, b) >= PATH_MAX) die("path too long");
    return out;
}

/* La carpeta resources/: donde la plataforma deja escribir (en Linux, {base_dir}/resources). */
static const char *resources_dir(void) { return data_dir(); }

/* La posición de md5 en la lista, o -1 si no es una versión compatible. */
static int supported_index(const char *md5) {
    for (int i = 0; supported[i].md5; i++)
        if (!strcmp(supported[i].md5, md5)) return i;
    return -1;
}

/* 1 si el nombre se queda dentro de su carpeta: no empieza por / ni tiene un componente .. */
static int safe_name(const char *name) {
    if (!name[0] || name[0] == '/') return 0;
    for (const char *p = name; *p;) {
        size_t n = strcspn(p, "/");
        if (n == 2 && p[0] == '.' && p[1] == '.') return 0;
        p += n;
        if (*p) p++;
    }
    return 1;
}

/* Crea las carpetas que faltan en el camino de path, sin la última parte. */
static void make_parents(char *path) {
    for (char *p = path + 1; *p; p++) {
        if (*p != '/') continue;
        *p = 0;
        if (mkdir(path, 0755) && errno != EEXIST) {
            log_msg("[apk] cannot create %s\n", path);
            die("cannot create a folder in resources/");
        }
        *p = '/';
    }
}

/* Dónde va una entrada del .apk dentro de dir, o 0 si no se extrae. Los .so de lib/armeabi/ van
   todos a libs/, sin sus carpetas; assets/ conserva las suyas. */
static int destination(const char *name, const char *dir, char *out, size_t room) {
    size_t n = strlen(name);
    int written;
    if (!strncmp(name, "lib/armeabi/", 12) && n > 3 && !strcmp(name + n - 3, ".so"))
        written = snprintf(out, room, "%s/libs/%s", dir, strrchr(name, '/') + 1);
    else if (!strncmp(name, "assets/", 7))
        written = snprintf(out, room, "%s/%s", dir, name);
    else
        return 0;
    if (written < 0 || (size_t)written >= room) die("a file name in the apk is too long");
    return 1;
}

/* Lo que miniz va descomprimiendo: se escribe en el fichero y se acumula su CRC32. */
typedef struct {
    FILE *file;
    mz_ulong crc;
} Sink;

static size_t write_out(void *opaque, mz_uint64 offset, const void *data, size_t n) {
    (void)offset; /* llega en orden */
    Sink *sink = opaque;
    sink->crc = mz_crc32(sink->crc, data, n);
    return fwrite(data, 1, n, sink->file);
}

/* Extrae el .so y los assets del .apk en dir. Primero revisa todos los nombres, y si alguno se
   saldría de la carpeta, rechaza el .apk sin escribir nada. */
static void extract(const char *apk, const char *dir) {
    mz_zip_archive zip;
    mz_zip_archive_file_stat entry;
    char path[PATH_MAX];
    memset(&zip, 0, sizeof zip);
    if (!mz_zip_reader_init_file(&zip, apk, 0)) die("the apk is not a valid zip file");
    mz_uint count = mz_zip_reader_get_num_files(&zip);
    int libraries = 0;
    for (mz_uint i = 0; i < count; i++) {
        if (!mz_zip_reader_file_stat(&zip, i, &entry)) die("cannot read the apk index");
        if (!safe_name(entry.m_filename)) {
            log_msg("[apk] %s: unsafe file name: %s\n", apk, entry.m_filename);
            die("the apk tries to write outside resources/");
        }
        if (!entry.m_is_directory && destination(entry.m_filename, dir, path, sizeof path) &&
            !strncmp(entry.m_filename, "lib/", 4))
            libraries++;
    }
    if (!libraries) die("the apk has no lib/armeabi/*.so");

    log_msg("[apk] extracting into %s/\n", dir);
    for (mz_uint i = 0; i < count; i++) {
        mz_zip_reader_file_stat(&zip, i, &entry);
        if (!destination(entry.m_filename, dir, path, sizeof path)) continue;
        make_parents(path);
        if (entry.m_is_directory) {
            if (mkdir(path, 0755) && errno != EEXIST) die("cannot create a folder in resources/");
            continue;
        }
        Sink sink = {fopen(path, "wb"), MZ_CRC32_INIT};
        if (!sink.file) {
            log_msg("[apk] cannot write %s\n", path);
            die("cannot write into resources/");
        }
        int ok = mz_zip_reader_extract_to_callback(&zip, i, write_out, &sink, 0);
        ok = !fclose(sink.file) && ok;
        if (!ok || sink.crc != entry.m_crc32) {
            log_msg("[apk] %s: %s\n", entry.m_filename,
                    ok ? "CRC32 does not match" : mz_zip_get_error_string(mz_zip_get_last_error(&zip)));
            die("cannot extract the apk");
        }
    }
    mz_zip_reader_end(&zip);
}

/* El único .so de {base}/libs. El loader carga uno solo: si hay varios, no puede elegir. */
static const char *find_library(const char *libs) {
    static char path[PATH_MAX];
    DIR *dir = opendir(libs);
    if (!dir) die("the apk folder has no libs");
    int found = 0;
    for (struct dirent *e; (e = readdir(dir));) {
        size_t n = strlen(e->d_name);
        if (n > 3 && !strcmp(e->d_name + n - 3, ".so")) {
            log_msg("[apk] library: %s\n", e->d_name);
            join(path, libs, e->d_name);
            found++;
        }
    }
    closedir(dir);
    if (found != 1) die(found ? "several .so in the apk: the loader needs only one" : "no .so in the apk");
    return path;
}

/* ---------- El registry: resources/registry.txt ----------
   Con game.so, la versión no sale de ningún .apk, y sin ella no se sabe dónde están sus assets
   ni sus reglas. El registry lo recuerda, con una línea por cada .so extraído:
     so MD5_DEL_SO MD5_DEL_APK
   Se pone al día cada vez que se usa una versión conocida, así que se rellena solo. */
#define MAX_REGISTERED 64
static struct {
    char so[33], apk[33];
} registered[MAX_REGISTERED];
static int registered_count, registry_loaded;

static const char *registry_path(void) {
    static char path[PATH_MAX];
    if (!path[0]) join(path, resources_dir(), "registry.txt");
    return path;
}

/* Lee el registry, una vez. Las líneas que no entiende se ignoran: el registry se puede borrar
   sin perder nada, porque se rehace al usar cada versión. */
static void registry_load(void) {
    if (registry_loaded) return;
    registry_loaded = 1;
    char *text = read_file(registry_path());
    if (!text) return;
    char so[33], apk[33];
    for (char *line = strtok(text, "\n"); line; line = strtok(NULL, "\n")) {
        if (sscanf(line, "so %32s %32s", so, apk) == 2 && registered_count < MAX_REGISTERED) {
            memcpy(registered[registered_count].so, so, 33);
            memcpy(registered[registered_count].apk, apk, 33);
            registered_count++;
        }
    }
    free(text);
}

/* Escribe el registry en un fichero temporal y lo renombra: un corte no lo deja a medias. */
static void registry_save(void) {
    char temporary[PATH_MAX];
    join(temporary, resources_dir(), "registry.txt.tmp");
    mkdir(resources_dir(), 0755);
    FILE *file = fopen(temporary, "w");
    if (!file) {
        log_msg("[apk] cannot write %s\n", temporary);
        return;
    }
    fprintf(file, "# Written by the loader: which version each extracted .so belongs to.\n"
                  "# It can be deleted: it is rebuilt.\n");
    for (int i = 0; i < registered_count; i++)
        fprintf(file, "so %s %s\n", registered[i].so, registered[i].apk);
    if (fclose(file) || rename(temporary, registry_path())) log_msg("[apk] cannot write %s\n", registry_path());
}

/* Apunta que el .so con ese md5 es de la versión i. */
static void registry_note(int i, const char *so_md5) {
    registry_load();
    int changed = 0, found = 0;
    for (int k = 0; k < registered_count; k++) {
        if (strcmp(registered[k].so, so_md5)) continue;
        found = 1;
        if (strcmp(registered[k].apk, supported[i].md5)) {
            memcpy(registered[k].apk, supported[i].md5, 33);
            changed = 1;
        }
    }
    if (!found && registered_count < MAX_REGISTERED) {
        memcpy(registered[registered_count].so, so_md5, 33);
        memcpy(registered[registered_count].apk, supported[i].md5, 33);
        registered_count++;
        changed = 1;
    }
    if (changed) registry_save();
}

/* Pasa a usar la versión i: desde aquí, apk_resources y apk_rules son los suyos. */
static void set_version(int i) {
    in_use = i;
    join(base, resources_dir(), supported[i].md5);
}

/* Pasa a usar la versión i de la lista: la extrae de apk si aún no está completa (apk puede
   ser NULL si ya lo está) y devuelve la ruta de su .so. */
static const char *use_version(int i, const char *apk) {
    char libs[PATH_MAX], complete[PATH_MAX];
    set_version(i);
    join(libs, base, "libs");
    join(complete, base, ".complete");
    if (access(complete, F_OK)) {
        extract(apk, base);
        FILE *done = fopen(complete, "w");
        if (!done || fclose(done)) die("cannot write the .complete file");
    }
    const char *library = find_library(libs);
    char so_md5[33];
    if (!md5_file(library, so_md5)) registry_note(i, so_md5);
    return library;
}

/* Con game.so: su versión, si es un .so extraído de un .apk compatible. */
static void use_library_version(const char *path) {
    char md5[33];
    if (md5_file(path, md5)) return; /* no se puede leer: ya fallará al cargarlo */
    registry_load();
    for (int k = 0; k < registered_count; k++) {
        int i = strcmp(registered[k].so, md5) ? -1 : supported_index(registered[k].apk);
        if (i < 0) continue;
        set_version(i);
        registry_note(i, md5);
        log_msg("[apk] %s: .so of %s\n", path, supported[i].name);
        return;
    }
    /* No está en el registry (se borró, o se extrajo antes de que existiera): se busca entre
       los .so ya extraídos de cada versión, y si aparece, queda registrado. */
    char libs[PATH_MAX], dir[PATH_MAX], library[PATH_MAX], other[33];
    for (int i = 0; supported[i].md5; i++) {
        DIR *folder = opendir(join(libs, join(dir, resources_dir(), supported[i].md5), "libs"));
        if (!folder) continue;
        int found = 0;
        for (struct dirent *e; !found && (e = readdir(folder));) {
            size_t n = strlen(e->d_name);
            found = n > 3 && !strcmp(e->d_name + n - 3, ".so") && !md5_file(join(library, libs, e->d_name), other) &&
                    !strcmp(other, md5);
        }
        closedir(folder);
        if (!found) continue;
        set_version(i);
        registry_note(i, md5);
        log_msg("[apk] %s: .so of %s\n", path, supported[i].name);
        return;
    }
    log_msg("[apk] %s: md5 %s is not a .so extracted from a supported .apk: no version\n", path, md5);
}

/* Para apk_each: usa este .apk si es compatible. Si no, muestra su md5 y sigue buscando. */
static const char *found_library;
static int try_apk(const char *path) {
    char md5[33];
    if (md5_file(path, md5)) {
        log_msg("[apk] %s: cannot read it, ignored\n", path);
        return 0;
    }
    int i = supported_index(md5);
    if (i < 0) {
        log_msg("[apk] %s: md5 %s is not a supported version, ignored\n", path, md5);
        return 0;
    }
    log_msg("[apk] %s: %s\n", path, supported[i].name);
    found_library = use_version(i, path);
    return 1;
}

/* Sin argumento: lo ya extraído, si no un .apk aportado por el usuario, y si no, se lo pide. */
static const char *find_game(void) {
    char dir[PATH_MAX], complete[PATH_MAX];
    for (int i = 0; supported[i].md5; i++) {
        join(complete, join(dir, resources_dir(), supported[i].md5), ".complete");
        if (!access(complete, F_OK)) {
            log_msg("[apk] %s: already extracted\n", supported[i].name);
            return use_version(i, NULL);
        }
    }
    if (apk_each(try_apk)) return found_library;
    log_msg("[apk] The game is not included.\n"
            "[apk] %s\n"
            "[apk] Supported versions:\n", apk_hint());
    for (int i = 0; supported[i].md5; i++) log_msg("[apk]   %s\n", supported[i].name);
    die("no game found");
    return NULL;
}

/* Sin path, busca el juego (find_game). Si path es un .apk, lo comprueba, lo extrae si hace
   falta y devuelve la ruta de su .so. Si no, devuelve path tal cual, un .so, y su versión sale
   del registry. */
const char *apk_library(const char *path) {
    if (!path) return find_game();
    size_t n = strlen(path);
    if (n < 4 || strcmp(path + n - 4, ".apk")) {
        use_library_version(path);
        return path;
    }

    char md5[33];
    if (md5_file(path, md5)) die("cannot read the apk");
    int i = supported_index(md5);
    if (i < 0) {
        log_msg("[apk] %s: md5 %s is not in src/port/apk_table.h\n", path, md5);
        die("unsupported apk");
    }
    log_msg("[apk] %s: %s\n", path, supported[i].name);
    return use_version(i, path);
}

/* Dónde están los recursos del .apk en uso, para el gestor de resources: resources/{md5}, con
   assets/ dentro. Sin .apk, resources. */
const char *apk_resources(void) {
    if (!base[0]) snprintf(base, sizeof base, "%s", resources_dir());
    return base;
}

/* Las reglas propias de la versión en uso: {base_dir}/rules/{md5}.txt, si existe. Solo se
   usan cuando no se pasa un fichero de reglas, que siempre tiene prioridad. */
const char *apk_rules(void) {
    static char path[PATH_MAX];
    if (in_use < 0) return NULL;
    if (snprintf(path, sizeof path, "%s/rules/%s.txt", base_dir(), supported[in_use].md5) >= (int)sizeof path ||
        access(path, R_OK))
        return NULL;
    log_msg("[rules] %s: rules of %s\n", path, supported[in_use].name);
    return path;
}

/* La ruta de un asset por su nombre dentro de assets/, por ejemplo "data/level1.bin". */
int asset_path(char *out, size_t room, const char *name) {
    if (!safe_name(name)) return -1;
    int n = snprintf(out, room, "%s/assets/%s", apk_resources(), name);
    return n < 0 || (size_t)n >= room ? -1 : 0;
}

/* El asset abierto para leer, o NULL si no existe. */
FILE *asset_open(const char *name) {
    char path[PATH_MAX];
    return asset_path(path, sizeof path, name) ? NULL : fopen(path, "rb");
}
