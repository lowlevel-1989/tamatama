// Esto se llama desde init
// @0x3bd64  Tama2MovieMain_Implement::run()
//
// El loop de la lógica del Tamagotchi. Corre en su propio thread (el que crea
// OnThreadCreate), unas 16 vueltas por segundo, mientras m_loopFlag no sea 0.
//
// La lógica y el dibujo se turnan con m_endDraw. La lógica va en este
// thread, y el dibujo en step, una vez por frame, a través de repaint() ->
// ScreenCanvas::paint():
//   0  dibujado: esta vuelta puede ejecutar la lógica
//   1  solo hay que volver a dibujar, sin lógica
//   2  lógica hecha, falta dibujarla: paint la dibuja
//   3  paint está dibujando; al terminar lo deja en 0
//
//   run: m_endDraw == 0 -> lógica -> m_endDraw = 2 -> repaint
//   step -> paint: m_endDraw == 2 -> 3 -> dispMain (dibuja) -> 0
//   run, siguiente vuelta: m_endDraw == 0 -> lógica otra vez
// Si paint aún no ha dibujado (sigue en 2), la vuelta no hace lógica: solo
// vuelve a pedir el dibujo. Así nunca hay dos vueltas de lógica sin dibujar
// entre medias.
void Tama2MovieMain_Implement::run(void) {
    int iVar3;
    int *piVar4;
    uint32_t uVar5;

L_3bd8a:
    // @0xbd234  Tama2MovieMain_Implement::m_loopFlag
    // Con 0 el loop termina y la aplicación se cierra.
    m_loopFlag;
    do {
        if (m_loopFlag == '\0') {
            // @0x31100  IApplication::terminate()
            IApplication::terminate();
            return;
        }

        // @0x320f4  Tama2MovieMain_Implement::controlSleep(int)
        // sleep 62ms es decir 1000/62 = 16 veces por segundo
        controlSleep(62);

        // @0xc1f88  Tama2MovieMain_Implement::m_endDraw
        // La lógica solo si el frame anterior ya se dibujó.
        if (m_endDraw == 0) {
            // @0x32a7c  Tama2MovieMain_Implement::controlKey()
            controlKey();
            // @0xc1e74  Tama2MovieMain_Implement::m_vibWork
            
            // La vibración: m_vibWork[0] son los frames que le quedan, y m_vibWork[1] si está encendida.
            if (m_vibWork[0] > 0) {
                // m_recData[0xc]: por confirmar si es el int 0xc, la copia
                // de m_para[HUNGER_06] (m_recData[6 + i] = m_para[i], ver
                // saveRecord.c), o el byte 0xc, m_recData[3].
                if (m_recData[0xc] == 0 && m_vibWork[1] == 0) {
                    m_vibWork[1] = 1;               /* empieza a vibrar */
                }
                m_vibWork[0]--;
                if (m_vibWork[0] == 0) {
                    m_vibWork[1] = 0;               /* se acabó el tiempo: para */
                }
            }

            // @0x3b0a0  Tama2MovieMain_Implement::controlSystemEvent()
            controlSystemEvent();
            // @0x3bc7c  Tama2MovieMain_Implement::doMain()
            doMain();

            // @0xc01c4  Tama2MovieMain_Implement::m_rand
            //
            // Es el ejemplo tal cual que viene en man 3 rand 
            // https://man7.org/linux/man-pages/man3/rand.3.html#EXAMPLES
            //
            // POSIX la incluye como ejemplo para obtener la misma serie en dos máquinas distintas.
            // 
            // El valor inicial de m_rand es la semilla. Elige en qué punto del circuito se empieza. 
            // Con la misma semilla sale siempre la misma serie. Si el juego la toma de la hora al 
            // arrancar, cada partida empieza en un sitio distinto.
            //
            // next = next * 1103515245 + 12345;
            m_rand = m_rand * 0x41c64e6d + 0x3039 & 0x7fff;

            // @0xc1f9c  Tama2MovieMain_Implement::m_timer
            // Cuenta frames, sin pasar nunca a negativo, y cada 8 frames pide un gc.
            m_timer = (m_timer + 1) & 0x7fffffff;
            if ((m_timer & 7) == 0) {
                System::gc();
            }
L_3bdfa:
            // Lógica hecha: falta dibujarla.
            m_endDraw = 2;
            // @0x30f40  ScreenCanvas::repaint()
            // @0xc1fa8  Tama2MovieMain_Implement::m_canvas
            m_canvas.repaint();
        }
        // 1: solo hay que dibujar, sin lógica.
        else if (m_endDraw == 1) goto L_3bdfa;
        // Si paint ya lo dejó en 0 o lo está dibujando (3), otra vuelta.
        if (m_endDraw != 2) goto L_3bd8a;
        // Sigue sin dibujarse: lo vuelve a pedir.
        m_canvas.repaint();
    } while( true );
}

