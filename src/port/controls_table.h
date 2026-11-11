/* Cómo se navega cada pantalla del juego con el mando (controls.c). Cada
   pantalla crea sus botones desde un CSV (tc_*btn*.csv), uno por línea y
   con su nombre en la primera columna; aquí van esos nombres, agrupados en
   filas separadas por "/". Arriba y abajo cambian de fila; izquierda y
   derecha se mueven dentro de ella. Las filas se ordenan por su altura en
   la pantalla y cada fila de izquierda a derecha, con las posiciones que da
   el juego, así que un CSV vale para todas las versiones aunque cambie la
   colocación (la de los iconos de arriba cambia en Angel).

   Además de nombres:
     *        el centro de la pantalla ("TOUCH TO START" en el título)
     @nombre  donde empieza el puntero al entrar (si no, en el primero)
     <nombre  la flecha de página anterior: izquierda al final de la fila
     >nombre  la de la siguiente: derecha al final de la fila
     ~        izquierda y derecha al final de la fila arrastran la pantalla
              (el manual original no tiene flechas)
     1=nombre los botones A, B y C del aparato: 1, 2 y 3 los pulsan
   Los botones que el juego tiene ocultos o deshabilitados se saltan. Un
   CSV que no está aquí se navega como una columna, en el orden del
   fichero. */

/* El título y los avisos del principio. */
SCREEN("tc_title_btn.csv", "@* / SBTNID_OPTION SBTNID_HELP / SBTNID_ADFREE SBTNID_RESTORE / SBTNID_LIFE / "
                           "SBTNID_FB_Apprequests")
SCREEN("tc_title_btn_with_FB.csv", "@* / SBTNID_OPTION SBTNID_HELP / SBTNID_ADFREE SBTNID_RESTORE / SBTNID_LIFE / "
                                   "SBTNID_FB_Apprequests")
SCREEN("tc_title_adree_dlg_btn.csv", "SBTNID_YES SBTNID_NO")
SCREEN("tc_title_cm_btn.csv", "SBTNID_OK SBTNID_CANCEL")
SCREEN("tc_agreement_dialog_btn.csv", "SBTNID_AGREEMENT_HTML / @SBTNID_OK")
/* El Login Bonus de Angel: el sello ganado (TBTNID_BOUNSE) y close. */
SCREEN("tc_login_btn.csv", "TBTNID_BOUNSE / @TBTNID_CLOSE")

/* Options. */
SCREEN("tc_option_btn.csv", "SBTNID_BGM_ON SBTNID_BGM_OFF / SBTNID_SE_ON SBTNID_SE_OFF / SBTNID_PUSH_ON SBTNID_PUSH_OFF / "
                            "SBTNID_FACEBOOK_IN SBTNID_FACEBOOK_OUT / SBTNID_RESET / SBTNID_BACK")
SCREEN("tc_option_with_LoginBonus_btn.csv",
       "SBTNID_BGM_ON SBTNID_BGM_OFF / SBTNID_SE_ON SBTNID_SE_OFF / SBTNID_PUSH_ON SBTNID_PUSH_OFF / "
       "SBTNID_FACEBOOK_IN SBTNID_FACEBOOK_OUT / SBTNID_RESET SBTNID_LOGINBOUNS / SBTNID_BACK")
SCREEN("tc_option_dialog_btn.csv", "SBTNID_YES SBTNID_NO")
SCREEN("tc_msel_btn.csv", "TBTNID_DLG_OLD / TBTNID_DLG_NEW / TBTNID_DLG_BACK / SBTNID_YES SBTNID_NO")
SCREEN("tc_reset_msel_btn.csv", "TBTNID_DLG_OLD / TBTNID_DLG_NEW / TBTNID_DLG_BACK")
SCREEN("tc_reset_mselk_btn.csv", "SBTNID_YES SBTNID_NO")

/* Help: el tutorial pasa página con sus flechas y el manual original
   (howtoplay) arrastrando; el puntero se queda en Back. */
SCREEN("tc_help_btn.csv", "TBTNID_TUTORIAL / TBTNID_HOWTOPLAY / TBTNID_FAQ / TBTNID_BACK")
SCREEN("tc_help_with_agreement_btn.csv",
       "TBTNID_TUTORIAL / TBTNID_HOWTOPLAY / TBTNID_FAQ / TBTNID_AGREEMENT / TBTNID_BACK")
SCREEN("tc_help_howtoplay_dlgbtn.csv", "SBTNID_YES SBTNID_NO")
SCREEN("tc_tutorial_btn.csv", "<SBTNID_LEFT >SBTNID_RIGHT TBTNID_BACK SBTNID_BACK SBTNID_START")
SCREEN("tc_howtoplay_btn.csv", "~ TBTNID_BACK")
SCREEN("tc_faq_btn.csv", "TBTNID_BACK")

/* El juego: los iconos de arriba, el aparato y los iconos de cuidado
   alrededor de su pantalla (el modo APP, y en Angel también el aparato). */
SCREEN("tc_common_icon_btn.csv",
       "TBTNID_TOP_BTN TBTNID_GAME_BTN TBTNID_ZUKAN_BTN TBTNID_CHANGE_BTN TBTNID_SNS_BTN / SBTNID_RESET")
SCREEN("tc_game_retro_btn.csv", "1=TBTNID_LEFT_BTN 2=TBTNID_CENTER_BTN 3=TBTNID_RIGHT_BTN")
SCREEN("tc_game_ima_btn.csv", "TBTNID_SEG_ICON_00000 TBTNID_SEG_ICON_00001 TBTNID_SEG_ICON_00002 TBTNID_SEG_ICON_00003 / "
                              "TBTNID_SEG_ICON_00004 TBTNID_SEG_ICON_00005 TBTNID_SEG_ICON_00006 TBTNID_SEG_ICON_00007")
/* En Angel, zonas táctiles sobre los iconos de cuidado de la pantalla del
   aparato: no se ven como botones y para eso están 1, 2 y 3. Fuera. */
SCREEN("tc_game_retro_osewabtn.csv", "")
SCREEN("tc_game_ima_food_dialog_btn.csv", "SBTNID_MEAL / SBTNID_SNACK")
SCREEN("tc_game_ima_light_dialog_btn.csv", "SBTNID_ON / SBTNID_OFF")
SCREEN("tc_game_reset_dialog_btn.csv", "SBTNID_YES SBTNID_NO")
SCREEN("tc_game_mg1_btn.csv", "TBTNID_MG1_ROCK TBTNID_MG1_SCISSORS TBTNID_MG1_PAPER")
SCREEN("tc_game_mg1_help_dialog_btn.csv", "SBTNID_OK")

/* La galería (zukan): Tamagotchis, Wallpaper y Shells, con sus páginas. */
SCREEN("tc_library_top_btn.csv", "TBTNID_TAMAGOTCHI / TBTNID_WALLPAPER / TBTNID_BODY / TBTNID_BACK")
SCREEN("tc_library_msel_btn.csv", "TBTNID_TAMAGOTCHI_OLD / TBTNID_TAMAGOTCHI_NEW / TBTNID_BACK")
SCREEN("tc_library_tamasel_btn.csv", "TBTNID_ITEM0 TBTNID_ITEM1 TBTNID_ITEM2 / TBTNID_ITEM3 TBTNID_ITEM4 TBTNID_ITEM5")
SCREEN("tc_library_tamadetail_btn.csv", "TBTNID_ITEM0 TBTNID_ITEM1 TBTNID_ITEM2")
SCREEN("tc_library_bodysel_btn.csv", "TBTNID_ITEM0 TBTNID_ITEM1 / TBTNID_ITEM2 TBTNID_ITEM3")
SCREEN("tc_library_bodysel_back_btn.csv", "~ TBTNID_BACK")
SCREEN("tc_library_pageselLR_btn.csv", "<TBTNID_LEFT >TBTNID_RIGHT")
SCREEN("tc_library_dialog_btn.csv", "SBTNID_YES SBTNID_NO")
