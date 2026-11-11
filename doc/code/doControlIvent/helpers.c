// doControlIvent: lo que el original repite en cada sitio, en una función
// con nombre. El binario no tiene estas funciones: el compilador las escribe
// enteras cada vez.

#include "../m_para.h"

// Un timer de m_para: con -1 está parado. Si no, baja step y avisa cuando
// llega a 0 o menos. El original lo escribe en cada uno:
//   (m_para[i] > -1) && (m_para[i] -= step) < 1
bool timerDone(int i, int step) {
    if (m_para[i] < 0) return false;
    m_para[i] -= step;
    return m_para[i] < 1;
}

// Va delante de casi todo lo que cambia el estado.
// @0xc1f68  Tama2MovieMain_Implement::m_timeStep
// @0xc1f58  Tama2MovieMain_Implement::m_timeNow
// @0xc1f60  Tama2MovieMain_Implement::m_timeBack
void restartTimeStep(void) {
    m_timeStep = 0;
    m_timeNow = m_timeBack;
}

// Copia m_para[0x0] a m_para[0x2f] (48 int) en m_recData[6] a m_recData[53]
// y lo guarda en gamestatus_0.dat (doc/code/saveRecord.c). Es el mismo bucle
// que al final de initCharacterData.
// @0xc1d50  Tama2MovieMain_Implement::m_recData
// @0x32e90  Tama2MovieMain_Implement::saveRecord()
void saveParams(void) {
    for (int i = 0; i < 48; i++) {
        m_recData[6 + i] = m_para[i];
    }
    saveRecord();
}

// El personaje llama al jugador: vibra medio segundo y, si el sonido está
// activado, cambia la música por el aviso.
void callOwner(void) {
    // @0x44ce4  CommonUtil::mvSetVibrator(int)
    CommonUtil::mvSetVibrator(500);
    // @0xc1e74  Tama2MovieMain_Implement::m_vibWork
    m_vibWork = 8;
    // @0xbd078  Tama2MovieMain_Implement::m_SePlay_OnOff
    if (m_SePlay_OnOff != 0) {
        // @0xbd224  Tama2MovieMain_Implement::bgm_num
        bgm_num = -1;
        // @0x31acc  Tama2MovieSound::stopSound()
        Tama2MovieSound::stopSound();
        // @0x31a94  Tama2MovieSound::playSound(int, int)
        Tama2MovieSound::playSound(0, 3);
    }
    // @0xc1ec4  Tama2MovieMain_Implement::sound_num
    sound_num = -1;
    // @0x37fb8  Tama2MovieMain_Implement::setGameSec(int)
    setGameSec(1);
}

// Muere: FLAG_DEAD_2. En vivo, setDay muestra la muerte; en tiempo pasado,
// pone además la flag 0x200 y la animación normal.
void setDead(void) {
    m_para[FLAGS_04] |= FLAG_DEAD_2;
    if (m_iventMode == 0) {
        m_para[FLAGS_04] |= 0x200;
        // @0x380ec  Tama2MovieMain_Implement::setDefaultAnim()
        setDefaultAnim();
    }
    else {
        // @0x38160  Tama2MovieMain_Implement::setDay()
        setDay();
    }
}

// Enferma: FLAG_SICK_1, las dosis de medicina de su ficha, una enfermedad
// más y el timer de 6 horas para morir si no se cura. Lo hacen las 8 cacas
// (clock.c) y CANDY_SICK_23 (awake.c).
// @0xa3890  Tama2MovieMain_Implement::DATA_PARAMETER
void setSick(void) {
    m_para[FLAGS_04] |= FLAG_SICK_1;
    m_para[MEDICINE_05] = DATA_PARAMETER[m_para[ID_00]][0x14];
    m_para[SICK_COUNT_12] += 1;
    if (m_para[SICK_DEATH_TIMER_11] < 0) {
        m_para[SICK_DEATH_TIMER_11] = 360;
    }
}

// La hora del día está dentro de las horas en que el personaje está
// despierto: DT_SLEEP_TIME[id] es [hora de despertar, hora de dormir).
// @0xa3a88  Tama2MovieMain_Implement::DT_SLEEP_TIME
bool awakeHours(int id) {
    return DT_SLEEP_TIME[id][0] <= m_para[DAY_HOUR_1d] && m_para[DAY_HOUR_1d] < DT_SLEEP_TIME[id][1];
}

// next = next * 1103515245 + 12345, quedándose con 15 bits: como en run.c.
int nextRand(int r) {
    return r * 1103515245 + 12345 & 0x7fff;
}

