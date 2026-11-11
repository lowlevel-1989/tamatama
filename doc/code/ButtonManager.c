// Los botones de la interfaz: ButtonManager. Cada pantalla crea los suyos
// desde un CSV (tc_*_btn.csv, MakeBtnObjLocationData_csvFileData) y cada
// imagen Update mira si el touch está dentro de alguno. Es lo que usa el
// mando (src/port/controls.c) para saber dónde tocar.
//
// Las direcciones son de Classic 1.0.2. La forma de los datos cambia según
// la versión, así que el mando no los lee a mano: llama a estas funciones.
//   versión                  botones                     cámara del sprite
//   Classic, Gen-2, L.i.f.e. en el objeto, +4, 0x4c cada  Sprite +0x30
//   Angel                    puntero en +8, 0x50 cada     Sprite +0x40
// En Angel el número de botones está en +4 (en las demás, en +0) y la
// posición del sprite se calcula con sus padres (Sprite::GetCalcPos).

// @0xc25a0  ButtonManager::instance

// Un botón, por su número: NULL fuera de rango (0 a 99).
// @0x3f77c  ButtonManager::GetOneBtnParam(int)
ONE_BTN_PARAM *ButtonManager::GetOneBtnParam(int i) {
    return i < 100 ? &this->buttons[i] : NULL;   // +4 + i * 0x4c
}

// @0x3f814  ButtonManager::GetButtonNum()
int ButtonManager::GetButtonNum(void) {
    return this->count;   // +0
}

// Visible y habilitado: los dos primeros bytes del botón.
// @0x40ad8  ButtonManager::IsVisible(int)
// @0x40aec  ButtonManager::IsEnabled(int)
bool ButtonManager::IsVisible(int i) {
    return GetOneBtnParam(i)->visible;   // +0
}
bool ButtonManager::IsEnabled(int i) {
    return GetOneBtnParam(i)->enabled;   // +1
}

// El centro del sprite del botón y el tamaño de su textura, en la vista de
// su cámara.
// @0x409c0  ButtonManager::GetSpriteViewParam(int, int, float*, float*,
//           float*, float*)
bool ButtonManager::GetSpriteViewParam(int i, int s, float *x, float *y,
                                       float *w, float *h) {
    // @0x3fdfc  ButtonManager::GetSprite(int, int)
    Sprite *sprite = GetSprite(i, s);
    if (!sprite) return false;
    *x = sprite->x;   // +0x10
    *y = sprite->y;   // +0x14
    // @0x437f4  SpriteManager::GetTexSize(int, float*, float*)
    SpriteManager::instance->GetTexSize(sprite->texture, w, h);   // +0x1c
    return true;
}

// El toque: el primer botón visible y habilitado que tiene el touch dentro.
// Devuelve su número, o -1.
// @0x3ff38  ButtonManager::Update(bool*, int*)
int ButtonManager::Update(bool *pushed, int *hit) {
    for (int i = 0; i < GetButtonNum(); i++) {
        if (!IsVisible(i) || !IsEnabled(i)) continue;
        Sprite *sprite = GetSprite(i, 0);
        float w, h;
        SpriteManager::instance->GetTexSize(sprite->texture, &w, &h);

        // Sin cámara (-1), el toque tal cual; con cámara, pasado a su vista.
        Vector2D touch = sprite->camera == -1                        // +0x30
            ? Input::GetTouchPos(0, 0)
            : Input::GetTouch_CameraView(sprite->camera, 0);

        if (sprite->x - w / 2 < touch.x && touch.x < sprite->x + w / 2 &&
            sprite->y - h / 2 < touch.y && touch.y < sprite->y + h / 2) {
            *hit = i;
            ...   // pulsado, soltado, radio: cambia el sprite y suena
        }
    }
    return -1;
}

// De la pantalla a la vista de una cámara: lo que el mando deshace para
// saber dónde dibujar el puntero y dónde tocar.
//   vista = cámara.pos + toque * cámara.tamaño * escala / pantalla real
// La escala (+0x24 y +0x28 de la cámara) solo existe en Angel; en las demás
// es 1. La pantalla real es el tamaño que recibió init (320x480).
// @0x42ec8  Input::GetTouch_CameraView(int, int)
Vector2D Input::GetTouch_CameraView(int camera, int finger) {
    Vector2D touch = GetTouchPos(finger, 1);
    // @0x3eea8  Camera::GetCameraParameter(int)
    CameraParameter p = Camera::instance->GetCameraParameter(camera);
    float real_w, real_h;
    // @0x3edb8  Camera::GetRealScreenSize(float*, float*)
    Camera::instance->GetRealScreenSize(&real_w, &real_h);
    return Vector2D(p.x + touch.x * p.w / real_w,    // +0x0, +0x8
                    p.y + touch.y * p.h / real_h);   // +0x4, +0xc
}

// Las cámaras que se ven en las pantallas de juego:
//   -1  sin cámara: coordenadas de pantalla (el título de Classic)
//   3   la interfaz: 640x960, el doble de la pantalla
//   5   el aparato: también 640x960
//
// Los botones A, B y C del aparato son los mismos en todas las versiones
// (tc_game_retro_btn.csv, en 640x960): los de los lados a la misma altura y
// el del centro algo más abajo. Por esa forma los reconoce el mando.
//   TBTNID_LEFT_BTN    155,622  64x64
//   TBTNID_CENTER_BTN  268,639  64x64
//   TBTNID_RIGHT_BTN   380,622  64x64

// Los diálogos (el aviso de Original Manual en Help, Ok y Back) no usan
// ButtonManager::instance: tienen su propio ButtonManager, DialogManager,
// que hereda de él (UpdatetDlg llama a ButtonManager::Update con su this).
// Mientras se ve, los botones de debajo siguen visibles y habilitados, pero
// el juego no los atiende. Está igual en las cinco versiones.
// @0xc25a4  DialogManager::instance
// @0x41690  DialogManager::IsVisibletDlg()
// Se ve si se ve su sprite: el byte +0x24 del sprite cuyo número guarda en
// +0x1df4.
bool DialogManager::IsVisibletDlg(void) {
    Sprite *sprite = SpriteManager::instance->GetSprite(this->sprite);
    return sprite ? sprite->visible : false;
}
// @0x416bc  DialogManager::UpdatetDlg()
void DialogManager::UpdatetDlg(void) {
    if (IsVisibletDlg() && CMvFrameWork::AppRequest_GetRequest() == 3)
        CMvFrameWork::AppRequest_SetRequest(0);
    ButtonManager::Update(NULL, NULL);
}

// Qué pantalla es: cada una crea sus botones desde CSV (tc_*btn*.csv), un
// botón por línea, seguidos en el orden de lectura: en Shells, los iconos de
// arriba (tc_common_icon_btn.csv) del 0 al 5, las flechas
// (tc_library_pageselLR_btn.csv) 6 y 7, Back 8 y las miniaturas del 9 al 12.
// GetButtonNum cuenta además botones que no salen de ningún CSV. El mando
// (src/port/controls.c) mira qué CSV lee el juego y navega cada uno con una
// tabla escrita a mano (src/port/controls_table.h).
//
// El aviso de Login Bonus de Angel no usa ninguno de los dos managers: es un
// LoginBouns, miembro de la pantalla del título y de la de Options, con su
// propio ButtonManager dentro. Sus 9 botones: los 8 sellos (con la primera
// línea de tc_login_btn.csv) y close, el 8.
//   CTcTitlePart  +0x16ac  LoginBouns     CTcOptionPart  +0x1278  LoginBouns
//   LoginBouns    +4       ButtonManager  +0x508  número del primer botón
// @0x5e954  LoginBouns::ConvLocalToGloalBtnId(int)   (Angel)
int LoginBouns::ConvLocalToGloalBtnId(int local) {
    return local + this->first_button;   // +0x508
}
// La pantalla actual: mvsp::exec::update llama a la que hay en una variable
// sin nombre (la misma secuencia de instrucciones en las cinco versiones).
// @0x4e948  mvsp::exec::update()   (Angel)
void mvsp::exec::update(void) {
    current_executor->update();   // vtable +0xc
}

// La captura de pantalla: el icono de la cámara del juego. Las cinco
// versiones lo tienen.
// @0x46ed8  CTcGameMainPart::processCapture()
// Una máquina de estados, de 0 a 3, que se mira en cada imagen: el 1 es
// justo después de StartCapture(1), el 2 cuenta hasta 0x5d (unas 93
// imágenes) y el 3 llama a DeviceImplement::SelectSNS. Los nombres de sus
// campos no se conocen.
// DeviceImplement::StartCapture(int) llama a Java OnStartCapture(I)V y
// DeviceImplement::SelectSNS() a OnSelectSNS()Z: Android hace la foto y
// ofrece compartirla en una red social (SNS, Social Networking Service).
// src/port/natives/screen.c guarda la imagen en screenshots/ y no comparte.
