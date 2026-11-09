/* backend/sdl/m4a.c: SDL_mixer no lee AAC, así que un M4A se convierte antes
   a un WAV en memoria:
     M4A -> minimp4 (third_party/minimp4) separa la pista de audio en frames
       -> fdk-aac decodifica cada frame a PCM de 16 bits
       -> se le pone delante la cabecera de un WAV
   Va en el backend SDL porque fdk-aac es una library del sistema, como
   SDL_mixer. */
/* minimp4 trae un aviso en su parte de escribir MP4, que aquí no se usa */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#define MINIMP4_IMPLEMENTATION
#include "minimp4.h"
#pragma GCC diagnostic pop
#include <fdk-aac/aacdecoder_lib.h>
#include "common.h"
#include "m4a.h"

int m4a_detect(const uint8_t *data, size_t size) {
    return size > 8 && !memcmp(data + 4, "ftyp", 4);
}

typedef struct {
    const uint8_t *data;
    size_t size;
} Source;

/* minimp4 lee por aquí: 0 si pudo, otro valor si no. */
static int read_at(int64_t offset, void *buffer, size_t size, void *token) {
    const Source *source = token;
    if (offset < 0 || (uint64_t)offset + size > source->size) return 1;
    memcpy(buffer, source->data + offset, size);
    return 0;
}

/* Escribe un entero little endian de n bytes. */
static void put_le(uint8_t *p, uint32_t value, int n) {
    for (int i = 0; i < n; i++) p[i] = (uint8_t)(value >> (8 * i));
}

uint8_t *m4a_to_wav(const uint8_t *data, size_t size, size_t *wav_size) {
    Source source = {data, size};
    MP4D_demux_t mp4;
    if (!MP4D_open(&mp4, read_at, &source, (int64_t)size)) return NULL;
    /* la primera pista de audio AAC */
    int track = -1;
    for (unsigned i = 0; i < mp4.track_count && track < 0; i++)
        if (mp4.track[i].handler_type == MP4D_HANDLER_TYPE_SOUN &&
            mp4.track[i].object_type_indication == MP4_OBJECT_TYPE_AUDIO_ISO_IEC_14496_3)
            track = (int)i;
    HANDLE_AACDECODER decoder = track < 0 ? NULL : aacDecoder_Open(TT_MP4_RAW, 1);
    uint8_t *wav = NULL;
    size_t used = 44, room = 0; /* los 44 primeros bytes son la cabecera */
    int rate = 0, channels = 0;
    if (decoder) {
        MP4D_track_t *t = &mp4.track[track];
        UCHAR *config[1] = {t->dsi};
        UINT config_bytes[1] = {t->dsi_bytes};
        if (aacDecoder_ConfigRaw(decoder, config, config_bytes) != AAC_DEC_OK) t = NULL;
        for (unsigned n = 0; t && n < t->sample_count; n++) {
            unsigned frame_bytes, timestamp, duration;
            MP4D_file_offset_t offset =
                MP4D_frame_offset(&mp4, (unsigned)track, n, &frame_bytes, &timestamp, &duration);
            if (!frame_bytes || offset + frame_bytes > size) break;
            UCHAR *in = (UCHAR *)data + offset;
            UINT in_bytes = frame_bytes, left = frame_bytes;
            if (aacDecoder_Fill(decoder, &in, &in_bytes, &left) != AAC_DEC_OK) break;
            static INT_PCM pcm[8 * 2048]; /* el frame más grande: 8 canales */
            AAC_DECODER_ERROR error = aacDecoder_DecodeFrame(decoder, pcm, 8 * 2048, 0);
            if (error == AAC_DEC_NOT_ENOUGH_BITS) continue;
            if (error != AAC_DEC_OK) break;
            CStreamInfo *info = aacDecoder_GetStreamInfo(decoder);
            rate = info->sampleRate;
            channels = info->numChannels;
            size_t bytes = (size_t)info->frameSize * (size_t)channels * sizeof(INT_PCM);
            if (used + bytes > room) {
                room = (used + bytes) * 2;
                uint8_t *bigger = realloc(wav, room);
                if (!bigger) break;
                wav = bigger;
            }
            memcpy(wav + used, pcm, bytes);
            used += bytes;
        }
        aacDecoder_Close(decoder);
    }
    MP4D_close(&mp4);
    if (!wav || !rate || !channels) {
        free(wav);
        return NULL;
    }
    /* la cabecera de un WAV PCM de 16 bits */
    uint32_t pcm_bytes = (uint32_t)(used - 44);
    memcpy(wav, "RIFF", 4);
    put_le(wav + 4, 36 + pcm_bytes, 4);
    memcpy(wav + 8, "WAVEfmt ", 8);
    put_le(wav + 16, 16, 4);
    put_le(wav + 20, 1, 2); /* PCM */
    put_le(wav + 22, (uint32_t)channels, 2);
    put_le(wav + 24, (uint32_t)rate, 4);
    put_le(wav + 28, (uint32_t)(rate * channels * 2), 4);
    put_le(wav + 32, (uint32_t)(channels * 2), 2);
    put_le(wav + 34, 16, 2);
    memcpy(wav + 36, "data", 4);
    put_le(wav + 40, pcm_bytes, 4);
    *wav_size = used;
    return wav;
}
