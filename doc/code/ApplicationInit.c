// @0x2cf44  Application_Init
void Application_Init(int arg_width, int arg_height) {
    Camera *camera;
    float f_game_width;
    float f_game_height;
    int game;
    int iVar1;
    int32_t iVar3;
    float f_width;
    float f_height;
    float width_scale;
    int game_width;
    int game_height;

    // @0x2ce9c  Device_PrintMessage
    Device_PrintMessage("Application_Init width=%d height=%d", arg_width, arg_height);
    
    // @0xbd01c  g_real_width
    g_real_width = arg_width;
    // @0xbd020  g_real_height
    g_real_height = arg_height;
    // @0x3f454  Game::GetInstance()
    game = Game::GetInstance();
    if (game == 0) {
        // @0x3f408  Game::CreateInstance()
        Game::CreateInstance();
        game = Game::GetInstance();
        if (game != 0) {
            // @0x3f460  Game::GameInit()
            game->GameInit();
            // @0xc2594  Camera::instance
            if (Camera::instance != 0) {
                f_width = (float)arg_width;
                f_height = (float)arg_height;
                camera = Camera::instance;
                // @0x3ee34  Camera::SetRealScreenSize(float, float)
                camera->SetRealScreenSize(f_width, f_height);
                width_scale = 320.0f / f_width;
                // @0xbd010  g_screen_scale
                g_screen_scale = width_scale;
                scaled_height = f_height * width_scale;

                if (scaled_height >= 480.0f) {
                    game_height = (int)scaled_height;
                    game_width = 320;
                    Device_PrintMessage("Application_Init witdh_base");
                }
                else {
                    height_scale = 480.0f / f_height;
                    g_screen_scale = height_scale;
                    scaled_width = f_width * height_scale;
                    game_width = (int)scaled_width;
                    Device_PrintMessage("Application_Init height_base");
                    game_height = 480;
                }
                Device_PrintMessage("Application_Init openGL width=%d height=%d", game_width, game_height);

                f_game_width  = (float)game_width;
                f_game_height = (float)game_height;
                // @0x3edd0  Camera::SetBaseScreenSize(float, float)
                camera->SetBaseScreenSize(f_game_width, f_game_height);

                DAT_bd014 = game_width;
                DAT_bd018 = game_height;

                // @0x3f1c4  Camera::UseUICamera(bool)
                camera->UseUICamera(0);
                // @0x3ece4  Camera::SetScreenSize(float, float)
                camera->SetScreenSize(f_game_width, f_game_height);
                // @0x3ee7c  Camera::SetUICameraParam(CameraParam)
                Camera::SetUICameraParam(camera, 0, 0, f_game_width);
            }
        }
        // @0x44cec  IsExist_MV_FADE_MNG()
        tmp = IsExist_MV_FADE_MNG();
        if (tmp != 0) {
            // @0x44d00  CMvFadeMng::Init()
            // @0xc27c0  CMvFadeMng::instance
            CMvFadeMng::instance->Init();
            CMvFadeMng::instance->field_0x20 += 1;
        }
    }
    // @0x2f74c  Application::getInstance()
    // vcall[0] -> @0x2f684  Application_Implement::init()
    Application::getInstance()->vcall_0x0();
}

