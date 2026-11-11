# android/: la app de Android

`make PLATFORM=android` construye `tamatama.apk`, sin Gradle:

1. `android/deps/`: las fuentes de SDL2, SDL2_mixer y fdk-aac, y el
   `qemu-arm` estático de Debian (paquete `qemu-user`), descargados la
   primera vez.
2. CMake con el NDK (`CMakeLists.txt`), una vez por arquitectura:
   `libSDL2.so`, `libSDL2_mixer.so` y `libtama.so`, con el backend SDL en
   GLES 1.1 y la plataforma `src/platform/android`. Android 5.0 (API 21) o
   más nuevo.
   - `armeabi-v7a`: `libtama.so` es `loader-arm`: el `.so` del juego corre
     en el mismo proceso. Para teléfonos de 32 bits.
   - `arm64-v8a` y `x86_64`: `libtama.so` es `loader` (`src/host`), y van
     con él `libloader-arm.so` (el `loader-arm` estático del PC, que
     construye el Makefile) y `libqemu-arm.so`. Para los teléfonos
     modernos, que ya no ejecutan código de 32 bits, y para emuladores y
     Chromebooks: como en el PC, el `.so` corre en `loader-arm` bajo QEMU,
     en otro proceso.
3. `javac` y `d8`: el Java de SDL y `java/` en `classes.dex`.
4. `aapt2`: `AndroidManifest.xml` y `res/`.
5. `zipalign` y `apksigner`, con la clave de depuración de siempre
   (`~/.android/debug.keystore`; si no existe, se crea).

Hace falta el SDK de Android (`ANDROID_HOME`, por defecto `~/Android/Sdk`)
con un NDK, unas build-tools (las 36 o más nuevas con un JDK reciente: el
`d8` de las 34 falla con Java 25) y una plataforma, y un JDK.

    sdkmanager "ndk;28.2.13676358" "platforms;android-34" "build-tools;36.1.0"
    make PLATFORM=android
    adb install -r tamatama.apk

## Cómo funciona

- `MainActivity` es la pantalla de inicio, hecha con vistas de Android y los
  colores del menú del PC: una fila grande por versión, para el touch, y
  "Import games". Las versiones salen de `libtama.so` (`choices`, en
  `src/platform/android/platform.c`), que reconoce cada `.apk` por su md5
  como en el PC.
- El juego no viene con la app. "Import games" abre el selector de ficheros:
  un `.zip` con los `.apk` (en la raíz o en carpetas) o un `.apk` suelto, que
  se copian a `{files}/apk/`. También importa el `.zip` o `.apk` con el que
  se abra la app desde un gestor de ficheros.
- "Transfer Save" abre `TransferActivity`: las pantallas de Transfer Save
  hechas con vistas de Android (filas grandes, como la de inicio). La red y
  las partidas son las del PC (`src/transfer`: `sender.c`, `receiver.c`),
  que llama por JNI (`src/platform/android/transfer.c`). Los colores y las
  filas que comparten las dos pantallas están en `Ui.java`.
- El icono es un huevo pixelado (`res/drawable/ic_launcher_foreground.xml`),
  adaptativo desde Android 8 (`res/mipmap-anydpi-v26`).
- Al tocar una versión, `ArenaActivity` (SDL) la arranca: `SDL_main` llama a
  `loader-arm` (32 bits) o a `loader` (64 bits) con ella, en un thread con
  un stack de 16 MiB. Va en su propio proceso (`:game`), uno nuevo por
  partida; al salir, vuelve la lista.
- En 64 bits, `loader` lanza `loader-arm --serve` bajo QEMU, o directo si la
  CPU aún ejecuta 32 bits (`--probe`). Las apps de Android pasan por un
  filtro seccomp que mata a quien use llamadas al sistema que bionic no
  usa, y glibc hace algunas al arrancar (`set_robust_list`, `rseq`,
  `clone3`). `src/host/sandbox.c` traza al hijo con `ptrace`, como PRoot, y
  las contesta con ENOSYS (o repite `access` como `faccessat`). Los
  mensajes de `loader-arm` llegan a logcat por un pipe.
- La cámara del juego guarda la captura en la carpeta de la app y la copia a
  la galería (`Pictures/Tamatama`, Android 10 o más nuevo).
- Si ningún mando vibra, vibra el teléfono (`ArenaActivity.vibrate`, por
  `platform_vibrate`), como vibración de un juego (`USAGE_GAME`). La de SDL
  cuenta como respuesta al touch, y muchos teléfonos la tienen apagada.
- En el juego, el touch es el touch: SDL convierte los toques en el ratón que
  ya lee `src/port/touch.c`. Un mando o un teclado funcionan como en el PC.
  El botón Atrás es "salir" (dos veces).
- Los mensajes van a logcat, con la etiqueta `tamatama`:
  `adb logcat -s tamatama SDL`.
