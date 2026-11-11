// doControlIvent: la transformación (la evolución) del personaje.
//
// m_para[TRANSFORM_0e] cuenta los pasos que faltan: el huevo empieza en 2
// (segundos) y Babytchi en 60 (minutos); con -1 no hay transformación
// pendiente. Al llegar a 0, setTransform dice en qué personaje se convierte,
// e initCharacterData lo prepara.
//
// Devuelve 0 si doControlIvent tiene que terminar: la transformación en vivo,
// que se ve con su animación.

#include "../m_para.h"

int transform(int step) {
    if (m_para[TRANSFORM_0e] < 0) {
        return 1;
    }
    // Los pasos que faltan: es el printf que se ve en el log.
    printf("------------m_para[PARA_TIME_TRANSFORM]=%d", m_para[TRANSFORM_0e]);
    if (!timerDone(TRANSFORM_0e, step)) {   // helpers.c
        return 1;
    }

    restartTimeStep();
    // @0x33738  Tama2MovieMain_Implement::setTransform()
    int next = setTransform();
    if (next == m_para[ID_00]) {
        return 1;   // sigue siendo el mismo
    }

    if (m_iventMode == 0) {
        // Tiempo pasado: cambia sin animación.
        // @0x38bc4  Tama2MovieMain_Implement::initCharacterData(int, int)
        initCharacterData(1, next);
        // @0xc1b90  Tama2MovieMain_Implement::m_animNum
        m_animNum = -1;
        // @0x380ec  Tama2MovieMain_Implement::setDefaultAnim()
        setDefaultAnim();
        return 1;
    }

    // En vivo: con sonido y la animación 7 (m_gameSec pasa a 5, y mientras
    // tanto doControlIvent no hace nada).
    if (m_para[ID_00] != 0) {
        fcn.00039ac4();
    }
    // @0x32974  Tama2MovieMain_Implement::playSE(int)
    playSE(2);
    // @0x33830  Tama2MovieMain_Implement::setAnimData(int)
    setAnimData(7);
    // @0x37fb8  Tama2MovieMain_Implement::setGameSec(int)
    setGameSec(5);
    initCharacterData(1, next);
    return 0;
}
