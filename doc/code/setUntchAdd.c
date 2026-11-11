// Esto se llama desde doControlIvent: POOP_TIMER_0c en tiempo pasado, y al
// despertar Babytchi
// @0x3a3c0  Tama2MovieMain_Implement::setUntchAdd()
//
// Una caca más ("unchi" en japonés). POOP_08 es cuántas hay en pantalla:
// dispUntch (@0x3565c) dibuja tantas, en dos columnas. Con 8, enferma:
//   FLAG_SICK_1
//   MEDICINE_05          las dosis de su ficha (DATA_PARAMETER[id][0x14])
//   SICK_COUNT_12        una enfermedad más; con más de 2, muere
//   SICK_DEATH_TIMER_11  6 horas para curarse, o muere
// Después guarda la partida.
//
// Lo mismo hace doControlIvent con POOP_PENDING_24 (doControlIvent/clock.c).

#include "m_para.h"

void Tama2MovieMain_Implement::setUntchAdd(void) {
    // @0xc1ba8  m_para
    m_para[POOP_08] += 1;
    if (m_para[POOP_08] >= 8) {
        m_para[POOP_08] = 8;
        // @0x33628  Tama2MovieMain_Implement::setParaTimer(int)
        setParaTimer(SICK_TIMER_0d);
        // Enferma: lo mismo que setSick en doControlIvent/helpers.c.
        m_para[FLAGS_04] |= FLAG_SICK_1;
        // @0xa3890  Tama2MovieMain_Implement::DATA_PARAMETER
        m_para[MEDICINE_05] = DATA_PARAMETER[m_para[ID_00]][0x14];
        m_para[SICK_COUNT_12] += 1;
        if (m_para[SICK_DEATH_TIMER_11] < 0) {
            m_para[SICK_DEATH_TIMER_11] = 360;
        }
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
