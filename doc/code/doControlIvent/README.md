# doControlIvent

`Tama2MovieMain_Implement::doControlIvent(bool)`, en `@0x38e48`: el reloj
del Tamagotchi. En el binario es una sola función muy larga; aquí está
repartida en varios ficheros para poder seguirla.

Lee la hora real y avanza el reloj del juego hasta alcanzarla, paso a paso: un
segundo por paso mientras es un huevo y un minuto para el resto. En cada paso
corren los timers de `m_para` (nombres en [../m_para.h](../m_para.h)).

Una llamada no es un paso. Mientras se juega, `run()` llama aquí en cada
vuelta, unas 16 por segundo (`run -> doMain -> controlGame ->
doControlIvent(true)`), y cada llamada da los pasos que falten: casi siempre
0, uno al pasar un minuto, y muchos si el juego estuvo cerrado. Las llamadas
del arranque, el título y el menú pasan `live = 0`: ponen el reloj al día sin
animaciones ni sonidos.

| Fichero | Qué tiene |
|---|---|
| [doControlIvent.c](doControlIvent.c) | el orden de todo: empieza por aquí |
| [clock.c](clock.c) | el reloj: la hora real, ponerlo en hora y avanzarlo un paso |
| [awake.c](awake.c) | un paso despierto: enfermar, timers, dormirse, la caca |
| [calls.c](calls.c) | las tres llamadas al jugador y las que quedan sin atender |
| [transform.c](transform.c) | la transformación del personaje |
| [sleeping.c](sleeping.c) | un paso dormido: despertarse |
| [helpers.c](helpers.c) | lo que el original repite en cada sitio |

El flujo:

```
doControlIvent
  -> readRealTime, splitRealTime          clock.c
  -> startClock: en hora la primera vez   clock.c
  -> por cada paso hasta la hora real:
       clockStep                          clock.c
       dormido   -> sleepingStep          sleeping.c
       despierto -> awakeStep             awake.c
                      -> missedCalls, ownerCalls   calls.c
                      -> transform                 transform.c
```
