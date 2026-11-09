/* port/game/sound.h: los sonidos del juego, por su id. */
#pragma once

/* Lo hace sonar; lo carga la primera vez. */
void sound_play(int id);
void sound_stop(int id);
/* 1 mientras suena */
int sound_playing(int id);
/* el volumen de la música o de los efectos, de 0 a 1 */
void sound_volume(int music, float volume);
