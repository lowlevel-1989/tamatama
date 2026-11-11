// Las funciones cortas que leen o escriben m_para directamente. Las de
// Tama2MovieMain y GameInterface_Implement son el puente con la interfaz del
// móvil (CMvFrameWork): no tienen llamadas directas, las llaman por la vtable.

#include "m_para.h"

// Cualquier posición, por su índice: de 0 a 0x25, y -1 fuera.
// @0x31bbc  Tama2MovieMain::GetParam(int)
int Tama2MovieMain::GetParam(int i) {
    // @0xc1ba8  m_para
    return i < 0x26 ? m_para[i] : -1;
}

// @0x31bd8  Tama2MovieMain::SetMParam(int, int)
void Tama2MovieMain::SetMParam(int i, int value) {
    if (i < 0x26) {
        m_para[i] = value;
    }
}

// El array entero.
// @0x31b80  Tama2MovieMain::getParameterArray()
int *Tama2MovieMain::getParameterArray(void) {
    return m_para;
}

// La semilla de los eventos: EVENT_SEED_28. controlMenu y fcn.00039b20 la
// escriben.
// @0x31bac  Tama2MovieMain::getEventRandomSeed()
int Tama2MovieMain::getEventRandomSeed(void) {
    return m_para[EVENT_SEED_28];
}

// El tipo de color del personaje: COLOR_TYPE_25. Lo usa doCountupGochCount.
// @0x38a28  Tama2MovieMain_Implement::getCharColorType()
int Tama2MovieMain_Implement::getCharColorType(void) {
    return m_para[COLOR_TYPE_25];
}

// ¿Hay alguna llamada abierta? Las cuatro flags de llamada: hambre 0x10000,
// felicidad 0x20000, luz 0x40000 y disciplina 0x80000 (setMissCnt.c).
// @0x38a40  Tama2MovieMain_Implement::testGochCall()
bool Tama2MovieMain_Implement::testGochCall(void) {
    return (m_para[FLAGS_04] & 0xf0000) != 0;
}

// La colección de personajes. doCountupGochCount (@0x38b70), al cambiar de
// personaje, suma 1 a HISTORY_COUNT_26 y pone su bit en HISTORY_FLAGS_27.
// @0x38a98  Tama2MovieMain_Implement::getHistoryCount()
int Tama2MovieMain_Implement::getHistoryCount(void) {
    return m_para[HISTORY_COUNT_26];   // cuántos cambios de personaje
}
// @0x38ab0  Tama2MovieMain_Implement::getHistoryFlags()
int Tama2MovieMain_Implement::getHistoryFlags(void) {
    return m_para[HISTORY_FLAGS_27];   // un bit por cada tipo visto
}
// Cuántos bits hay a 1 en m_para[HISTORY_FLAGS_27]: cuántos tipos distintos.
// @0x38ac8  Tama2MovieMain_Implement::getHistoryFlagsCount()
int Tama2MovieMain_Implement::getHistoryFlagsCount(void) {
    int count = 0;
    for (unsigned bits = m_para[HISTORY_FLAGS_27]; bits != 0; bits >>= 1) {
        count += bits & 1;
    }
    return count;
}
