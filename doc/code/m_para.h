// m_para: los datos del personaje, un array de int en
// @0xc1ba8  Tama2MovieMain_Implement::m_para
//
// Cada nombre lleva detrás su índice en hex: m_para[TRANSFORM_0e] es
// m_para[0xe]. Así se lee rápido y la posición sigue a la vista para buscarla
// en el binario. Las posiciones sin nombre se escriben en hex: m_para[0x13].
//
// El offset es el índice * 4, el que va en las reglas: watch ... m_para+0x38.
//
// En la partida guardada: initCharacterData y doControlIvent copian
// m_para[0x0] a m_para[0x2f] en m_recData[6] a m_recData[53], y saveRecord
// lo escribe en gamestatus_0.dat, en big endian. Ahí m_para[i] está en
//   0x118 + i * 4      (TRANSFORM_0e en 0x150; doc/code/saveRecord.c)
//
//  idx   offset  nombre                 qué es
//  0x00  +0x00   ID_00                  id del personaje: 0 huevo, 1 Babytchi
//  0x01  +0x04   AGE_01                 edad en días: +1 al despertar, hasta
//                                       99
//  0x02  +0x08   WEIGHT_02              peso: como mínimo
//                                       DATA_PARAMETER[id][0x0]
//  0x03  +0x0c   -                      sin uso: solo va en las copias
//                                       enteras (partida, FutureScanner)
//  0x04  +0x10   FLAGS_04               flags: ver los FLAG_ de abajo
//  0x05  +0x14   MEDICINE_05            dosis de medicina que faltan: al
//                                       enfermar, DATA_PARAMETER[id][0x14]
//  0x06  +0x18   HUNGER_06              corazones de hambre, de 0 a 4
//  0x07  +0x1c   HAPPY_07               corazones de felicidad, de 0 a 4
//  0x08  +0x20   POOP_08                cacas en pantalla; con 8, enferma
//  0x09  +0x24   DISCIPLINE_09          nivel de disciplina, de 0 a 4:
//                                       regañarle a tiempo suma 1 (setCare).
//                                       Cada etapa empieza con el de su ficha
//                                       (DATA_PARAMETER[id][0x1c]; un 2 sale
//                                       2 o 3 al azar)
//  0x0a  +0x28   HUNGER_TIMER_0a        minutos para perder un corazón de
//                                       hambre (DATA_PARAMETER[id][0x8])
//  0x0b  +0x2c   HAPPY_TIMER_0b         lo mismo para la felicidad
//  0x0c  +0x30   POOP_TIMER_0c          minutos para la siguiente caca (180)
//  0x0d  +0x34   SICK_TIMER_0d          minutos para enfermar (setParaTimer)
//  0x0e  +0x38   TRANSFORM_0e           PARA_TIME_TRANSFORM, como lo llama el
//                                       printf del juego: pasos hasta la
//                                       transformación. Huevo 2 (segundos),
//                                       Babytchi 60 (minutos)
//  0x0f  +0x3c   DISCIPLINE_TIMER_0f    minutos para la llamada de disciplina
//  0x10  +0x40   STARVE_TIMER_10        muere de hambre: 720 minutos desde
//                                       que el hambre llega a 0
//  0x11  +0x44   SICK_DEATH_TIMER_11    muere enfermo: 360 minutos sin curar
//  0x12  +0x48   SICK_COUNT_12          veces que ha enfermado en la etapa;
//                                       con más de 2, muere
//  0x13  +0x4c   NO_FOOD_COUNT_13       veces que un adulto sin días para
//                                       evolucionar se queda sin hambre; con
//                                       más de 4, muere (doControlIvent/
//                                       awake.c)
//  0x14  +0x50   HUNGER_CALL_14         minutos para atender la llamada de
//                                       hambre (15; Babytchi 2)
//  0x15  +0x54   HAPPY_CALL_15          lo mismo, la de felicidad (15)
//  0x16  +0x58   DISCIPLINE_CALL_16     lo mismo, la de disciplina (30)
//  0x17  +0x5c   LIGHT_CALL_17          minutos para apagar la luz al
//                                       dormirse (60; Babytchi 5)
//  0x18  +0x60   CARE_MISS_18           fallos de cuidado: hambre, felicidad
//                                       o luz sin atender (Babytchi no)
//  0x19  +0x64   DISCIPLINE_MISS_19     fallos de disciplina
//  0x1a  +0x68   GAME_SEC_1a            segundos del reloj del juego, desde
//                                       1970 (solo el huevo)
//  0x1b  +0x6c   GAME_MIN_1b            minutos del reloj del juego, desde
//                                       1970
//  0x1c  +0x70   GAME_HOUR_1c           horas del reloj del juego, desde 1970
//  0x1d  +0x74   DAY_HOUR_1d            la hora que marca el teléfono, de 0 a
//                                       23 (m_nowTime.field_0xc): decide
//                                       cuándo duerme
//  0x1e  +0x78   STAGE_MINUTES_1e       minutos en la etapa actual
//  0x1f  +0x7c   SNACKS_1f              dulces hacia CANDY_22: con 3, vuelve
//                                       a 0 y CANDY_22 sube 1 (setCare)
//  0x20  +0x80   EVOL_DAYS_20           días para evolucionar por edad: -1 al
//                                       despertar (DATA_PARAMETER[id][0x4])
//  0x21  +0x84   STAGE_DAYS_21          días en la etapa actual: +1 al
//                                       despertar ("Parameter[33]")
//  0x22  +0x88   CANDY_22               contador de dulces: al despertar, con
//                                       CANDY_22 * 10 % de probabilidad,
//                                       programa una enfermedad y vuelve a 0
//  0x23  +0x8c   CANDY_SICK_23          minutos para esa enfermedad
//  0x24  +0x90   POOP_PENDING_24        cacas vistas en vivo que aún no se
//                                       han sumado a POOP_08
//  0x25  +0x94   COLOR_TYPE_25          tipo de color (getCharColorType)
//  0x26  +0x98   HISTORY_COUNT_26       colección: cambios de personaje
//                                       (getHistoryCount)
//  0x27  +0x9c   HISTORY_FLAGS_27       colección: un bit por tipo visto
//                                       (getHistoryFlags)
//  0x28  +0xa0   EVENT_SEED_28          semilla de eventos (getEventRandomSeed)
//  0x29 a 0x2f   -                      sin uso: solo se guardan en la
//                                       partida; GetParam y FutureScanner
//                                       llegan hasta 0x25
//
// Los bits de FLAGS_04 que se conocen. El resto, en hex.
//  0x1       FLAG_SICK_1             enfermo: 8 cacas, SICK_TIMER_0d o
//                                    CANDY_SICK_23
//  0x2       FLAG_DEAD_2             muerto: STARVE_TIMER_10,
//                                    SICK_DEATH_TIMER_11 o 3 enfermedades;
//                                    en vivo, setDay (sección 8)
//  0x8       -                       los adultos 7, 8 y 9 al acabarse
//                                    EVOL_DAYS_20 (sleeping.c): al morir,
//                                    controlGame (@0x3a784) añade la
//                                    animación 0x13 tras la 0x12
//  0x40      FLAG_LIGHT_ON_40        la luz encendida: se enciende al
//                                    despertar; setCare la apaga y enciende
//  0x80      FLAG_CLOCK_80           el reloj del juego ya está en hora (ver
//                                    doControlIvent)
//  0x100     FLAG_SLEEP_100          dormido
//  0x400     -                       ya se cerró alguna llamada de hambre:
//                                    la primera, atendida o no, cierra
//                                    también la de felicidad (setMissCnt).
//                                    Solo se borra con una partida nueva
//  0x10000   FLAG_HUNGER_CALL_10000      llamada de hambre      \ testGochCall
//  0x20000   FLAG_HAPPY_CALL_20000       llamada de felicidad    | las mira;
//  0x40000   FLAG_LIGHT_CALL_40000       llamada de la luz       | setMissCnt
//  0x80000   FLAG_DISCIPLINE_CALL_80000  llamada de disciplina  /  las cierra
//  0x100000  FLAG_CLOCK_BACK_100000  el reloj del sistema se atrasó

#define ID_00                0x00
#define AGE_01               0x01
#define WEIGHT_02            0x02
#define FLAGS_04             0x04
#define MEDICINE_05          0x05
#define HUNGER_06            0x06
#define HAPPY_07             0x07
#define POOP_08              0x08
#define DISCIPLINE_09        0x09
#define HUNGER_TIMER_0a      0x0a
#define HAPPY_TIMER_0b       0x0b
#define POOP_TIMER_0c        0x0c
#define SICK_TIMER_0d        0x0d
#define TRANSFORM_0e         0x0e
#define DISCIPLINE_TIMER_0f  0x0f
#define STARVE_TIMER_10      0x10
#define SICK_DEATH_TIMER_11  0x11
#define SICK_COUNT_12        0x12
#define NO_FOOD_COUNT_13     0x13
#define HUNGER_CALL_14       0x14
#define HAPPY_CALL_15        0x15
#define DISCIPLINE_CALL_16   0x16
#define LIGHT_CALL_17        0x17
#define CARE_MISS_18         0x18
#define DISCIPLINE_MISS_19   0x19
#define GAME_SEC_1a          0x1a
#define GAME_MIN_1b          0x1b
#define GAME_HOUR_1c         0x1c
#define DAY_HOUR_1d          0x1d
#define STAGE_MINUTES_1e     0x1e
#define SNACKS_1f            0x1f
#define EVOL_DAYS_20         0x20
#define STAGE_DAYS_21        0x21
#define CANDY_22             0x22
#define CANDY_SICK_23        0x23
#define POOP_PENDING_24      0x24
#define COLOR_TYPE_25        0x25
#define HISTORY_COUNT_26     0x26
#define HISTORY_FLAGS_27     0x27
#define EVENT_SEED_28        0x28

#define FLAG_SICK_1                 0x1
#define FLAG_DEAD_2                 0x2
#define FLAG_LIGHT_ON_40            0x40
#define FLAG_CLOCK_80               0x80
#define FLAG_SLEEP_100              0x100
#define FLAG_HUNGER_CALL_10000      0x10000
#define FLAG_HAPPY_CALL_20000       0x20000
#define FLAG_LIGHT_CALL_40000       0x40000
#define FLAG_DISCIPLINE_CALL_80000  0x80000
#define FLAG_CLOCK_BACK_100000      0x100000
