// doControlIvent: el reloj del juego.
//
// Hay dos relojes:
//   la hora real   m_timeSec, m_timeMin, m_timeHour: segundos, minutos y
//                  horas desde 1970, de Date::getTime()
//   el del juego   m_para[GAME_SEC_1a], m_para[GAME_MIN_1b],
//                  m_para[GAME_HOUR_1c]: los mismos, pero solo avanzan de
//                  paso en paso
// doControlIvent avanza el del juego hasta alcanzar la hora real.
//
// Una llamada no es un paso. Mientras se juega, run() llama a doControlIvent
// en cada vuelta, unas 16 por segundo (run -> doMain -> controlGame), y cada
// llamada da los pasos que falten para alcanzar la hora real:
//   casi siempre                            0 pasos: ya está al día
//   al pasar un minuto (un segundo, huevo)  1 paso
//   tras 3 horas con el juego cerrado       180 pasos seguidos
//
// Un paso es una vuelta del bucle de dentro de doControlIvent (su
// do { ... } while), no una vuelta de run(): en una sola llamada puede dar
// 0, 1 o muchas. En cada paso el reloj del juego avanza una unidad y los
// timers de m_para bajan 1. La unidad depende del personaje:
//   huevo     1 paso = 1 segundo: sube GAME_SEC_1a hasta m_timeSec
//   el resto  1 paso = 1 minuto: sube GAME_MIN_1b hasta m_timeMin (y cada
//             60 pasos, GAME_HOUR_1c y DAY_HOUR_1d)
// Así que un paso no es tiempo de juego aparte: es tiempo real, contado de
// uno en uno. Si el juego estuvo cerrado 3 horas, al abrirlo da 180 pasos
// seguidos, uno por minuto que pasó, y los timers corren como si hubiera
// estado abierto.

#include "../m_para.h"

// La hora real, en ms desde 1970.
void readRealTime(void) {
    // @0x334d0  Tama2MovieMain_Implement::getNowTime()
    getNowTime();
    // @0xc1c80  Tama2MovieMain_Implement::m_realTime
    // @0x30164  Date::getTime()
    m_realTime = Date::getTime();
}

// La hora real en segundos, minutos y horas desde 1970. El % solo evita que
// pasen a negativo.
void splitRealTime(void) {
    // @0xc1c74  Tama2MovieMain_Implement::m_timeSec
    m_timeSec = (m_realTime / 1000) % 0x7ffffff8;
    // @0xc1c70  Tama2MovieMain_Implement::m_timeMin
    m_timeMin = (m_realTime / 60000) % 0x7ffffff8;
    // @0xc1c6c  Tama2MovieMain_Implement::m_timeHour
    m_timeHour = (m_realTime / 3600000) % 0x7ffffff8;
}

// El reloj del juego, en la hora real de ahora.
//
// m_nowTime es la fecha y la hora del reloj del teléfono, con su zona
// horaria: getNowTime (@0x334d0) la saca de Calendar::get(n), que usa
// localtime (@0x301b8, un switch sobre struct tm):
//   field_0x0   get(0)      año (tm_year + 1900)
//   field_0x4   get(1) + 1  mes, sumado dos veces: get(1) ya da tm_mon + 1,
//                           así que noviembre sale 12. Parece un error.
//   field_0x8   get(2)      día del mes
//   field_0xc   get(4)      hora, de 0 a 23
//   field_0x10  get(5)      minuto
//   field_0x14  get(6)      segundo
// get(3), el día de la semana, no se usa.
// @0xc1cb4  Tama2MovieMain_Implement::m_nowTime
static void setClockNow(void) {
    m_para[GAME_HOUR_1c] = m_timeHour;
    m_para[GAME_MIN_1b] = m_timeMin;
    m_para[GAME_SEC_1a] = m_timeSec;
    m_para[DAY_HOUR_1d] = m_nowTime.field_0xc;   // la hora del teléfono
    m_para[FLAGS_04] |= FLAG_CLOCK_80;
}

// Antes del bucle.
//
// FLAG_CLOCK_80: el reloj del juego ya tiene una hora de partida. El bucle
// avanza el reloj del juego paso a paso hasta la hora real; si siguiera a 0,
// que es 1970, daría unos 29 millones de pasos, uno por minuto desde
// entonces. Así que antes se pone en hora, una sola vez:
//   sin el bit -> reloj del juego = hora real -> pone el bit
//   con el bit -> el bucle solo cuenta lo que pasó desde la última vez
// Se borra cuando initCharacterData empieza de cero (FLAGS_04 = 0x40).
//
// Además, si no es en vivo:
//   un huevo vuelve a poner su reloj en hora y se transforma en 2 pasos
//   se procesa lo pendiente de m_para[POOP_PENDING_24]
void startClock(bool live) {
    int first = (m_para[FLAGS_04] & FLAG_CLOCK_80) == 0;
    if (first || (!live && m_para[ID_00] == 0)) {
        setClockNow();
    }
    if (live) {
        return;
    }
    if (m_para[ID_00] == 0) {
        m_para[TRANSFORM_0e] = 2;
    }
    // Las cacas vistas en vivo (POOP_PENDING_24, awake.c) se suman a
    // POOP_08; con 8, enferma.
    while (m_para[POOP_PENDING_24] > 0) {
        m_para[POOP_PENDING_24] -= 1;
        m_para[POOP_08] += 1;
        if (m_para[POOP_08] >= 8) {
            m_para[POOP_08] = 8;
            // @0x33628  Tama2MovieMain_Implement::setParaTimer(int)
            setParaTimer(SICK_TIMER_0d);
            setSick();   // helpers.c
        }
        saveParams();   // helpers.c
    }
}

// Una vuelta del bucle. Devuelve:
//   1   el reloj del juego iba por detrás: ha avanzado un paso
//   0   al día, pero es la primera vez (m_firstFlg): una vuelta más, en la
//       que los timers se comprueban sin bajar
//   -1  al día: doControlIvent termina
int clockStep(bool live) {
    int egg = m_para[ID_00] == 0;

    // ¿Por detrás? El huevo mira los segundos y el resto, los minutos.
    if (egg ? m_para[GAME_SEC_1a] < m_timeSec : m_para[GAME_MIN_1b] < m_timeMin) {
        if (egg) {
            m_para[GAME_SEC_1a] = (m_para[GAME_SEC_1a] + 1) % 0x7ffffff8;
            // @0xc1ba1  Tama2MovieMain_Implement::m_iventMode
            m_iventMode = live;
            return 1;
        }
        // Un minuto más del personaje y del reloj. Cada 60, una hora más.
        m_para[STAGE_MINUTES_1e] = (m_para[STAGE_MINUTES_1e] + 1) % 0x7ffffff8;
        m_para[GAME_MIN_1b] = (m_para[GAME_MIN_1b] + 1) % 0x7ffffff8;
        if (m_para[GAME_HOUR_1c] < m_timeHour && m_para[GAME_MIN_1b] % 60 == 0) {
            m_para[GAME_HOUR_1c] = (m_para[GAME_HOUR_1c] + 1) % 0x7ffffff8;
            m_para[DAY_HOUR_1d] = (m_para[DAY_HOUR_1d] + 1) % 24;
        }
        if (m_para[GAME_MIN_1b] == m_timeMin) {
            m_iventMode = live;   // el último paso: ahora mismo
        }
        else {
            // Tiempo pasado con el juego cerrado: m_db cuenta cuántos pasos.
            // @0xc1ba4  Tama2MovieMain_Implement::m_db
            m_db += 1;
            m_iventMode = 0;
        }
        return 1;
    }

    // Al día. Si la hora real va por detrás del reloj del juego, el reloj del
    // sistema se atrasó.
    if (m_timeMin < m_para[GAME_MIN_1b]) {
        m_para[FLAGS_04] |= FLAG_CLOCK_BACK_100000;
    }
    if (egg) {
        m_para[GAME_SEC_1a] = m_timeSec;
    }
    m_para[GAME_MIN_1b] = m_timeMin;
    m_para[GAME_HOUR_1c] = m_timeHour;
    m_para[DAY_HOUR_1d] = m_nowTime.field_0xc;   // la hora del teléfono

    // @0xc1ba0  Tama2MovieMain_Implement::m_firstFlg
    if (m_firstFlg == 0) {
        return -1;
    }
    m_firstFlg = 0;
    return 0;
}
