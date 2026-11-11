// Esto se llama desde doControlIvent, en vivo, al poner la flag 0x2
// @0x38160  Tama2MovieMain_Implement::setDay()
//
// Parece la muerte del personaje, aunque se llame setDay. Llega desde los
// tres caminos a la flag 0x2 de doControlIvent/awake.c:
//   m_para[STARVE_TIMER_10]  12 horas sin corazones de hambre
//   m_para[SICK_DEATH_TIMER_11]  6 horas enfermo sin curar
//   m_para[SICK_COUNT_12]  más de 2 enfermedades
// Pone la sección 8, la música 9 y la animación 0x12, y reinicia lo de la
// tumba (m_grave...: dispGrave dibuja la tumba). En tiempo pasado,
// doControlIvent no llama aquí: pone la flag 0x200 y la animación normal.

#include "m_para.h"

void Tama2MovieMain_Implement::setDay(void) {
    // @0xc1ba8  m_para
    m_para[FLAGS_04] |= FLAG_LIGHT_ON_40;
    m_para[POOP_08] = 0;   // sin cacas

    // @0x37fb8  Tama2MovieMain_Implement::setGameSec(int)
    setGameSec(8);
    // @0xbd078  Tama2MovieMain_Implement::m_SePlay_OnOff
    if (m_SePlay_OnOff != 0) {
        // @0xbd224  Tama2MovieMain_Implement::bgm_num
        bgm_num = -1;
        // @0x31acc  Tama2MovieSound::stopSound()
        Tama2MovieSound::stopSound();
        // @0x31a94  Tama2MovieSound::playSound(int, int)
        Tama2MovieSound::playSound(0, 9);
    }
    // @0xc1ec4  Tama2MovieMain_Implement::sound_num
    sound_num = -1;
    // @0x33830  Tama2MovieMain_Implement::setAnimData(int)
    setAnimData(0x12);

    // La tumba, desde cero.
    // @0xc1cfc  Tama2MovieMain_Implement::m_graveFlg
    m_graveFlg = 0;
    // @0xc1cf4  Tama2MovieMain_Implement::m_graveSec
    m_graveSec = 0;
    // @0xc1cf8  Tama2MovieMain_Implement::m_graveScr
    m_graveScr = 0;
    // @0xc1b98  Tama2MovieMain_Implement::m_dayTimer
    // @0x30104  System::currentTimeMillis()
    m_dayTimer = System::currentTimeMillis();
}
