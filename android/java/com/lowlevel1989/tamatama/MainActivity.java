/* MainActivity: la pantalla de inicio de la app, con los colores del menú de
   --menu (src/shared/menu.c), pero hecha con vistas de Android: filas grandes
   para el touch, el scroll y el foco del mando o del teclado de Android.

     ARENA
     choose a game
     una fila por versión     la juega (ArenaActivity, con el extra "game")
     Transfer Save            pasa la partida a otro dispositivo
                              (TransferActivity)
     Import games             el selector de ficheros: un .zip con los .apk

   Las versiones salen de libtama (choices, en src/platform/android), que
   reconoce cada .apk por su md5 igual que en el PC. El juego no viene con la
   app: sin versiones, la pantalla explica cómo importarlas.

   Importar: lo elegido se copia en un thread aparte. Los .apk del .zip, estén
   en la raíz o en carpetas, van a {files}/apk/; un .apk suelto (un zip con
   AndroidManifest.xml) se copia tal cual. Abrir un .zip o un .apk con la app,
   desde un gestor de ficheros, importa igual. */
package com.lowlevel1989.tamatama;

import android.app.Activity;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.provider.OpenableColumns;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.Enumeration;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

public class MainActivity extends Activity {
    private static final int CREAM = Ui.CREAM, PINK = Ui.PINK, GREEN = Ui.GREEN, CORAL = Ui.CORAL,
            BLACK = Ui.BLACK;
    private static final int PICK = 1;

    static {
        System.loadLibrary("SDL2");
        System.loadLibrary("SDL2_mixer");
        System.loadLibrary("tama");
    }

    /* {nombre, juego, nombre, juego...}: src/platform/android/platform.c */
    private static native String[] choices(String files);

    private TextView subtitle, help, notice;
    private LinearLayout games;
    private View importRow;
    private View transferRow;
    private boolean busy;

    @Override
    protected void onCreate(Bundle saved) {
        super.onCreate(saved);
        buildScreen();
        Uri uri = incoming(getIntent());
        if (uri != null) importAsync(uri);
    }

    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        Uri uri = incoming(intent);
        if (uri != null) importAsync(uri);
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (!busy) refresh(null);
    }

    /* El fichero con el que se ha abierto la app, si lo hay. */
    private static Uri incoming(Intent intent) {
        if (intent == null) return null;
        if (Intent.ACTION_VIEW.equals(intent.getAction())) return intent.getData();
        if (Intent.ACTION_SEND.equals(intent.getAction())) return intent.getParcelableExtra(Intent.EXTRA_STREAM);
        return null;
    }

    /* ---- la pantalla ---- */

    private int dp(float value) { return Ui.dp(this, value); }

    private TextView text(String value, float sp, int color, boolean bold) {
        return Ui.text(this, value, sp, color, bold);
    }

    private TextView row(String value, boolean outline, View.OnClickListener action) {
        return Ui.row(this, value, outline, action);
    }

    private void buildScreen() {
        LinearLayout screen = new LinearLayout(this);
        screen.setOrientation(LinearLayout.VERTICAL);
        screen.setBackgroundColor(CREAM);
        screen.setPadding(dp(20), dp(40), dp(20), dp(24));

        TextView title = text("ARENA", 64, PINK, true);
        title.setLetterSpacing(0.12f);
        screen.addView(title, Ui.wide());
        subtitle = text("choose a game", 20, GREEN, true);
        subtitle.setPadding(0, 0, 0, dp(24));
        screen.addView(subtitle, Ui.wide());

        /* las versiones, con scroll si no caben */
        ScrollView scroll = new ScrollView(this);
        scroll.setFocusable(false);
        Ui.noFocusHighlight(scroll);
        games = new LinearLayout(this);
        games.setOrientation(LinearLayout.VERTICAL);
        help = text("", 16, BLACK, false);
        help.setLineSpacing(0, 1.3f);
        help.setPadding(dp(8), dp(24), dp(8), dp(24));
        games.addView(help, Ui.wide());
        scroll.addView(games, Ui.wide());
        screen.addView(scroll, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 0, 1));

        notice = text("", 14, CORAL, true);
        notice.setPadding(0, dp(8), 0, dp(8));
        screen.addView(notice, Ui.wide());

        transferRow = row("Transfer Save", true, v -> transfer());
        screen.addView(transferRow);
        importRow = row("Import games", true, v -> pick());
        screen.addView(importRow);
        TextView licenses = text("Licenses", 13, 0x99000000, false);
        licenses.setPadding(0, dp(10), 0, 0);
        licenses.setFocusable(true);
        licenses.setOnClickListener(v -> showLicenses());
        screen.addView(licenses, Ui.wide());

        setContentView(screen);
        Ui.creamStatusBar(this);
    }

    /* Vuelve a buscar las versiones (en un thread: calcula el md5 de cada
       .apk) y rehace la lista. message va al aviso. */
    private void refresh(String message) {
        busy = true;
        importRow.setEnabled(false);
        notice.setText(message != null ? message : "");
        String files = getFilesDir().getAbsolutePath();
        new Thread(() -> {
            String[] found;
            try {
                found = choices(files);
            } catch (RuntimeException | UnsatisfiedLinkError e) {
                found = new String[0];
            }
            final String[] list = found;
            runOnUiThread(() -> show(list));
        }).start();
    }

    private void show(String[] list) {
        busy = false;
        importRow.setEnabled(true);
        games.removeAllViews();
        int count = list.length / 2;
        if (count == 0) {
            subtitle.setText("no games yet");
            help.setText("Tamatama plays the Tamagotchi games for Android that you own.\n\n"
                    + "Import a .zip with their .apk files: in its root or in folders.");
            games.addView(help, Ui.wide());
            importRow.requestFocus();
            return;
        }
        subtitle.setText("choose a game");
        for (int i = 0; i < count; i++) {
            final String game = list[2 * i + 1];
            games.addView(row(list[2 * i], false, v -> play(game)));
        }
        games.getChildAt(0).requestFocus();
    }

    private void transfer() {
        if (!busy) startActivity(new Intent(this, TransferActivity.class));
    }

    private void play(String game) {
        if (busy) return;
        Intent intent = new Intent(this, ArenaActivity.class);
        intent.putExtra(ArenaActivity.GAME, game);
        startActivity(intent);
    }

    /* ---- licencias ---- */

    /* Las licencias de lo que lleva la app (assets/licenses, de android.mk),
       una detrás de otra, con el nombre de cada una: la de Tamatama, QEMU
       con su GPL, y las librerías. */
    private void showLicenses() {
        StringBuilder all = new StringBuilder();
        try {
            String[] names = {"tamatama.txt", "QEMU.txt", "GPL-2.0.txt", "SDL2.txt", "SDL2_mixer.txt",
                    "fdk-aac.txt", "miniz.txt", "minimp4.txt", "stb.txt"};
            for (String name : names) {
                all.append("==== ").append(name.replace(".txt", "")).append(" ====\n\n");
                try (InputStream in = getAssets().open("licenses/" + name)) {
                    java.io.ByteArrayOutputStream bytes = new java.io.ByteArrayOutputStream();
                    copy(in, bytes);
                    all.append(bytes.toString("UTF-8")).append("\n\n");
                }
            }
        } catch (IOException e) {
            all.append("cannot read the licenses");
        }
        TextView body = text(all.toString(), 11, BLACK, false);
        body.setGravity(Gravity.START);
        body.setPadding(dp(16), dp(16), dp(16), dp(16));
        ScrollView scroll = new ScrollView(this);
        scroll.setBackgroundColor(CREAM);
        scroll.addView(body);
        new android.app.AlertDialog.Builder(this)
                .setTitle("Licenses")
                .setView(scroll)
                .setPositiveButton(android.R.string.ok, null)
                .show();
    }

    /* ---- importar ---- */

    private void pick() {
        if (busy) return;
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("*/*");
        intent.putExtra(Intent.EXTRA_MIME_TYPES, new String[] {"application/zip", "application/x-zip-compressed",
                "application/vnd.android.package-archive", "application/octet-stream"});
        try {
            startActivityForResult(intent, PICK);
        } catch (RuntimeException e) {
            notice.setText("cannot open the file chooser");
        }
    }

    @Override
    protected void onActivityResult(int request, int result, Intent data) {
        super.onActivityResult(request, result, data);
        if (request == PICK && result == RESULT_OK && data != null && data.getData() != null)
            importAsync(data.getData());
    }

    private void importAsync(Uri uri) {
        busy = true;
        importRow.setEnabled(false);
        notice.setText("importing the games...");
        new Thread(() -> {
            String message;
            try {
                message = importFile(uri) > 0 ? null : "no .apk file in it";
            } catch (IOException | RuntimeException e) {
                message = "cannot import it";
            }
            final String result = message;
            runOnUiThread(() -> refresh(result));
        }).start();
    }

    private File apkDir() {
        File dir = new File(getFilesDir(), "apk");
        dir.mkdirs();
        return dir;
    }

    /* Copia lo elegido a un fichero temporal (ZipFile necesita un fichero) y
       saca de él los .apk. Devuelve cuántos ha dejado en {files}/apk/. */
    private int importFile(Uri uri) throws IOException {
        File temporary = new File(getCacheDir(), "import.zip");
        try (InputStream in = getContentResolver().openInputStream(uri);
             OutputStream out = new FileOutputStream(temporary)) {
            if (in == null) throw new IOException("cannot open the file");
            copy(in, out);
        }
        int count = 0;
        try (ZipFile zip = new ZipFile(temporary)) {
            if (zip.getEntry("AndroidManifest.xml") != null) {
                /* es un .apk suelto */
                File target = new File(apkDir(), apkName(uri));
                if (!temporary.renameTo(target)) throw new IOException("cannot keep " + target.getName());
                return 1;
            }
            /* los .apk, en la raíz o en cualquier carpeta */
            Enumeration<? extends ZipEntry> entries = zip.entries();
            while (entries.hasMoreElements()) {
                ZipEntry entry = entries.nextElement();
                String name = new File(entry.getName()).getName();
                if (entry.isDirectory() || !name.toLowerCase().endsWith(".apk") || name.startsWith(".")) continue;
                try (InputStream in = zip.getInputStream(entry);
                     OutputStream out = new FileOutputStream(new File(apkDir(), name))) {
                    copy(in, out);
                }
                count++;
            }
        } finally {
            temporary.delete();
        }
        return count;
    }

    /* El nombre del .apk elegido, o game.apk. */
    private String apkName(Uri uri) {
        try (Cursor cursor = getContentResolver().query(uri, null, null, null, null)) {
            if (cursor != null && cursor.moveToFirst()) {
                int column = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME);
                if (column >= 0) {
                    String name = new File(cursor.getString(column)).getName();
                    return name.toLowerCase().endsWith(".apk") ? name : name + ".apk";
                }
            }
        } catch (RuntimeException ignored) {
        }
        return "game.apk";
    }

    private static void copy(InputStream in, OutputStream out) throws IOException {
        byte[] buffer = new byte[1 << 16];
        for (int n; (n = in.read(buffer)) > 0;) out.write(buffer, 0, n);
    }
}
