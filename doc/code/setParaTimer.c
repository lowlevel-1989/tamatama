// Esto se llama desde initCharacterData, doControlIvent, setUntchAdd y
// setCarePlayResult
// @0x33628  Tama2MovieMain_Implement::setParaTimer(int)
//
// Pone en marcha un timer de m_para: n es su posición. Cada timer cuenta los
// minutos que faltan para que pase algo, y doControlIvent lo baja 1 por
// minuto. Los valores salen de la fila del personaje en DATA_PARAMETER.
//
//   n     timer de           al llegar a 0                  valor
//   0xa   el hambre          un corazón de hambre menos     fijo
//   0xb   la felicidad       un corazón de felicidad menos  fijo, el mismo
//   0xd   la enfermedad      enferma                        al azar
//   0xf   la disciplina      llamada de disciplina          al azar
// Con cualquier otro n no hace nada.
//
// Los campos de la fila que usa (en bytes, como en los demás ficheros):
//   DATA_PARAMETER[id][0x8]   hungry/happy timer
//   DATA_PARAMETER[id][0xc]   illness, scale factor
//   DATA_PARAMETER[id][0x10]  illness, div factor
//   DATA_PARAMETER[id][0x20]  discipline timer

#include "m_para.h"

// Un número al azar de 0 a 32767: el generador de run.c.
// @0xc01c4  m_rand
static int nextRand(void) {
    m_rand = m_rand * 0x41c64e6d + 0x3039 & 0x7fff;
    return m_rand;
}

void Tama2MovieMain_Implement::setParaTimer(int n) {
    // @0xa3890  Tama2MovieMain_Implement::DATA_PARAMETER
    // @0xc1ba8  m_para
    int id = m_para[ID_00];

    switch (n) {
    // El timer del hambre: hungry/happy timer.
    case 0xa:
        m_para[HUNGER_TIMER_0a] = DATA_PARAMETER[id][0x8];
        break;

    // El timer de la felicidad: el mismo valor, bajan al mismo ritmo.
    case 0xb:
        m_para[HAPPY_TIMER_0b] = DATA_PARAMETER[id][0x8];
        break;

    // El timer de la enfermedad: al azar, según las horas despierto.
    case 0xd: {
        // El huevo y Babytchi no vuelven a enfermar por tiempo: timer parado.
        // Babytchi enferma una sola vez, con el 45 que le pone
        // initCharacterData al nacer.
        if (id <= 1) {
            m_para[SICK_TIMER_0d] = -1;
            break;
        }
        // Una parte de los minutos que pasa despierto al día, según su
        // scale y div factor:
        //   base = minutos despierto * scale / div
        // y se le restan dos cuartos al azar: queda entre la mitad y el total.
        // @0xa3a88  Tama2MovieMain_Implement::DT_SLEEP_TIME
        int awake = (DT_SLEEP_TIME[id][1] - DT_SLEEP_TIME[id][0]) * 60;
        int base = awake * DATA_PARAMETER[id][0xc] / DATA_PARAMETER[id][0x10];
        int quarter = base / 4;
        int a = nextRand();
        int b = nextRand();
        m_para[SICK_TIMER_0d] = base - quarter * a / 32768 - quarter * b / 32768;
        break;
    }

    // El timer de la disciplina: al azar, según su discipline timer.
    case 0xf: {
        // Entre la mitad y el total. Si es 0 o menos, el personaje no pide
        // disciplina y el timer no se toca.
        int total = DATA_PARAMETER[id][0x20];
        if (total > 0) {
            m_para[DISCIPLINE_TIMER_0f] = total - (total / 2) * nextRand() / 32768;
        }
        break;
    }
    }
}
