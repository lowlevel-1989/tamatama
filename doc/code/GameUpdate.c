// Este se llama desde Step
// @0x3f4f8  Game::GameUpdate()
//
// app_main llama a step una vez por frame (src/port/app.c), y step lo trae
// aquí:
//   step -> Application_Step -> GameUpdate, y después GameDraw
//
// No es la lógica del Tamagotchi: esa es run() (doc/code/run.c), en su thread,
// unas 16 veces por segundo. Esto mueve el motor: InGame::Update actualiza los
// sprites (SpriteManager::UpdateAll) y las animaciones (CMvFrameWork), e
// Input::Update la entrada.
//
// Cuenta con 60 frames por segundo, 16 ms cada uno. Si step llega tarde, da
// pasos extra para que las animaciones no vayan a cámara lenta.
//
// Los campos de Game que usa:
//   field_0x0   segundos del frame anterior   (de ftime)
//   field_0x4   milisegundos del frame anterior
//   field_0x8   dstflag del frame anterior    (no se usa para nada)
//   field_0xc   FPS de los últimos 60 frames
//   field_0x10  FPS suavizados
//   field_0x14  ms acumulados para medir los FPS
//   field_0x18  frames acumulados para medir los FPS
//   field_0x1c  retraso acumulado, en ms
//   field_0x20  pasos extra que dio el frame anterior
void Game::GameUpdate(void) {
    struct timeb now;
    int elapsed;    // ms desde el frame anterior
    int lag;        // retraso que queda por recuperar
    int frames;
    float fps;

    // Cuánto ha pasado desde el frame anterior.
    ftime(&now);
    elapsed = (now.time - this->field_0x0) * 1000 + (now.millitm - this->field_0x4);
    this->field_0x0 = now.time;
    this->field_0x4 = now.millitm;
    this->field_0x8 = now.dstflag;

    // Cada 60 frames calcula los FPS y los mezcla con los anteriores:
    // 70 % lo que había y 30 % lo nuevo, para que el valor no salte.
    this->field_0x14 += elapsed;
    frames = ++this->field_0x18;
    if (frames > 59) {
        fps = (float)frames * 1000.0f / (float)this->field_0x14;
        this->field_0x18 = 0;
        this->field_0x14 = 0;
        this->field_0xc = fps;
        this->field_0x10 = this->field_0x10 * 0.7f + fps * 0.3f;
    }

    // El paso normal del frame.
    // @0x3f6b0  InGame::Update()
    // @0xc259c  InGame::instance
    InGame::instance->Update();
    // @0x42c3c  Input::Update()
    // @0xc27ac  Input::instance
    Input::instance->Update();

    // Si el frame anterior ya dio pasos extra, el retraso no pasa de 32 ms:
    // lo que sobre se pierde y el juego no intenta ponerse al día de golpe.
    // (El original resta field_0x20 * 16 cuando field_0x20 < 1, pero
    // field_0x20 nunca es negativo, así que siempre resta 0.)
    lag = this->field_0x1c;
    if (this->field_0x20 > 0 && lag > 32) {
        this->field_0x1c = 32;
        lag = 32;
    }

    // Solo si va por debajo de 48 FPS: lo que el frame tardó de más sobre
    // 16 ms se suma al retraso.
    if (this->field_0xc < 48.0f && elapsed > 16) {
        lag += elapsed - 16;
        this->field_0x1c = lag;
    }

    // Recupera el retraso con pasos extra de 16 ms, como mucho 2 por frame.
    // Con menos de 8 ms de retraso no compensa dar otro paso.
    this->field_0x20 = 0;
    while (lag >= 8) {
        InGame::instance->Update();
        Input::instance->Update();
        this->field_0x1c -= 16;
        this->field_0x20++;
        if (this->field_0x20 > 1) break;
        lag -= 16;
    }
}
