// doControlIvent: un paso con el personaje dormido (FLAG_SLEEP_100). Solo
// mira dos cosas: el timer 0x17 y si toca despertar.
//   Babytchi  se despierta de la siesta pasado el minuto 44 de su etapa
//   el resto  se despierta al llegar su hora: DT_SLEEP_TIME[id] es
//             [hora de despertar, hora de dormir), con la hora del teléfono
// Despertar es un día más de edad (AGE_01).

#include "../m_para.h"

// Lo que tienen en común los dos despertares.
static void wakeUp(void) {
    m_para[FLAGS_04] = (m_para[FLAGS_04] & ~FLAG_SLEEP_100) | FLAG_LIGHT_ON_40;
    m_para[AGE_01] = min(m_para[AGE_01] + 1, 99);   // un día más
    // @0x33538  Tama2MovieMain_Implement::setMissCnt(int, int)
    setMissCnt(FLAG_LIGHT_CALL_40000, 2);
}

// La evolución por edad: se le acabaron los días de EVOL_DAYS_20. Se
// transformará hoy (TRANSFORM_0e, doControlIvent/transform.c), entre 2
// horas después de despertar y un minuto antes de dormir, y avisa al
// jugador. Los adultos 7, 8 y 9 ponen además la flag 0x8: al morir, tras la
// animación 0x12 de setDay, controlGame (@0x3a784) muestra también la 0x13.
static void evolveByAge(int id) {
    if (id >= 7 && id <= 9) {
        m_para[FLAGS_04] |= 0x8;
    }
    int awake = (DT_SLEEP_TIME[id][1] - DT_SLEEP_TIME[id][0]) * 60;
    m_rand = nextRand(m_rand);   // helpers.c
    m_para[TRANSFORM_0e] = 120 + (awake - 121) * m_rand / 32768;
    // @0x44ce4  CommonUtil::mvSetVibrator(int)
    CommonUtil::mvSetVibrator(500);
    // @0xc1e74  Tama2MovieMain_Implement::m_vibWork
    m_vibWork = 8;
}

// Los dulces: con CANDY_22 * 10 % de probabilidad, programa en
// CANDY_SICK_23 un minuto al azar de las horas despierto, a más de una hora
// de SICK_TIMER_0d. Al llegar a 0, enferma (awake.c). CANDY_22 vuelve a 0.
static void scheduleRandomTimer(int id) {
    if (m_para[CANDY_22] <= 0) {
        return;
    }
    // @0xc01c4  m_rand
    m_rand = nextRand(m_rand);   // helpers.c
    if (m_rand * 100 / 32768 < m_para[CANDY_22] * 10) {
        int minutes = (DT_SLEEP_TIME[id][1] - DT_SLEEP_TIME[id][0]) * 60 - 1;
        int r = m_rand;
        int when;
        do {
            r = nextRand(r);
            when = r * minutes / 32768;
        } while (when >= m_para[SICK_TIMER_0d] - 59 && when <= m_para[SICK_TIMER_0d] + 59);
        m_para[CANDY_SICK_23] = when;
        m_rand = r;
    }
    m_para[CANDY_22] = 0;
}

void sleepingStep(int step) {
    if (timerDone(LIGHT_CALL_17, step)) {   // helpers.c
        setMissCnt(FLAG_LIGHT_CALL_40000, 2);
    }

    // Babytchi: se despierta de la siesta.
    if (m_para[ID_00] == 1 && m_para[STAGE_MINUTES_1e] > 44) {
        wakeUp();
        restartTimeStep();
        saveParams();
        if (m_iventMode == 0) {
            // @0x3a3c0  Tama2MovieMain_Implement::setUntchAdd()
            setUntchAdd();
        }
        else {
            m_para[POOP_PENDING_24] += 1;
            // @0x33830  Tama2MovieMain_Implement::setAnimData(int)
            setAnimData(8);
            // @0x37fb8  Tama2MovieMain_Implement::setGameSec(int)
            setGameSec(5);
        }
        return;
    }

    // El resto: se despierta si ya es su hora.
    // @0xa3a88  Tama2MovieMain_Implement::DT_SLEEP_TIME
    int id = m_para[ID_00];
    if (DT_SLEEP_TIME[id][0] == -1 || !awakeHours(id)) {
        return;   // sigue durmiendo
    }
    wakeUp();
    m_para[STAGE_DAYS_21] += 1;
    if (id > 1) {
        // Un día menos para evolucionar; el último, evoluciona hoy.
        m_para[EVOL_DAYS_20] -= 1;
        if (m_para[EVOL_DAYS_20] < 1) {
            evolveByAge(id);
        }
        scheduleRandomTimer(id);
    }
    saveParams();
    // @0x380ec  Tama2MovieMain_Implement::setDefaultAnim()
    setDefaultAnim();
    if (m_iventMode != 0) {
        setGameSec(1);
    }
}
