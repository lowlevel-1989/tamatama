// Esto se llama desde Step
// @0x38bc4  Tama2MovieMain_Implement::initCharacterData(int, int)
//
// Prepara m_para, los datos del personaje, cuando cambia:
//   is_evol == 0  empieza de cero con un huevo: casi todo a 0 o a -1, y los
//                 timers de evolución de la fila del huevo
//                 (DATA_PARAMETER[0]: de ahí sale TRANSFORM_0e = 2)
//   is_evol != 0  pasa al personaje id: si ya es ese, no hace nada; si no,
//                 conserva lo que sigue valiendo (el peso, si no baja del
//                 mínimo del nuevo) y reinicia el resto
// Los valores de cada personaje salen de su fila en DATA_PARAMETER.
//
// Al final copia m_para en el registro de la partida (m_recData) y lo guarda
// en gamestatus_0.dat con saveRecord (doc/code/saveRecord.c).

#include "m_para.h"

void Tama2MovieMain_Implement::initCharacterData(int is_evol, int id) {

    // @0xc1ba8  Tama2MovieMain_Implement::m_para
    if (is_evol == 0) {
        // El binario pone m_para[ID_00] = 0 (0x38c92) antes de leerlo para
        // DATA_PARAMETER (0x38cac), así que usa la fila del huevo, no la del
        // personaje que había.
        id = 0;

        m_para[ID_00] =  0; // id
        m_para[AGE_01] =  0;
        m_para[WEIGHT_02] =  1; // peso minimo
        m_para[FLAGS_04] = 64;
        m_para[MEDICINE_05] =  0;
        m_para[HUNGER_06] =  0;
        m_para[HAPPY_07] =  0;
        m_para[POOP_08] =  0;
        m_para[DISCIPLINE_09] =  0;

        // @0x33628  Tama2MovieMain_Implement::setParaTimer(int)
        setParaTimer(HUNGER_TIMER_0a);
        setParaTimer(HAPPY_TIMER_0b);

        m_para[POOP_TIMER_0c] = -1;
        m_para[SICK_TIMER_0d] = -1;

        // @0xa3890  Tama2MovieMain_Implement::DATA_PARAMETER
        // Probablemente es el que el printf del juego llama
        // PARA_TIME_TRANSFORM: con Babytchi vale 60 (ver más abajo) y el log
        // cuenta hacia atrás desde 60.
        m_para[TRANSFORM_0e] = DATA_PARAMETER[id][0x4];  // Timer evol
        m_para[DISCIPLINE_TIMER_0f] = DATA_PARAMETER[id][0x20];

        m_para[STARVE_TIMER_10] = -1;
        m_para[SICK_DEATH_TIMER_11] = -1;
        m_para[SICK_COUNT_12] = 0;
        m_para[NO_FOOD_COUNT_13] = 0;
        m_para[HUNGER_CALL_14] = -1;
        m_para[HAPPY_CALL_15] = -1;
        m_para[DISCIPLINE_CALL_16] = -1;
        m_para[LIGHT_CALL_17] = -1;
        m_para[CARE_MISS_18] = 0;
        m_para[DISCIPLINE_MISS_19] = 0;
        m_para[STAGE_MINUTES_1e] = 0;
        m_para[SNACKS_1f] = 0;
        m_para[EVOL_DAYS_20] = 0;
        m_para[STAGE_DAYS_21] = 0;
        m_para[CANDY_22] = 0;
        m_para[CANDY_SICK_23] = -1;
        m_para[POOP_PENDING_24] = 0;
        goto L_38c6e;
    }

    // Evolución: si ya es ese personaje, no cambia nada.
    if (m_para[ID_00] == id) {
        return;
    }

    m_para[ID_00] = id;

    // Cuenta un personaje más en la colección (el contador de "goch").
    // @0x38b70  Tama2MovieMain_Implement::doCountupGochCount()
    doCountupGochCount();

    // El peso se conserva, pero no por debajo del mínimo del nuevo personaje.
    if (m_para[WEIGHT_02] < DATA_PARAMETER[id][0x0]) {
        m_para[WEIGHT_02] = DATA_PARAMETER[id][0x0]; // peso minimo
    }

    // Borra el bit 0x8 y las cuatro llamadas (0xf0000): el nuevo personaje
    // empieza sin llamadas abiertas.
    m_para[FLAGS_04] = m_para[FLAGS_04] & 0xfff0fff7;

    // Un 2 en la tabla significa "sortéalo": sale 3 con un 51 % y 2 con un
    // 49 % (m_rand * 100 / 32768 es un porcentaje de 0 a 99).
    m_para[DISCIPLINE_09] = DATA_PARAMETER[id][0x1c];
    if (DATA_PARAMETER[id][0x1c] == 2){
        // @0xc01c4  m_rand
        // next = next * 1103515245 + 12345;
        m_rand = m_rand * 0x41c64e6d + 0x3039 & 0x7fff;
        m_para[DISCIPLINE_09] = (m_rand * 100 / 32768 <= 50) ? 3 : 2;
    }

    // Babytchi
    if (id == 1) {
        m_para[HAPPY_TIMER_0b] = 2;
        m_para[POOP_TIMER_0c] = 15;
        m_para[SICK_TIMER_0d] = 45;
        m_para[HUNGER_TIMER_0a]  = 0;
        m_para[TRANSFORM_0e] = 60; // tiempo de evolución
        // @0xc1ba0  Tama2MovieMain_Implement::m_firstFlg
        m_firstFlg = 1;
    }
    // El resto de personajes: timers nuevos, y el de evolución
    // (m_para[EVOL_DAYS_20]) sale de su fila de DATA_PARAMETER.
    else {
        m_para[POOP_TIMER_0c] = 0xb4;
        setParaTimer(SICK_TIMER_0d);
        setParaTimer(HUNGER_TIMER_0a);
        setParaTimer(HAPPY_TIMER_0b);
        m_para[EVOL_DAYS_20] = DATA_PARAMETER[id][0x4];
        m_para[TRANSFORM_0e] = -1;
    }

    // Esto lo hacen las dos ramas: el original lo tiene una sola vez y salta
    // a él con goto desde cada una. Si el personaje pide disciplina, pone en
    // marcha su timer (setParaTimer.c); si no, se queda con el valor de la
    // tabla (-1: nunca).
    if (DATA_PARAMETER[id][0x20] > 0) {
        setParaTimer(DISCIPLINE_TIMER_0f);
    }
    else {
        m_para[DISCIPLINE_TIMER_0f] = DATA_PARAMETER[id][0x20];
    }

    m_para[STAGE_DAYS_21] = 0;
    m_para[SICK_COUNT_12] = 0;
    m_para[NO_FOOD_COUNT_13] = 0;
    m_para[CARE_MISS_18] = 0;
    m_para[DISCIPLINE_MISS_19] = 0;
    m_para[SNACKS_1f] = 0;
    m_para[STAGE_MINUTES_1e] = 0;
    m_para[CANDY_22] = 0;
    m_para[HUNGER_CALL_14] = -1;
    m_para[HAPPY_CALL_15] = -1;
    m_para[DISCIPLINE_CALL_16] = -1;
    m_para[LIGHT_CALL_17] = -1;
    m_para[CANDY_SICK_23] = -1;
L_38c6e:
    // Copia m_para[0x0] a m_para[0x2f] (48 int) en m_recData[6] a
    // m_recData[53]. El original lo escribe en bytes: de field_0x18 a
    // field_0xd8.
    // @0xc1d50  Tama2MovieMain_Implement::m_recData
    for (int i = 0; i < 48; i++) {
        m_recData[6 + i] = m_para[i];
    }
    // Y lo guarda: en el fichero, m_para[i] queda en 0x118 + i * 4, en big
    // endian (doc/code/saveRecord.c).
    // @0x32e90  Tama2MovieMain_Implement::saveRecord()
    saveRecord();
}

