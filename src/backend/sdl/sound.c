/* backend/sdl/sound.c: el sonido con SDL_mixer. Los efectos son Mix_Chunk, que
   pueden sonar varios a la vez, cada uno en su canal; la música es Mix_Music,
   y solo suena una. El audio se abre la primera vez que se carga un sonido:
   si no se puede, el juego sigue sin sonido. Un M4A, que SDL_mixer no lee,
   pasa antes a WAV (m4a.c). */
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
} sounds[MAX_SOUNDS + 1]; /* el 0 no se usa: es "no cargado" */
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
    audio_state = 1;
    return 1;
}

int backend_sound_load(const void *data, size_t size, int music) {
    if (!audio_open() || sound_count == MAX_SOUNDS) return 0;
    uint8_t *wav = NULL;
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
    if (sounds[handle].music)
        Mix_PlayMusic(sounds[handle].music, loop ? -1 : 0);
    else
        sounds[handle].channel = Mix_PlayChannel(-1, sounds[handle].effect, loop ? -1 : 0);
}

void backend_sound_stop(int handle) {
    if (!valid(handle)) return;
    if (sounds[handle].music) {
        Mix_HaltMusic();
    } else if (backend_sound_playing(handle)) {
        Mix_HaltChannel(sounds[handle].channel);
        sounds[handle].channel = -1;
    }
}

int backend_sound_playing(int handle) {
    if (!valid(handle)) return 0;
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
