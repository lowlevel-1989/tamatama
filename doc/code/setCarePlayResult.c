// Esto se llama al terminar el minijuego
// @0x3abe0  Tama2MovieMain_Implement::setCarePlayResult(int)
//
// El resultado del minijuego. result:
//   0   ganado: un corazón de felicidad más (hasta 4), un punto menos de
//       peso, y la llamada de felicidad queda atendida
//   1   perdido: solo un punto menos de peso
// El peso nunca baja del mínimo del personaje (DATA_PARAMETER[id][0x0]), y a
// Babytchi no le cambia. Después guarda la partida.

#include "m_para.h"

void Tama2MovieMain_Implement::setCarePlayResult(int result) {
    // @0xc1ba8  m_para
    // @0xa3890  Tama2MovieMain_Implement::DATA_PARAMETER
    int id = m_para[ID_00];

    // Jugar adelgaza, ganes o pierdas.
    if ((result == 0 || result == 1) && id != 1) {
        m_para[WEIGHT_02] = max(m_para[WEIGHT_02] - 1, DATA_PARAMETER[id][0x0]);
    }

    if (result == 0) {
        // m_para[HAPPY_07]: los corazones de felicidad, hasta 4.
        if (m_para[HAPPY_07] < 4) {
            m_para[HAPPY_07] += 1;
        }
        // @0x33538  Tama2MovieMain_Implement::setMissCnt(int, int)
        // La llamada de felicidad, atendida.
        setMissCnt(FLAG_HAPPY_CALL_20000, 0);
        // @0x33628  Tama2MovieMain_Implement::setParaTimer(int)
        setParaTimer(HAPPY_TIMER_0b);        // su timer vuelve a empezar
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
