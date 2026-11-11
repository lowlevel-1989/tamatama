// doControlIvent: las llamadas del personaje al jugador.
//
// Hay tres, cada una con su timer y su flag. Al llamar, pone un segundo
// timer con el tiempo que hay para atenderla: 2 minutos con Babytchi, 15 o
// 30 con el resto. Si llega a 0 sin atenderla, cuenta como un fallo.
//
//   timer                cuenta     flag     plazo
//   HUNGER_TIMER_0a      HUNGER_06  0x10000  HUNGER_CALL_14
//   HAPPY_TIMER_0b       HAPPY_07   0x20000  HAPPY_CALL_15
//   DISCIPLINE_TIMER_0f  -          0x80000  DISCIPLINE_CALL_16
// Si vence el plazo sin atenderla, la llamada se cierra y cuenta un fallo
// (setMissCnt.c). Solo eso: con el hambre a 0 no enferma.
//
// Las dos primeras no llaman a la primera: cada vez que expira su timer
// baja su cuenta, y llaman cuando la cuenta llega a 0. Al dormirse, las tres
// quedan sin atender (setMissCnt, awake.c).

#include "../m_para.h"

// Las llamadas que se quedaron sin atender.
void missedCalls(int step) {
    // @0x33538  Tama2MovieMain_Implement::setMissCnt(int, int)
    if (timerDone(HUNGER_CALL_14, step)) {   // helpers.c
        setMissCnt(FLAG_HUNGER_CALL_10000, 2);
    }
    if (timerDone(HAPPY_CALL_15, step)) {
        setMissCnt(FLAG_HAPPY_CALL_20000, 2);
    }
    if (timerDone(DISCIPLINE_CALL_16, step)) {
        setMissCnt(FLAG_DISCIPLINE_CALL_80000, 2);
        setDefaultAnim();
    }
}

// Lo común de las tres: la llamada, y el aviso si es en vivo.
// @0xc1c68  Tama2MovieMain_Implement::m_callFlg
static void call(void) {
    m_callFlg = 1;
    if (m_iventMode != 0) {
        callOwner();   // helpers.c: vibra y suena
    }
}

void ownerCalls(int step) {
    // La primera. Además, la primera vez pone en marcha el timer 0x10 (12
    // horas) y, a partir del personaje 7, cuenta en m_para[NO_FOOD_COUNT_13].
    if (timerDone(HUNGER_TIMER_0a, step)) {
        restartTimeStep();
        m_para[HUNGER_06] -= 1;
        if (m_para[HUNGER_06] < 1) {
            m_para[HUNGER_06] = 0;
            m_para[FLAGS_04] |= FLAG_HUNGER_CALL_10000;
            m_para[HUNGER_CALL_14] = (m_para[ID_00] == 1) ? 2 : 15;
            call();
            if (m_para[STARVE_TIMER_10] < 0) {
                m_para[STARVE_TIMER_10] = 720;
            }
            if (m_para[ID_00] > 6 && m_para[EVOL_DAYS_20] < 0) {
                m_para[NO_FOOD_COUNT_13] += 1;
            }
        }
        // @0x33628  Tama2MovieMain_Implement::setParaTimer(int)
        setParaTimer(HUNGER_TIMER_0a);
    }

    // La segunda.
    if (timerDone(HAPPY_TIMER_0b, step)) {
        restartTimeStep();
        m_para[HAPPY_07] -= 1;
        if (m_para[HAPPY_07] < 1) {
            m_para[FLAGS_04] |= FLAG_HAPPY_CALL_20000;
            m_para[HAPPY_07] = 0;
            m_para[HAPPY_CALL_15] = (m_para[ID_00] == 1) ? 2 : 15;
            call();
        }
        setParaTimer(HAPPY_TIMER_0b);
    }

    // La tercera: llama en cuanto expira su timer.
    if (timerDone(DISCIPLINE_TIMER_0f, step)) {
        restartTimeStep();
        m_para[FLAGS_04] |= FLAG_DISCIPLINE_CALL_80000;
        m_para[DISCIPLINE_CALL_16] = (m_para[ID_00] == 1) ? 2 : 30;
        call();
        setParaTimer(DISCIPLINE_TIMER_0f);
    }
}
