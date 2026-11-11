// Esto se llama al elegir un cuidado en el juego
// @0x39ba0  Tama2MovieMain_Implement::setCare(int, int)
//
// Todos los cuidados. what dice cuál y how, la variante:
//   what  how   cuidado
//   0     0     comida
//   0     1     dulce
//   1     0, 1  encender, apagar la luz
//   2     0, 1  minijuego ganado, perdido (lo mismo que setCarePlayResult)
//   3     -     medicina
//   4     -     limpiar las cacas
//   5     -     nada
//   6     -     regañar
// Siempre termina guardando la partida (saveRecord.c). Cada cuidado pone su
// animación (setAnimData) y casi todos la sección 5 (setGameSec).

#include "m_para.h"

// La comida. La rechaza (animación 0x14) si se está portando mal (llamada de
// disciplina abierta), si está enfermo o si tiene el hambre llena.
static void meal(void) {
    int full = m_para[HUNGER_06] > 3;
    int sick = m_para[FLAGS_04] & FLAG_SICK_1;
    int naughty = m_para[FLAGS_04] & FLAG_DISCIPLINE_CALL_80000;
    if (full || sick || naughty) {
        // @0x33830  Tama2MovieMain_Implement::setAnimData(int)
        setAnimData(0x14);
        // @0x37fb8  Tama2MovieMain_Implement::setGameSec(int)
        setGameSec(5);
        return;
    }
    m_para[HUNGER_06] += 1;
    // El peso sube 1, hasta 99. A Babytchi no le cambia.
    if (m_para[ID_00] != 1) {
        m_para[WEIGHT_02] = min(m_para[WEIGHT_02] + 1, 99);
    }
    // @0x33628  Tama2MovieMain_Implement::setParaTimer(int)
    setParaTimer(HUNGER_TIMER_0a);
    m_para[STARVE_TIMER_10] = -1;   // ya no muere de hambre
    setAnimData(1);
    setGameSec(5);
    // @0x33538  Tama2MovieMain_Implement::setMissCnt(int, int)
    setMissCnt(FLAG_HUNGER_CALL_10000, 0);   // la llamada, atendida
}

// El dulce: nunca lo rechaza. Cada 3 dulces, CANDY_22 sube 1: hace que
// enferme más (doControlIvent/sleeping.c).
static void snack(void) {
    if (m_para[HAPPY_07] < 4) {
        m_para[HAPPY_07] += 1;
    }
    // A Babytchi no le cuentan los dulces ni le cambia el peso.
    if (m_para[ID_00] != 1) {
        m_para[SNACKS_1f] += 1;
        if (m_para[SNACKS_1f] == 3) {
            m_para[SNACKS_1f] = 0;
            m_para[CANDY_22] += 1;
        }
        m_para[WEIGHT_02] = min(m_para[WEIGHT_02] + 2, 99);
    }
    setParaTimer(HAPPY_TIMER_0b);
    setAnimData(2);
    setGameSec(5);
    setMissCnt(FLAG_HAPPY_CALL_20000, 0);   // la llamada, atendida
}

void Tama2MovieMain_Implement::setCare(int what, int how) {
    // @0xc1ba8  m_para
    // @0xa3890  Tama2MovieMain_Implement::DATA_PARAMETER
    int id = m_para[ID_00];

    switch (what) {
    // Comer: comida o dulce.
    case 0:
        if (how == 0) meal();
        else if (how == 1) snack();
        break;

    // La luz. Si se apaga con la llamada de la luz abierta (se ha dormido),
    // la llamada queda atendida.
    case 1:
        if (how == 0) m_para[FLAGS_04] |= FLAG_LIGHT_ON_40;
        if (how == 1) m_para[FLAGS_04] &= ~FLAG_LIGHT_ON_40;
        if ((m_para[FLAGS_04] & FLAG_LIGHT_CALL_40000) &&
            !(m_para[FLAGS_04] & FLAG_LIGHT_ON_40)) {
            setMissCnt(FLAG_LIGHT_CALL_40000, 0);
        }
        break;

    // El minijuego: lo mismo que setCarePlayResult.c, con otras
    // animaciones.
    case 2:
        if ((how == 0 || how == 1) && id != 1) {
            m_para[WEIGHT_02] = max(m_para[WEIGHT_02] - 1, DATA_PARAMETER[id][0x0]);
        }
        if (how == 0) {
            if (m_para[HAPPY_07] < 4) {
                m_para[HAPPY_07] += 1;
            }
            setMissCnt(FLAG_HAPPY_CALL_20000, 0);
            setParaTimer(HAPPY_TIMER_0b);
            setAnimData(0xa);
        }
        else if (how == 1) {
            setAnimData(0xb);
        }
        break;

    // La medicina: una dosis menos, y con 0 se cura. Sin estar enfermo, la
    // rechaza (animación 0xd).
    case 3:
        if (m_para[FLAGS_04] & FLAG_SICK_1) {
            m_para[MEDICINE_05] -= 1;
            if (m_para[MEDICINE_05] == 0) {
                m_para[FLAGS_04] &= ~FLAG_SICK_1;
                m_para[SICK_DEATH_TIMER_11] = -1;   // ya no muere enfermo
            }
            setAnimData(0xb);
        }
        else {
            setAnimData(0xd);
        }
        setGameSec(5);
        break;

    // Limpiar: todas las cacas de golpe.
    case 4:
        if (m_para[POOP_08] > 0) {
            m_para[POOP_08] = 0;
            setAnimData(0xa);
            setGameSec(5);
        }
        break;

    // Regañar. Solo sirve si se está portando mal: cierra la llamada de
    // disciplina y su nivel de disciplina sube 1, hasta 4. Si no, solo la
    // animación.
    case 6:
        if (m_para[FLAGS_04] & FLAG_DISCIPLINE_CALL_80000) {
            setMissCnt(FLAG_DISCIPLINE_CALL_80000, 0);
            if (m_para[DISCIPLINE_09] < 4) {
                m_para[DISCIPLINE_09] += 1;
            }
            setAnimData(0xb);
            setGameSec(5);
            // Con el sonido activado, el de regañar (playSound(0, 0xb)).
            // @0xbd078  Tama2MovieMain_Implement::m_SePlay_OnOff
            if (m_SePlay_OnOff != 0) {
                // @0xbd224  Tama2MovieMain_Implement::bgm_num
                bgm_num = -1;
                // @0x31acc  Tama2MovieSound::stopSound()
                Tama2MovieSound::stopSound();
                // @0x31a94  Tama2MovieSound::playSound(int, int)
                Tama2MovieSound::playSound(0, 0xb);
            }
            // @0xc1ec4  Tama2MovieMain_Implement::sound_num
            sound_num = -1;
        }
        else {
            setAnimData(0xb);
            setGameSec(5);
        }
        break;
    }

    // Copia m_para[0x0] a m_para[0x2f] en m_recData[6] a m_recData[53] y lo
    // guarda (doc/code/saveRecord.c).
    // @0xc1d50  Tama2MovieMain_Implement::m_recData
    for (int i = 0; i < 48; i++) {
        m_recData[6 + i] = m_para[i];
    }
    // @0x32e90  Tama2MovieMain_Implement::saveRecord()
    saveRecord();
}
