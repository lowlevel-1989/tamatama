/* platform/android/android_platform.h: lo que comparten platform.c y
   transfer.c, solo en Android. */
#pragma once

/* La carpeta interna de la app ({files}), cuando llega de Java: sin SDL
   arrancado, base_dir no la sabe. */
void android_set_base(const char *files);
