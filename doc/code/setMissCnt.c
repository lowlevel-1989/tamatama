// Esto se llama desde doControlIvent y setCarePlayResult, y seguramente desde
// los cuidados (comida, luz, disciplina)
// @0x33538  Tama2MovieMain_Implement::setMissCnt(int, int)
//
// Cierra una llamada del personaje: borra su flag y su plazo, y dice si fue
// un fallo. call es la flag de la llamada; how, cómo se cerró:
//   0   atendida a tiempo: no es un fallo
//   1   sin atender, pero la llamada sigue abierta: cuenta el fallo
//   2   sin atender y se cierra: cuenta el fallo
//
//   llamada     plazo               fallo
//   hambre      HUNGER_CALL_14      CARE_MISS_18
//   felicidad   HAPPY_CALL_15       CARE_MISS_18
//   luz         LIGHT_CALL_17       CARE_MISS_18
//   disciplina  DISCIPLINE_CALL_16  DISCIPLINE_MISS_19
// Con Babytchi (id 1) los fallos no cuentan. Si la llamada no está abierta,
// no hace nada.

#include "m_para.h"

void Tama2MovieMain_Implement::setMissCnt(int call, int how) {
    // @0xc1ba8  m_para
    if ((m_para[FLAGS_04] & call) == 0) {
        return;   // esa llamada no está abierta
    }

    switch (call) {
    case FLAG_HUNGER_CALL_10000:   // hambre
        if (how == 1) break;
        // Una sola vez por partida: la primera llamada de hambre que se
        // cierra, atendida o no, cierra también la de felicidad, sin fallo
        // por ella. La flag 0x400 lo apunta, y solo se borra al empezar una
        // partida nueva (initCharacterData).
        if ((m_para[FLAGS_04] & 0x400) == 0) {
            m_para[FLAGS_04] = (m_para[FLAGS_04] & ~FLAG_HAPPY_CALL_20000) | 0x400;
            m_para[HAPPY_CALL_15] = -1;
        }
        m_para[FLAGS_04] &= ~FLAG_HUNGER_CALL_10000;
        m_para[HUNGER_CALL_14] = -1;
        break;

    case FLAG_HAPPY_CALL_20000:   // felicidad
        if (how == 1) break;
        m_para[FLAGS_04] &= ~FLAG_HAPPY_CALL_20000;
        m_para[HAPPY_CALL_15] = -1;
        break;

    case FLAG_LIGHT_CALL_40000:   // luz
        if (how == 1) break;
        m_para[FLAGS_04] &= ~FLAG_LIGHT_CALL_40000;
        m_para[LIGHT_CALL_17] = -1;
        break;

    // La disciplina cuenta en DISCIPLINE_MISS_19, no en CARE_MISS_18.
    case FLAG_DISCIPLINE_CALL_80000:
        if (how != 1) {
            m_para[FLAGS_04] &= ~FLAG_DISCIPLINE_CALL_80000;
            m_para[DISCIPLINE_CALL_16] = -1;
            if (how < 1) return;
        }
        if (m_para[ID_00] != 1) {
            m_para[DISCIPLINE_MISS_19] += 1;
        }
        return;

    default:
        return;
    }

    // Un fallo de cuidado.
    if (how >= 1 && m_para[ID_00] != 1) {
        m_para[CARE_MISS_18] += 1;
    }
}
