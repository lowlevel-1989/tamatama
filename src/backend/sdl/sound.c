/* backend/sdl/sound.c: el sonido con SDL_mixer. Los efectos son Mix_Chunk, que
   pueden sonar varios a la vez, cada uno en su canal; la música es Mix_Music,
   y solo suena una. El audio se abre la primera vez que se carga un sonido:
   si no se puede, el juego sigue sin sonido. Un M4A, que SDL_mixer no lee,
   pasa antes a WAV (m4a.c).

   Una música M4A se decodifica en un thread (decode_music): la del título
   tarda segundos en una consola, y el juego espera a que load vuelva. Si se
   pide tocarla antes, suena en cuanto está lista, siempre que siga siendo la
   música pedida (wanted). */
#include <SDL.h>
#include <SDL_mixer.h>
#include "common.h"
#include "backend.h"
#include "m4a.h"

#define MAX_SOUNDS 256

static struct {
    Mix_Chunk *effect;
    Mix_Music *music;
    void *data;  /* Mix_Music lee el fichero mientras suena: se guarda aquí */
    int channel; /* el canal del último play de un efecto, o -1 */
    int pending; /* música aún decodificándose */
} sounds[MAX_SOUNDS + 1]; /* el 0 no se usa: es "no cargado" */
static SDL_mutex *lock;  /* sounds[].music, pending y wanted, con los threads */
static int wanted, wanted_loop; /* la música pedida (play), o 0 */
static int sound_count;
static int audio_state; /* 0 sin probar, 1 abierto, -1 no disponible */

static int load(const void *data, size_t size, int music);

static int audio_open(void) {
    if (audio_state) return audio_state > 0;
    audio_state = -1;
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
        log_msg("[sound] no audio: %s\n", SDL_GetError());
        return 0;
    }
    Mix_Init(MIX_INIT_OGG | MIX_INIT_MP3);
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 1024) < 0) {
        log_msg("[sound] no audio: %s\n", Mix_GetError());
        return 0;
    }
    Mix_AllocateChannels(16);
    lock = SDL_CreateMutex();
    audio_state = 1;
    return 1;
}

/* El thread de una música M4A: la decodifica y, si es la pedida, la toca. */
typedef struct {
    int handle;
    uint8_t *m4a;
    size_t size;
} Job;
static int decode_music(void *argument) {
    Job *job = argument;
    size_t size;
    uint8_t *wav = m4a_to_wav(job->m4a, job->size, &size);
    free(job->m4a);
    Mix_Music *music = wav ? Mix_LoadMUS_RW(SDL_RWFromConstMem(wav, (int)size), 1) : NULL;
    if (!music) {
        log_msg("[sound] cannot decode the m4a\n");
        free(wav);
    }
    SDL_LockMutex(lock);
    sounds[job->handle].music = music;
    sounds[job->handle].data = music ? wav : NULL;
    sounds[job->handle].pending = 0;
    if (music && wanted == job->handle) Mix_PlayMusic(music, wanted_loop ? -1 : 0);
    SDL_UnlockMutex(lock);
    free(job);
    return 0;
}

int backend_sound_load(const void *data, size_t size, int music) {
    if (!audio_open() || sound_count == MAX_SOUNDS) return 0;
    uint8_t *wav = NULL;
    if (music && lock && m4a_detect(data, size)) {
        Job *job = malloc(sizeof *job);
        uint8_t *copy = malloc(size);
        if (job && copy) {
            memcpy(copy, data, size);
            int handle = sound_count + 1;
            *job = (Job){handle, copy, size};
            sounds[handle].pending = 1;
            sounds[handle].channel = -1;
            sound_count = handle;
            SDL_Thread *thread = SDL_CreateThread(decode_music, "music", job);
            if (thread) {
                SDL_DetachThread(thread);
                return handle;
            }
            sounds[handle].pending = 0;
            sound_count = handle - 1;
        }
        free(job);
        free(copy);
    }
    if (m4a_detect(data, size)) {
        wav = m4a_to_wav(data, size, &size);
        if (!wav) {
            log_msg("[sound] cannot decode the m4a\n");
            return 0;
        }
        data = wav;
    }
    int handle = load(data, size, music);
    free(wav);
    return handle;
}

/* Carga un fichero que SDL_mixer sabe leer. */
static int load(const void *data, size_t size, int music) {
    int handle = sound_count + 1;
    if (music) {
        /* Mix_Music no copia el fichero: necesita su propia copia */
        void *copy = malloc(size);
        if (!copy) return 0;
        memcpy(copy, data, size);
        sounds[handle].music = Mix_LoadMUS_RW(SDL_RWFromConstMem(copy, (int)size), 1);
        if (!sounds[handle].music) {
            free(copy);
            log_msg("[sound] cannot decode it: %s\n", Mix_GetError());
            return 0;
        }
        sounds[handle].data = copy;
    } else {
        sounds[handle].effect = Mix_LoadWAV_RW(SDL_RWFromConstMem(data, (int)size), 1);
        if (!sounds[handle].effect) {
            log_msg("[sound] cannot decode it: %s\n", Mix_GetError());
            return 0;
        }
    }
    sounds[handle].channel = -1;
    sound_count = handle;
    return handle;
}

static int valid(int handle) { return handle >= 1 && handle <= sound_count; }

void backend_sound_play(int handle, int loop) {
    if (!valid(handle)) return;
    if (sounds[handle].music || sounds[handle].pending) {
        SDL_LockMutex(lock);
        wanted = handle;
        wanted_loop = loop;
        if (sounds[handle].music) Mix_PlayMusic(sounds[handle].music, loop ? -1 : 0);
        else Mix_HaltMusic(); /* la anterior no sigue mientras se espera */
        SDL_UnlockMutex(lock);
    } else {
        sounds[handle].channel = Mix_PlayChannel(-1, sounds[handle].effect, loop ? -1 : 0);
    }
}

void backend_sound_stop(int handle) {
    if (!valid(handle)) return;
    if (sounds[handle].music || sounds[handle].pending) {
        SDL_LockMutex(lock);
        if (wanted == handle) wanted = 0;
        Mix_HaltMusic();
        SDL_UnlockMutex(lock);
    } else if (backend_sound_playing(handle)) {
        Mix_HaltChannel(sounds[handle].channel);
        sounds[handle].channel = -1;
    }
}

int backend_sound_playing(int handle) {
    if (!valid(handle)) return 0;
    if (sounds[handle].pending) return wanted == handle; /* sonará en cuanto esté */
    if (sounds[handle].music) return Mix_PlayingMusic() != 0;
    int channel = sounds[handle].channel;
    /* el canal sigue siendo de este sonido solo si nadie lo ha reutilizado */
    return channel >= 0 && Mix_Playing(channel) &&
           Mix_GetChunk(channel) == sounds[handle].effect;
}

void backend_sound_volume(int music, float volume) {
    if (!audio_open()) return;
    int level = (int)(volume * MIX_MAX_VOLUME + 0.5f);
    if (level < 0) level = 0;
    if (level > MIX_MAX_VOLUME) level = MIX_MAX_VOLUME;
    if (music)
        Mix_VolumeMusic(level);
    else
        Mix_Volume(-1, level);
}
