// Esto se llama desde run(), en cada vuelta mientras se juega
// @0x38e48  Tama2MovieMain_Implement::doControlIvent(bool)
//
// Quién lo llama (r2: axt @ 0x38e48), y con qué live:
//   controlGame     desde doMain, en cada vuelta de run() jugando   1
//   controlMenu     desde doMain, en el menú                        0
//   controlTelLis,  desde controlTitle, en el título                0
//   controlMailFlg
//   fcn.00039b20    desde doBoot, startGame y controlSystemEvent:   0
//                   al arrancar o al volver al juego
//
// Una llamada no es un paso. run() da unas 16 vueltas por segundo
// (controlSleep(62), doc/code/run.c), y en cada una controlGame llama aquí.
// Cada llamada da los pasos que hagan falta para alcanzar la hora real:
//   casi siempre                          0 pasos: ya está al día
//   al pasar un minuto (un segundo huevo)  1 paso: corren los timers
//   tras 3 horas con el juego cerrado      180 pasos seguidos
//
// El reloj del Tamagotchi. Lee la hora real y avanza el reloj del juego
// hasta alcanzarla, paso a paso. En cada paso corren los timers de m_para,
// así que lo que pasó con el juego cerrado también cuenta.
//
// Cuánto tiempo real es un paso depende del personaje:
//   huevo (m_para[ID_00] == 0)  1 paso = 1 segundo
//   el resto                    1 paso = 1 minuto
// Cada timer de m_para baja 1 por paso, así que los del huevo cuentan
// segundos y los del resto, minutos:
//   huevo     m_para[TRANSFORM_0e] = 2     eclosiona a los 2 segundos
//   Babytchi  m_para[TRANSFORM_0e] = 60    evoluciona a la hora
//             siesta                       minutos 40 a 44 de su etapa
//   el resto  m_para[POOP_TIMER_0c] = 180            3 horas
//             m_para[SICK_DEATH_TIMER_11] = 360           6 horas
//             m_para[STARVE_TIMER_10] = 720           12 horas
//
// Solo el último paso, el de ahora mismo, es en vivo: m_iventMode = live. Los
// anteriores son tiempo pasado: m_iventMode = 0, sin animaciones ni sonidos.
//
// Está repartido en varios ficheros: ver README.md de esta carpeta. Los
// nombres de m_para: doc/code/m_para.h.

#include "../m_para.h"

void Tama2MovieMain_Implement::doControlIvent(bool live) {
    // @0x30100  System::gc()
    System::gc();
    readRealTime();   // clock.c

    // Ni durante una transformación ni en la sección 5.
    // @0xc1d08  Tama2MovieMain_Implement::m_transform
    // @0xc1d24  Tama2MovieMain_Implement::m_gameSec
    if (m_transform != 0 || m_gameSec == 5) {
        return;
    }

    splitRealTime();     // clock.c: m_timeSec, m_timeMin, m_timeHour
    startClock(live);    // clock.c: en hora la primera vez, y lo pendiente

    // Un paso del reloj por vuelta, hasta alcanzar la hora real.
    int step;
    do {
        step = clockStep(live);   // clock.c
        if (step < 0) {
            return;   // al día
        }
        if (m_para[FLAGS_04] & FLAG_SLEEP_100) {
            sleepingStep(step);   // sleeping.c
        }
        else if (!awakeStep(step)) {   // awake.c
            return;   // muere o se transforma en vivo
        }
    } while (step != 0);
}
