/* ArenaActivity: el juego. Es la actividad de SDL: carga SDL2, SDL2_mixer y
   tama (loader-arm o loader, según la arquitectura), y SDL llama a SDL_main
   (src/platform/android) con la versión que eligió MainActivity (el extra
   "game": un .apk o un .so ya extraído) y la carpeta de las librerías.

   Va en su propio proceso (":game" en el manifest): el .so del juego se carga
   una sola vez por proceso, así que cada partida empieza en uno nuevo, y al
   terminar el proceso se cierra y queda MainActivity.

   toGallery, para platform_screenshot_saved: copia una captura del juego a
   la galería (Pictures/Tamatama).
   vibrate, para platform_vibrate: vibra el teléfono como vibración de un
   juego (USAGE_GAME). La de SDL no dice para qué es, Android la toma por
   respuesta al touch, y si esa está apagada en los ajustes no vibra.
   openUrl, para platform_open_url: abre una página en el navegador (el
   botón "Official site" lleva a la de Tamatama). */
package com.lowlevel1989.tamatama;

import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Context;
import android.content.Intent;
import android.media.AudioAttributes;
import android.net.Uri;
import android.os.Build;
import android.os.VibrationEffect;
import android.os.Vibrator;
import android.provider.MediaStore;

import org.libsdl.app.SDL;
import org.libsdl.app.SDLActivity;

import java.io.File;
import java.io.FileInputStream;
import java.io.InputStream;
import java.io.OutputStream;

public class ArenaActivity extends SDLActivity {
    static final String GAME = "game";
    @Override
    protected String[] getLibraries() {
        return new String[] {"SDL2", "SDL2_mixer", "tama"};
    }

    @Override
    protected String[] getArguments() {
        /* la versión y dónde están libloader-arm.so y libqemu-arm.so */
        String game = getIntent().getStringExtra(GAME);
        String libs = getApplicationInfo().nativeLibraryDir;
        return game != null ? new String[] {game, libs} : new String[0];
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        /* el .so del juego no se descarga: el proceso entero fuera */
        if (isFinishing()) System.exit(0);
    }

    /* Abre url en el navegador: true si hay con qué. */
    public static boolean openUrl(String url) {
        Context context = SDL.getContext();
        if (context == null) return false;
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(url));
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        try {
            context.startActivity(intent);
            return true;
        } catch (RuntimeException e) {
            return false;
        }
    }

    /* Vibra ms milisegundos: true si el teléfono tiene vibrador. */
    public static boolean vibrate(int ms) {
        Context context = SDL.getContext();
        Vibrator vibrator = context != null ? (Vibrator) context.getSystemService(Context.VIBRATOR_SERVICE) : null;
        if (vibrator == null || !vibrator.hasVibrator()) return false;
        AudioAttributes game = new AudioAttributes.Builder().setUsage(AudioAttributes.USAGE_GAME).build();
        if (Build.VERSION.SDK_INT >= 26)
            vibrator.vibrate(VibrationEffect.createOneShot(ms, VibrationEffect.DEFAULT_AMPLITUDE), game);
        else
            vibrator.vibrate(ms, game);
        return true;
    }

    /* La captura en path, a Pictures/Tamatama con MediaStore: sin permisos
       desde Android 10. En los anteriores se queda en la carpeta de la app. */
    public static boolean toGallery(String path) {
        if (Build.VERSION.SDK_INT < 29 || SDL.getContext() == null) return false;
        ContentResolver resolver = SDL.getContext().getContentResolver();
        ContentValues values = new ContentValues();
        values.put(MediaStore.Images.Media.DISPLAY_NAME, "tamatama_" + new File(path).getName());
        values.put(MediaStore.Images.Media.MIME_TYPE, "image/png");
        values.put(MediaStore.Images.Media.RELATIVE_PATH, "Pictures/Tamatama");
        Uri uri = resolver.insert(MediaStore.Images.Media.EXTERNAL_CONTENT_URI, values);
        if (uri == null) return false;
        try (InputStream in = new FileInputStream(path); OutputStream out = resolver.openOutputStream(uri)) {
            if (out == null) return false;
            byte[] buffer = new byte[1 << 16];
            for (int n; (n = in.read(buffer)) > 0;) out.write(buffer, 0, n);
            return true;
        } catch (Exception e) {
            resolver.delete(uri, null, null);
            return false;
        }
    }
}
