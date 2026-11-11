# Tamagotchi P1: documentación de la primera generación (rev_2)

[English](tutorial_p1_rev_2.md) | Español

Sacado del `.so` de Tamagotchi Classic (Android). Cada apartado trae un
trozo de pseudocódigo simplificado y el fichero de [doc/code](code) de donde
sale, con la dirección de cada función en el `.so`.

Los nombres en mayúsculas (`HUNGER_06`, `SICK_TIMER_0d`...) son los datos de
la mascota, `m_para`: una lista de números que el juego guarda en la
partida. Qué es cada uno: [doc/code/m_para.h](code/m_para.h). Los timers
cuentan minutos (el huevo, segundos) y `-1` quiere decir parado.

## Los personajes

[setParaTimer.c](code/setParaTimer.c),
[InitCharacterData.c](code/InitCharacterData.c)

Cada personaje tiene una ficha en `DATA_PARAMETER` (9 números) y su horario
en `DT_SLEEP_TIME`. Estos son los valores tal cual están en el juego:

| id | Nombre | Peso mín. | Evolución | Hambre / felicidad | Enferm. scale | Enferm. div | Inyecciones | Sin uso | Disciplina inicial | Timer disciplina | Despierta | Duerme |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 0 | Huevo | 1 | 2 | -1 | -1 | -1 | 0 | 0 | 0 | -1 | -1 | -1 |
| 1 | Babytchi | 5 | 60 | 7 | 60 | 1 | 2 | 0 | 0 | -1 | -1 | -1 |
| 2 | Marutchi | 10 | 2 | 55 | 2 | 1 | 2 | 100 | 0 | 190 | 9 | 20 |
| 3 | Tamatchi | 20 | 3 | 80 | 3 | 1 | 2 | 50 | 2 | 480 | 9 | 21 |
| 4 | Tamatchi | 20 | 3 | 80 | 3 | 1 | 2 | 100 | 0 | 206 | 9 | 21 |
| 5 | Kuchitamatchi | 20 | 2 | 80 | 2 | 2 | 2 | 50 | 2 | 480 | 9 | 21 |
| 6 | Kuchitamatchi | 20 | 2 | 80 | 2 | 2 | 2 | 100 | 0 | 206 | 9 | 21 |
| 7 | Mametchi | 30 | 6 | 86 | 6 | 1 | 1 | 0 | 4 | -1 | 9 | 22 |
| 8 | Ginjirotchi | 30 | 4 | 86 | 4 | 1 | 1 | 50 | 2 | 520 | 9 | 22 |
| 9 | Maskutchi | 30 | 4 | 60 | 4 | 1 | 1 | 100 | 0 | 206 | 11 | 23 |
| 10 | Kuchipatchi | 20 | 2 | 65 | 2 | 1 | 2 | 0 | 4 | -1 | 9 | 22 |
| 11 | Nyorotchi | 10 | 1 | 65 | 1 | 2 | 3 | 50 | 2 | 206 | 9 | 22 |
| 12 | Tarakotchi | 20 | 2 | 48 | 2 | 2 | 2 | 100 | 0 | 520 | 10 | 22 |
| 13 | Bill (EE. UU.) / Oyajitchi (Japón) | 30 | 6 | 86 | 6 | 1 | 1 | 0 | 4 | -1 | 9 | 22 |

Qué es cada campo:

- **Peso mín.**: el peso nunca baja de aquí.
- **Evolución**: el huevo, en segundos; Babytchi, en minutos; del niño en
  adelante, en días.
- **Hambre / felicidad**: cada cuántos minutos pierde un corazón de hambre, y
  cada cuántos uno de felicidad (el mismo número).
- **Enferm. scale** y **div**: para calcular cuándo enferma (ver "Cuándo
  enferma").
- **Inyecciones**: dosis de medicina para curarse.
- **Sin uso**: 0, 50 o 100. El juego no lo lee nunca: ninguna de las
  funciones que usan la ficha toca este campo (el byte `0x18` de la fila).
  Sus valores son el inverso de la disciplina inicial (`100 - disciplina *
  25`: 100 con 0, 50 con 2, 0 con 4), así que seguramente era un porcentaje
  ligado a la disciplina en el Tamagotchi original.
- **Disciplina inicial**: el nivel de disciplina con el que empieza la etapa,
  de 0 a 4. Un 2 se queda en 2 o sube a 3 al azar:

  ```
  DISCIPLINE_09 = disciplina inicial
  si es 2: DISCIPLINE_09 = (azar() <= 0.5) ? 3 : 2    // 3 con un 51 %
  ```
- **Timer disciplina**: cada cuánto se porta mal; el valor real va entre la
  mitad y el total (ver "Cuándo se porta mal"). `-1`: nunca.
- **Despierta** y **Duerme**: la hora del teléfono, de 0 a 23.

Además:

- Los dos Tamatchi (3 y 4) y los dos Kuchitamatchi (5 y 6) solo se
  diferencian en la disciplina: los de id impar empiezan con 2 o 3 y se portan
  mal cada 240–480 min; los de id par empiezan con 0 y se portan mal cada
  103–206 min.
- Bill y Oyajitchi son el mismo personaje (id 13): uno en la versión de
  EE. UU. y otro en la de Japón.

## El tiempo

[doControlIvent/clock.c](code/doControlIvent/clock.c)

Mientras se juega, el juego mira la hora unas 16 veces por segundo y avanza
el reloj de la mascota a pasos: un paso por minuto (el huevo, uno por
segundo). En cada paso bajan todos sus timers.

```
mientras reloj_mascota < hora_real:
    reloj_mascota += 1 minuto          // el huevo: 1 segundo
    cada timer en marcha: timer -= 1   // al llegar a 0, pasa lo suyo
```

- Con el juego cerrado el tiempo sigue contando: al abrirlo da de golpe
  todos los pasos que faltan, sin animaciones ni sonidos. Si estuvo 3 horas
  cerrado, son 180 pasos seguidos.
- El horario de despertar y dormir usa la hora del reloj del teléfono, con
  su zona horaria.

## El huevo y Babytchi

Son los personajes de tutorial: siguen un patrón fijo.

**El huevo** eclosiona 2 segundos después de empezar a jugar: mientras se
está en el título, su contador vuelve a 2
([doControlIvent/clock.c](code/doControlIvent/clock.c)).

En Tamagotchi L.i.f.e. v1.0 y v1.01 tarda 2 minutos: el contador es el mismo
(`TRANSFORM_0e`, que empieza en 2), pero el huevo no cuenta en segundos sino
en minutos, como los demás personajes. Baja uno con cada cambio de minuto
del reloj, así que nace entre 1 y 2 minutos después de empezar a jugar
(medido: entre 94 y 125 s). Desde L.i.f.e. v1.02 vuelven a ser 2 segundos.

**Babytchi** ([InitCharacterData.c](code/InitCharacterData.c)):

```
al nacer:
    HUNGER_TIMER_0a = 0           // pierde un corazón de hambre enseguida
    HAPPY_TIMER_0b  = 2           // y uno de felicidad a los 2 minutos
    POOP_TIMER_0c   = 15          // una caca a los 15 minutos
    SICK_TIMER_0d   = 45          // enferma una vez, a los 45 minutos
    TRANSFORM_0e    = 60          // evoluciona a los 60 minutos
```

Después pierde un corazón cada 7 minutos. Además:

- Sus llamadas se cierran solas a los 2 minutos (las de hambre, felicidad y
  disciplina) o a los 5 (la de la luz), pero **no cuentan como fallo**, así
  que no influyen en su evolución
  ([setMissCnt.c](code/setMissCnt.c)):

  ```
  si se cierra una llamada sin atender y el personaje no es Babytchi:
      un fallo más
  ```
- Duerme la siesta entre los minutos 40 y 44 de su etapa, y al despertar
  hace una caca
  ([doControlIvent/awake.c](code/doControlIvent/awake.c),
  [doControlIvent/sleeping.c](code/doControlIvent/sleeping.c)).
- Comer y jugar no le cambian el peso, y los dulces no le cuentan
  ([setCare.c](code/setCare.c)).
- Enferma una sola vez: después, su timer de enfermedad se queda parado
  ([setParaTimer.c](code/setParaTimer.c)). Los timers solo corren despierto,
  así que con la siesta enferma hacia el minuto 50.
- No muere ([doControlIvent/awake.c](code/doControlIvent/awake.c)).

Lo que sigue vale a partir del niño.

## Lo que se pone a 0 al evolucionar

[InitCharacterData.c](code/InitCharacterData.c)

```
SICK_COUNT_12      = 0    // veces que ha enfermado
CANDY_22           = 0    // contador de dulces
SNACKS_1f          = 0    // dulces hacia ese contador
CARE_MISS_18       = 0    // fallos de cuidado
DISCIPLINE_MISS_19 = 0    // fallos de disciplina
STAGE_DAYS_21      = 0    // días en la etapa
NO_FOOD_COUNT_13   = 0    // veces sin comer (adultos)
STAGE_MINUTES_1e   = 0    // minutos en la etapa
plazos de las llamadas = -1
```

## Las llamadas

[doControlIvent/calls.c](code/doControlIvent/calls.c),
[setMissCnt.c](code/setMissCnt.c)

| Llamada | Cuándo | Cómo atenderla | Plazo | Si no se atiende |
|---|---|---|---|---|
| Hambre | `HUNGER_06` llega a 0 | una comida | 15 min | fallo de cuidado |
| Felicidad | `HAPPY_07` llega a 0 | un dulce o ganar el minijuego | 15 min | fallo de cuidado |
| Luz | se duerme | apagar la luz | 60 min | fallo de cuidado |
| Disciplina | vence `DISCIPLINE_TIMER_0f` | regañarle | 30 min | fallo de disciplina |

```
al vencer el plazo de una llamada:
    se cierra la llamada
    un fallo más (de cuidado o de disciplina)
```

Una excepción, una sola vez en toda la partida: la primera vez que se
cierra una llamada de hambre, se haya atendido o no, se cierra también la de
felicidad si estaba abierta, sin contar fallo por ella.

```
al cerrarse una llamada de hambre:
    si todavía no ha pasado nunca (falta la flag 0x400):
        se cierra también la llamada de felicidad, sin fallo
        flag 0x400                      // ya no vuelve a pasar
```

La flag `0x400` no se borra al evolucionar, solo al empezar una partida
nueva ([InitCharacterData.c](code/InitCharacterData.c)).

- Pasar el plazo con el hambre a 0 solo cuenta el fallo: no hace enfermar.
- Al dormirse, las llamadas de hambre, felicidad y disciplina abiertas se
  cierran y cuentan como fallo
  ([doControlIvent/awake.c](code/doControlIvent/awake.c)).

## Dar de comer

[setCare.c](code/setCare.c)

```
comida:
    si se está portando mal, está enfermo o HUNGER_06 == 4: la rechaza
    HUNGER_06 += 1
    WEIGHT_02 += 1                  // hasta 99
    el timer del hambre vuelve a empezar
    STARVE_TIMER_10 = -1            // ya no muere de hambre
    cierra la llamada de hambre

dulce:                              // nunca lo rechaza
    HAPPY_07 += 1                   // hasta 4
    SNACKS_1f += 1
    si SNACKS_1f == 3: SNACKS_1f = 0, CANDY_22 += 1
    WEIGHT_02 += 2                  // hasta 99
    el timer de la felicidad vuelve a empezar
    cierra la llamada de felicidad
```

## Cuándo pierde un corazón

[setParaTimer.c](code/setParaTimer.c),
[doControlIvent/calls.c](code/doControlIvent/calls.c)

```
HUNGER_TIMER_0a = HAPPY_TIMER_0b = "hambre / felicidad" de su ficha
al llegar a 0:
    HUNGER_06 -= 1  (o HAPPY_07 -= 1)
    si llega a 0: llamada de hambre (o de felicidad)
    el timer vuelve a empezar
```

Al quedarse sin corazones de hambre empieza `STARVE_TIMER_10`: 12 horas
para morir de hambre.

## Jugar

[setCarePlayResult.c](code/setCarePlayResult.c)

```
ganar:
    WEIGHT_02 -= 1                  // sin bajar del peso mínimo
    HAPPY_07 += 1                   // hasta 4
    el timer de la felicidad vuelve a empezar
    cierra la llamada de felicidad
perder:
    WEIGHT_02 -= 1                  // sin bajar del peso mínimo
```

## La luz

[setCare.c](code/setCare.c),
[doControlIvent/sleeping.c](code/doControlIvent/sleeping.c)

```
al despertar: luz encendida
apagar la luz con la llamada de la luz abierta: la llamada queda atendida
```

## Medicina

[setCare.c](code/setCare.c)

```
si está enfermo:
    MEDICINE_05 -= 1
    si llega a 0: se cura, y SICK_DEATH_TIMER_11 = -1
si no: la rechaza
```

Al enfermar, `MEDICINE_05` vale las "inyecciones" de su ficha.

## Cuándo enferma

[setParaTimer.c](code/setParaTimer.c),
[setUntchAdd.c](code/setUntchAdd.c),
[doControlIvent/sleeping.c](code/doControlIvent/sleeping.c),
[doControlIvent/helpers.c](code/doControlIvent/helpers.c)

Por tres motivos.

**1. Con 8 cacas en pantalla** ([setUntchAdd.c](code/setUntchAdd.c)).

**2. Por tiempo**: cuando vence `SICK_TIMER_0d`, enferma y el timer vuelve a
empezar ([setParaTimer.c](code/setParaTimer.c)):

```
minutos_despierto = (duerme - despierta) * 60      // despierto en un día
base              = minutos_despierto * scale / div
SICK_TIMER_0d     = base - base/4 * azar() - base/4 * azar()
```

### Cómo elegir scale y div

**`scale` son los días y `div`, en cuántas partes se divide ese tiempo.** El
resultado, `scale ÷ div`, es cada cuántos días enferma, como mucho; el azar lo
acorta hasta la mitad.

| scale | div | scale ÷ div | Con 12 h despierto al día | Enferma |
|---|---|---|---|---|
| 6 | 1 | 6 días | cada 36 a 72 h | cada 3 a 6 días |
| 8 | 2 | 4 días | cada 24 a 48 h | cada 2 a 4 días |
| 4 | 2 | 2 días | cada 12 a 24 h | cada 1 a 2 días |
| 6 | 3 | 2 días | cada 12 a 24 h | cada 1 a 2 días |
| 2 | 2 | 1 día | cada 6 a 12 h | 1 o 2 veces al día |
| 1 | 2 | 1 día en 2 partes | cada 3 a 6 h | 2 a 4 veces al día |
| 1 | 3 | 1 día en 3 partes | cada 2 a 4 h | 3 a 6 veces al día |

- Los días cuentan solo las horas despierto, porque el timer se para de
  noche.

Con los valores de cada personaje, en horas despierto (`horas despierto al
día × scale ÷ div`):

| Personaje | Despierto al día | scale | div | Cálculo | Enferma cada |
|---|---|---|---|---|---|
| Marutchi | 11 h | 2 | 1 | 11 × 2 ÷ 1 = 22 h | entre 11 h y 22 h |
| Tamatchi (3 y 4) | 12 h | 3 | 1 | 12 × 3 ÷ 1 = 36 h | entre 18 h y 36 h |
| Kuchitamatchi (5 y 6) | 12 h | 2 | 2 | 12 × 2 ÷ 2 = 12 h | entre 6 h y 12 h |
| Mametchi | 13 h | 6 | 1 | 13 × 6 ÷ 1 = 78 h | entre 39 h y 78 h |
| Ginjirotchi | 13 h | 4 | 1 | 13 × 4 ÷ 1 = 52 h | entre 26 h y 52 h |
| Maskutchi | 12 h | 4 | 1 | 12 × 4 ÷ 1 = 48 h | entre 24 h y 48 h |
| Kuchipatchi | 13 h | 2 | 1 | 13 × 2 ÷ 1 = 26 h | entre 13 h y 26 h |
| Nyorotchi | 13 h | 1 | 2 | 13 × 1 ÷ 2 = 6 h 30 min | entre 3 h 15 min y 6 h 30 min |
| Tarakotchi | 12 h | 2 | 2 | 12 × 2 ÷ 2 = 12 h | entre 6 h y 12 h |
| Bill / Oyajitchi | 13 h | 6 | 1 | 13 × 6 ÷ 1 = 78 h | entre 39 h y 78 h |

El huevo y Babytchi no lo tienen (Babytchi enferma una sola vez, ver "El huevo
y Babytchi").

**3. Por los dulces**, al despertar
([doControlIvent/sleeping.c](code/doControlIvent/sleeping.c)):

```
si azar() * 100 < CANDY_22 * 10:
    CANDY_SICK_23 = un minuto al azar de las horas despierto,
                    a más de 60 minutos de SICK_TIMER_0d
CANDY_22 = 0
```

- `CANDY_22` sube 1 cada 3 dulces ([setCare.c](code/setCare.c)), y cada
  punto es un 10 % de probabilidad: 3 dulces, un 10 %; 30 dulces, un 100 %.
- Se mira una vez al día, al despertar, y el contador vuelve a 0 salga lo que
  salga: los dulces de un día solo cuentan para el día siguiente.
- Si toca, el juego elige a qué hora del día enfermará: un minuto al azar,
  contado desde que despierta. Si ese minuto cae a menos de 60 minutos del
  momento en que vence `SICK_TIMER_0d` (el motivo 2), antes o después, elige
  otro. Así nunca enferma dos veces con menos de una hora de diferencia.

  Por ejemplo, Marutchi despierta a las 9:00 y su `SICK_TIMER_0d` vence a las
  14:00. Toca enfermar por los dulces:

  ```
  sale el minuto 270, las 13:30   -> a 30 min de las 14:00: no vale
  sale el minuto 500, las 17:20   -> a más de 60 min: vale
  ```

  Ese día enferma dos veces: a las 14:00 por el timer y a las 17:20 por los
  dulces.

Al enfermar:

```
MEDICINE_05         = inyecciones de su ficha
SICK_COUNT_12      += 1                 // con 3, muere
SICK_DEATH_TIMER_11 = 360               // 6 horas para curarse
```

## Cuándo se porta mal

[setParaTimer.c](code/setParaTimer.c),
[doControlIvent/calls.c](code/doControlIvent/calls.c)

Cada cierto tiempo, la mascota se porta mal: llama al jugador sin necesitar
nada. Ese tiempo sale del "timer de disciplina" de su ficha:

```
t = timer de disciplina de su ficha
DISCIPLINE_TIMER_0f = t - t/2 * azar()    // azar() de 0 a 1
```

Es un número al azar entre la mitad de `t` y `t`. Qué quiere decir:

- `t` es lo máximo que puede pasar sin portarse mal.
- El azar hace que no se porte mal siempre a la misma hora: así no se puede
  saber cuándo va a pasar.
- Pero nunca antes de la mitad de `t`: así no se porta mal dos veces casi
  seguidas.

Para crear una ficha: cuanto más pequeño `t`, más a menudo se porta mal.

Por ejemplo, Tamatchi (3) tiene `t = 480` minutos despierto:

```
azar() = 0    ->  480 - 240 * 0    = 480 minutos  (8 horas)
azar() = 0,5  ->  480 - 240 * 0,5  = 360 minutos  (6 horas)
azar() = 1    ->  480 - 240 * 1    = 240 minutos  (4 horas)
```

Nunca pasa más de 8 horas sin portarse mal, ni menos de 4.

Cuando el timer llega a 0:

1. Llama al jugador: es la llamada de disciplina.
2. El timer vuelve a empezar con otro valor al azar, se atienda o no.
3. Hay 30 minutos para regañarle. Si no, cuenta un fallo de disciplina.
4. Mientras la llamada siga abierta, rechaza la comida.

Los personajes con `t = -1` en su ficha (el huevo, Babytchi, Mametchi,
Kuchipatchi y Bill / Oyajitchi) no se portan mal nunca.

## Regañar

[setCare.c](code/setCare.c)

```
si se está portando mal:
    cierra la llamada de disciplina
    DISCIPLINE_09 += 1                  // hasta 4
si no: no pasa nada
```

## Cuándo hace caca

[doControlIvent/awake.c](code/doControlIvent/awake.c),
[doControlIvent/sleeping.c](code/doControlIvent/sleeping.c),
[InitCharacterData.c](code/InitCharacterData.c),
[setUntchAdd.c](code/setUntchAdd.c),
[setCare.c](code/setCare.c)

La mascota va dejando cacas en la pantalla, y se quedan ahí hasta que se
limpian. El juego las cuenta en `POOP_08`. Cuándo hace caca depende de la
etapa.

### El huevo y Babytchi

El huevo no hace caca. Babytchi hace dos, en momentos fijos, para enseñar a
limpiar:

1. A los 15 minutos de nacer.
2. Al despertar de la siesta, en el minuto 45.

```
al nacer:                   POOP_TIMER_0c = 15
a los 15 minutos:           caca, y el timer se para (-1)
al despertar de la siesta:  caca
```

Después ya no hace más hasta que evoluciona.

### Del niño en adelante

Hace caca **cada 3 horas despierto**, siempre exactas: aquí no hay azar.

```
al llegar a la etapa:  POOP_TIMER_0c = 180       // minutos: 3 horas
al llegar a 0:         caca, y el timer vuelve a 180
```

El timer empieza a contar al evolucionar, no a una hora fija del día. Y como
de noche se para, la hora de la caca cambia de un día a otro. Por ejemplo, un
Marutchi (despierto de 9:00 a 20:00) que evolucionó a las 10:00:

```
10:00  evoluciona: el timer empieza en 180
13:00  caca
16:00  caca
19:00  caca            quedan 180 minutos para la siguiente
20:00  se duerme       el timer se para con 120 minutos
 9:00  despierta       sigue con 120 minutos
11:00  caca            y así, cada 3 horas despierto
```

### Limpiar

Limpiar quita todas las cacas de golpe (`POOP_08 = 0`), pero no cambia el
timer: la siguiente llega a su hora igual.

Si se acumulan **8 cacas sin limpiar, enferma** (ver "Cuándo enferma").

## Causas de muerte

[doControlIvent/awake.c](code/doControlIvent/awake.c),
[setDay.c](code/setDay.c)

```
STARVE_TIMER_10 llega a 0      // 12 horas sin corazones de hambre
SICK_DEATH_TIMER_11 llega a 0  // 6 horas enfermo sin curarse
SICK_COUNT_12 > 2              // 3 enfermedades en la misma etapa
NO_FOOD_COUNT_13 > 4           // adultos: 5 veces sin comer
```

`NO_FOOD_COUNT_13` solo sube con los adultos (id 7 en adelante) a los que ya
no les quedan días para evolucionar, cada vez que se quedan sin corazones de
hambre ([doControlIvent/calls.c](code/doControlIvent/calls.c)).

Los dulces no matan directamente: hacen enfermar, y la enfermedad sí puede
matar.

## Dormir y despertar

[doControlIvent/awake.c](code/doControlIvent/awake.c),
[doControlIvent/sleeping.c](code/doControlIvent/sleeping.c),
[setCare.c](code/setCare.c)

Cada personaje tiene un horario fijo: la hora a la que despierta y la hora
a la que se duerme, según el reloj del teléfono. Mientras duerme, el juego
casi se detiene para él.

### El horario

[doControlIvent/helpers.c](code/doControlIvent/helpers.c)

No está en la ficha (`DATA_PARAMETER`), sino en otra tabla aparte,
`DT_SLEEP_TIME` (`@0xa3a88`). Tiene una fila por personaje, en el mismo
orden de id, con dos números: la hora de despertar y la de dormir, en horas
enteras de 0 a 23. Son las columnas **Despierta** y **Duerme** de la tabla de
"Los personajes".

No hay fórmula ni azar: son valores fijos, iguales todos los días. Solo
cambian al evolucionar, porque cada personaje tiene su fila. El huevo y
Babytchi tienen `-1`: no tienen horario.

Para saber si está despierto, el juego compara la hora del teléfono
(`DAY_HOUR_1d`, solo la hora, sin minutos) con su fila:

```
despierto = despierta <= hora < duerme
```

Por ejemplo, Marutchi tiene `9` y `20`:

```
hora 8   ->  8 < 9          dormido
hora 9   ->  9 <= 9 < 20    despierto
hora 19  ->  19 < 20        despierto
hora 20  ->  20 < 20 no     dormido
```

Así que está despierto de 9:00 a 19:59 (11 horas al día) y se duerme
justo a las 20:00. Las horas despierto al día, `duerme - despierta`, son las
que usan los cálculos de enfermar y evolucionar.

### El huevo y Babytchi

El huevo no duerme. Babytchi no tiene horario: echa una siesta corta, entre
los minutos 40 y 44 de su etapa, para enseñar a apagar la luz. Tiene 5
minutos para hacerlo, y al despertar hace una caca.

### Cuándo se duerme

Al llegar su hora de dormir. Por ejemplo, Marutchi duerme de 20:00 a 9:00.

En ese momento:

1. Las llamadas de hambre, felicidad y disciplina que siguieran abiertas se
   cierran y cuentan como fallo: hay que atenderlas antes de que se duerma.
2. Llama para que se apague la luz. Hay 60 minutos para hacerlo; si no,
   cuenta un fallo de cuidado.

```
al llegar la hora de dormir:
    llamadas abiertas -> se cierran y cuentan como fallo
    LIGHT_CALL_17 = 60                  // minutos para apagar la luz
```

### Mientras duerme

Todos sus timers se paran: no pierde corazones, no hace caca, no enferma,
no se porta mal y no avanza hacia la evolución. Lo único que corre es el plazo
para apagar la luz. Por eso todos los tiempos de este documento son "horas
despierto".

### Cuándo despierta

Al llegar su hora de despertar. Entonces empieza un día nuevo:

```
al llegar la hora de despertar:
    se enciende la luz
    AGE_01 += 1                         // un día más de edad, hasta 99
    STAGE_DAYS_21 += 1                  // un día más en esta etapa
    EVOL_DAYS_20 -= 1                   // un día menos para evolucionar;
                                        // con 0, evoluciona hoy
    se mira si enferma hoy por los dulces (ver "Cuándo enferma")
```

### Ejemplo

Un Marutchi recién evolucionado, con 2 días para evolucionar:

```
día 1   9:00   despierta: edad +1, días en la etapa 1, falta 1 para evolucionar
        20:00  se duerme: llamadas abiertas = fallos; 60 min para apagar la luz
día 2   9:00   despierta: edad +1, días en la etapa 2, faltan 0: evoluciona hoy
```
## Cuándo evoluciona

[doControlIvent/sleeping.c](code/doControlIvent/sleeping.c),
[doControlIvent/transform.c](code/doControlIvent/transform.c)

Al despertar el día que se acaban sus días de evolución:

```
TRANSFORM_0e = 120 + azar() * ((duerme - despierta) * 60 - 121)
```

Es decir, entre 2 horas después de despertar y un minuto antes de dormir.
Al llegar a 0, el árbol decide en qué se convierte.

## Árbol de evolución

[setTransform.c](code/setTransform.c)

"Cuidado" son los fallos de cuidado (`CARE_MISS_18`) y "disciplina", los
fallos de disciplina (`DISCIPLINE_MISS_19`) de la etapa que termina.

| Desde | Condición | Hacia |
|---|---|---|
| Huevo (0) | — | Babytchi (1) |
| Babytchi (1) | — | Marutchi (2) |
| Marutchi (2) | cuidado < 3, disciplina < 2 | Tamatchi (3) |
| Marutchi (2) | cuidado < 3, disciplina > 1 | Tamatchi (4) |
| Marutchi (2) | cuidado > 2, disciplina < 2 | Kuchitamatchi (5) |
| Marutchi (2) | cuidado > 2, disciplina > 1 | Kuchitamatchi (6) |
| Tamatchi (3) | cuidado < 3, disciplina 0 | Mametchi (7) |
| Tamatchi (3) | cuidado < 3, disciplina 1 | Ginjirotchi (8) |
| Tamatchi (3) | cuidado < 3, disciplina > 1 | Maskutchi (9) |
| Tamatchi (3) | cuidado > 2, disciplina 0 o 1 | Kuchipatchi (10) |
| Tamatchi (3) | cuidado > 2, disciplina 2 o 3 | Nyorotchi (11) |
| Tamatchi (3) | cuidado > 2, disciplina > 3 | Tarakotchi (12) |
| Tamatchi (4) | cuidado < 4, disciplina < 2 | Ginjirotchi (8) |
| Tamatchi (4) | cuidado < 4, disciplina > 1 | Maskutchi (9) |
| Tamatchi (4) | cuidado > 3, disciplina < 8 | Nyorotchi (11) |
| Tamatchi (4) | cuidado > 3, disciplina > 7 | Tarakotchi (12) |
| Kuchitamatchi (5) | cuidado < 3, disciplina 0 o 1 | Kuchipatchi (10) |
| Kuchitamatchi (5) | cuidado < 3, disciplina 2 | Nyorotchi (11) |
| Kuchitamatchi (5) | cuidado < 3, disciplina > 2 | Tarakotchi (12) |
| Kuchitamatchi (5) | cuidado > 2, disciplina < 4 | Nyorotchi (11) |
| Kuchitamatchi (5) | cuidado > 2, disciplina > 3 | Tarakotchi (12) |
| Kuchitamatchi (6) | cuidado < 3, disciplina 0 | Kuchipatchi (10) |
| Kuchitamatchi (6) | cuidado < 3, disciplina 1 | Nyorotchi (11) |
| Kuchitamatchi (6) | cuidado < 3, disciplina > 1 | Tarakotchi (12) |
| Kuchitamatchi (6) | cuidado > 2, disciplina < 6 | Nyorotchi (11) |
| Kuchitamatchi (6) | cuidado > 2, disciplina > 5 | Tarakotchi (12) |
| Maskutchi (9) | cuidado < 6, disciplina 0, 7 días o más en la etapa | Bill / Oyajitchi (13) |

Los demás adultos no cambian.

## Los huecos de m_para

[doControlIvent/sleeping.c](code/doControlIvent/sleeping.c),
[getters.c](code/getters.c), [m_para.h](code/m_para.h)

### La flag 0x8: la muerte con otra animación

La ponen Mametchi, Ginjirotchi y Maskutchi (7, 8 y 9) el día que se acaban
sus días de evolución. Ninguno de los dos primeros evoluciona, así que la
llevan desde ese día hasta el final. Maskutchi la pierde si se convierte en
Bill / Oyajitchi, porque evolucionar la borra.

Solo se lee en un sitio, `controlGame` (`@0x3a784`), en la pantalla de la
muerte. `setDay` (`@0x38160`) muestra la muerte con la animación `0x12` y
apunta la hora en `m_dayTimer`. Pasado un minuto:

```
si la animación sigue siendo la 0x12 (la muerte):
    con la flag 0x8:  pasa a la animación 0x13
    sin ella:         espera medio segundo y va a la sección 9
si no (ya es la 0x13):
    va a la sección 9
```

Es decir, estos tres personajes tienen una animación más al morir, después
de la de siempre. Qué se ve en la `0x13` y qué es la sección 9 aún no se ha
comprobado en pantalla.

