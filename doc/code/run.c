// @0x3bd64  Tama2MovieMain_Implement::run()
void Tama2MovieMain_Implement::run(void) {
    char cVar1;
    int iVar3;
    int *piVar4;
    uint32_t uVar5;

L_3bd8a:
    // @0xbd234  Tama2MovieMain_Implement::m_loopFlag
    cVar1 = m_loopFlag;
    do {
        if (cVar1 == '\0') {
            // @0x31100  IApplication::terminate()
            IApplication::terminate();
            return;
        }
        // @0x320f4  Tama2MovieMain_Implement::controlSleep(int)
        controlSleep(0x3e);
        // @0xc1f88  Tama2MovieMain_Implement::m_endDraw
        iVar3 = m_endDraw;
        if (iVar3 == 0) {
            // @0x32a7c  Tama2MovieMain_Implement::controlKey()
            controlKey();
            // @0xc1e74  Tama2MovieMain_Implement::m_vibWork
            piVar4 = &m_vibWork;
            if (*piVar4 > 0) {
                // @0xc1d50  Tama2MovieMain_Implement::m_recData
                if ((*(&m_recData + 0xc) == 0) && (piVar4[1] == 0)) {
                    piVar4[1] = 1;
                }
                iVar3 = *piVar4 - 1;
                *piVar4 = iVar3;
                if (iVar3 == 0) {
                    piVar4[1] = 0;
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
            uVar5 = m_timer + 1;
            m_timer = uVar5 & 0x7fffffff;
            if ((uVar5 & 7) == 0) {
                // @0x30100  System::gc()
                System::gc();
            }
L_3bdfa:
            m_endDraw = 2;
            // @0x30f40  ScreenCanvas::repaint()
            // @0xc1fa8  Tama2MovieMain_Implement::m_canvas
            m_canvas.repaint();
            iVar3 = m_endDraw;
        }
        else if (iVar3 == 1) goto L_3bdfa;
        if (iVar3 != 2) goto L_3bd8a;
        m_canvas.repaint();
        cVar1 = m_loopFlag;
    } while( true );
}

