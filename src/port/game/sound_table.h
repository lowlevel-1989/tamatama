/* port/game/sound_table.h: de qué fichero es cada id de sonido. El .so solo da
   el número; el fichero, si es música o efecto y si va en bucle los dice el
   código Java del juego, en su tabla de sonidos. Una línea por sonido:
     SOUND(id, "fichero", MUSIC o EFFECT, bucle)
   La música suena de una en una; un efecto puede sonar con otros. El .so
   cambia el volumen de los ids 0-3 con el de la música y el del resto con el
   de los efectos: el 15 es música en Java, pero suena a la vez que la de fondo
   y con el volumen de los efectos, así que aquí es un efecto. Los
   ficheros están en assets/, sin extensión: el backend reconoce el formato
   por su contenido. Un id que no esté aquí sale en el log la primera vez. Lo
   incluye solo game/sound.c, con su SOUND, así que no lleva #pragma once. */
SOUND(0, "sound_bgm/title", MUSIC, 1)
SOUND(1, "sound_bgm/gameplay", MUSIC, 1)
SOUND(2, "sound_bgm/imadokiminigame", MUSIC, 1)
SOUND(3, "sound_bgm/imadokiminigame", MUSIC, 1)
SOUND(4, "sound_se/gameend", EFFECT, 0)
SOUND(5, "sound_se/gameopen", EFFECT, 0)
SOUND(6, "sound_se/gameplay_se", EFFECT, 1)
SOUND(7, "sound_se/henshin", EFFECT, 0)
SOUND(8, "sound_se/okori", EFFECT, 0)
SOUND(9, "sound_se/reset", EFFECT, 0)
SOUND(10, "sound_se/shibou", EFFECT, 0)
SOUND(11, "sound_se/sousa", EFFECT, 0)
SOUND(12, "sound_se/tanjou", EFFECT, 0)
SOUND(13, "sound_se/wagamama_shitsuke", EFFECT, 0)
SOUND(14, "sound_se/yobidashi", EFFECT, 0)
SOUND(15, "sound_bgm/yorokobi.wav", EFFECT, 0)
