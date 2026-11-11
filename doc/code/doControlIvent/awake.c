// doControlIvent: un paso con el personaje despierto. Todo en este orden:
//   1. ¿muere? Si toca, termina doControlIvent
//   2. las llamadas sin atender y las tres llamadas al jugador (calls.c)
//   3. enfermar: SICK_TIMER_0d y CANDY_SICK_23
//   4. la transformación (transform.c): en vivo, termina doControlIvent
//   5. ¿toca dormir?
//   6. la caca: POOP_TIMER_0c
//
// Devuelve 0 si doControlIvent tiene que terminar.

#include "../m_para.h"

// 1. Tres maneras de morir. Babytchi nunca; el huevo, solo enfermo.
static int dies(int step) {
    // 12 horas sin corazones de hambre.
    if (timerDone(STARVE_TIMER_10, step) && m_para[ID_00] > 1) {
        restartTimeStep();
        m_para[STARVE_TIMER_10] = -1;
        setDead();   // helpers.c
        return 1;
    }
    // 6 horas enfermo sin curar.
    if (timerDone(SICK_DEATH_TIMER_11, step) && m_para[ID_00] != 1) {
        restartTimeStep();
        m_para[SICK_DEATH_TIMER_11] = -1;
        setDead();
        return 1;
    }
    // 3 enfermedades en la misma etapa.
    if (m_para[SICK_COUNT_12] > 2 && m_para[ID_00] != 1) {
        restartTimeStep();
        setDead();
        return 1;
    }
    return 0;
}

// 5. Se duerme fuera de sus horas despierto, según la hora del teléfono
// (DT_SLEEP_TIME[id] es [hora de despertar, hora de dormir)), o, Babytchi,
// en la siesta: minutos 40 a 44 de su etapa.
// @0xa3a88  Tama2MovieMain_Implement::DT_SLEEP_TIME
static void maybeSleep(void) {
    int id = m_para[ID_00];
    if (id <= 0) {
        return;   // el huevo no duerme
    }
    int nap = id == 1 && m_para[STAGE_MINUTES_1e] >= 40 && m_para[STAGE_MINUTES_1e] <= 44;
    int awake = DT_SLEEP_TIME[id][1] == -1 || awakeHours(id);
    if (!nap && awake) {
        return;
    }
    restartTimeStep();
    // Las tres llamadas quedan sin atender (calls.c).
    // @0x33538  Tama2MovieMain_Implement::setMissCnt(int, int)
    setMissCnt(FLAG_HUNGER_CALL_10000, 2);
    setMissCnt(FLAG_HAPPY_CALL_20000, 2);
    setMissCnt(FLAG_DISCIPLINE_CALL_80000, 2);
    m_para[FLAGS_04] |= FLAG_SLEEP_100 | FLAG_LIGHT_CALL_40000;
    m_para[LIGHT_CALL_17] = (id == 1) ? 5 : 60;   // el timer 0x17 de sleeping.c
    // @0xc1c68  Tama2MovieMain_Implement::m_callFlg
    m_callFlg = 1;
    saveParams();
    if (m_iventMode != 0) {
        fcn.00039a4c();
    }
    // @0x380ec  Tama2MovieMain_Implement::setDefaultAnim()
    setDefaultAnim();
}

// 6. Parece la caca ("unchi"): cada 180 minutos, Babytchi no. En vivo se ve
// con la animación 8 y se apunta en m_para[POOP_PENDING_24], que el siguiente
// doControlIvent no en vivo procesa (clock.c); en el pasado, setUntchAdd
// directamente.
static void poop(int step) {
    if (!timerDone(POOP_TIMER_0c, step)) {
        return;
    }
    restartTimeStep();
    m_para[POOP_TIMER_0c] = (m_para[ID_00] == 1) ? -1 : 180;
    if (m_iventMode != 0) {
        m_para[POOP_PENDING_24] += 1;
        // @0x33830  Tama2MovieMain_Implement::setAnimData(int)
        setAnimData(8);
        // @0xbd224  Tama2MovieMain_Implement::bgm_num
        bgm_num = -1;
        // @0x31acc  Tama2MovieSound::stopSound()
        Tama2MovieSound::stopSound();
        // @0x37fb8  Tama2MovieMain_Implement::setGameSec(int)
        setGameSec(5);
    }
    else {
        // @0x3a3c0  Tama2MovieMain_Implement::setUntchAdd()
        setUntchAdd();
    }
}

int awakeStep(int step) {
    // 1.
    if (dies(step)) {
        return 0;
    }

    // 2.
    // Un adulto que ha dejado de comer 5 veces muere.
    if (m_para[NO_FOOD_COUNT_13] > 4) {
        restartTimeStep();
        setDead();   // helpers.c
        return 0;
    }
    missedCalls(step);   // calls.c
    ownerCalls(step);    // calls.c

    // 3.
    // Enferma cuando vence su timer, y el timer vuelve a empezar.
    if (timerDone(SICK_TIMER_0d, step)) {
        restartTimeStep();
        setSick();   // helpers.c
        // @0x33628  Tama2MovieMain_Implement::setParaTimer(int)
        setParaTimer(SICK_TIMER_0d);
    }
    // Enferma por los dulces, en el minuto que se programó al despertar
    // (sleeping.c).
    if (timerDone(CANDY_SICK_23, step)) {
        restartTimeStep();
        setSick();   // helpers.c
        m_para[CANDY_SICK_23] = -1;
        setDefaultAnim();
        if (m_iventMode != 0) {
            bgm_num = -1;
            Tama2MovieSound::stopSound();
            setGameSec(1);
        }
    }

    // 4.
    if (!transform(step)) {   // transform.c
        return 0;
    }

    // 5. y 6.
    maybeSleep();
    poop(step);
    return 1;
}
