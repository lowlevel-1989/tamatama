// Esto se llama desde doControlIvent (doControlIvent/transform.c), cuando
// TRANSFORM_0e llega a 0
// @0x33738  Tama2MovieMain_Implement::setTransform()
//
// El árbol de evolución: en qué personaje se convierte, según los fallos de
// la etapa que termina. Devuelve el id nuevo; si es el mismo, no cambia.
//   care = CARE_MISS_18         fallos de cuidado
//   disc = DISCIPLINE_MISS_19   fallos de disciplina
// Los adultos no cambian, salvo Maskutchi (9), que pasa a Bill/Oyajitchi (13)
// con 7 días o más en la etapa, menos de 6 fallos de cuidado y ninguno de
// disciplina.

#include "m_para.h"

int Tama2MovieMain_Implement::setTransform(void) {
    // @0xc1ba8  m_para
    int id = m_para[ID_00];
    int care = m_para[CARE_MISS_18];
    int disc = m_para[DISCIPLINE_MISS_19];

    switch (id) {
    // Huevo -> Babytchi.
    case 0:
        return 1;

    // Babytchi -> Marutchi.
    case 1:
        return 2;

    // Marutchi: buen cuidado, Tamatchi; mal cuidado, Kuchitamatchi. La
    // disciplina elige cuál de los dos.
    case 2:
        if (care < 3) return disc < 2 ? 3 : 4;
        return disc < 2 ? 5 : 6;

    // Tamatchi (3).
    case 3:
        if (care < 3) {
            if (disc == 0) return 7;   // Mametchi
            if (disc == 1) return 8;   // Ginjirotchi
            return 9;                  // Maskutchi
        }
        if (disc < 2) return 10;       // Kuchipatchi
        if (disc < 4) return 11;       // Nyorotchi
        return 12;                     // Tarakotchi

    // Tamatchi (4).
    case 4:
        if (care < 4) return disc < 2 ? 8 : 9;   // Ginjirotchi, Maskutchi
        return disc < 8 ? 11 : 12;               // Nyorotchi, Tarakotchi

    // Kuchitamatchi (5).
    case 5:
        if (care < 3) {
            if (disc < 2) return 10;   // Kuchipatchi
            if (disc == 2) return 11;  // Nyorotchi
            return 12;                 // Tarakotchi
        }
        return disc < 4 ? 11 : 12;     // Nyorotchi, Tarakotchi

    // Kuchitamatchi (6).
    case 6:
        if (care < 3) {
            if (disc == 0) return 10;  // Kuchipatchi
            if (disc == 1) return 11;  // Nyorotchi
            return 12;                 // Tarakotchi
        }
        return disc < 6 ? 11 : 12;     // Nyorotchi, Tarakotchi

    // Maskutchi: la evolución especial.
    case 9:
        if (m_para[STAGE_DAYS_21] > 6 && care < 6 && disc == 0) {
            return 13;                 // Bill / Oyajitchi
        }
        return id;

    // Los demás adultos (7, 8, 10 a 13) no cambian.
    case 7: case 8: case 10: case 11: case 12: case 13:
        return id;

    default:
        return 0;
    }
}
