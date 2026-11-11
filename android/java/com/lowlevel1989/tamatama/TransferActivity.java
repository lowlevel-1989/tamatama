/* TransferActivity: Transfer Save en Android, con vistas de Android como la
   pantalla de inicio (filas grandes para el touch). Las pantallas son las
   mismas que en el PC (src/transfer/screens.c), una función show* cada una:

     showHome        Send Save, Receive Save, Restore Backup
     showVersions    elegir la versión (con partida, o con backup)
     showDevices     a quién mandarla: los que están en Receive Save
     showSending     esperando a que el otro pulse Accept; Cancel
     showReceive     esperando una partida: este dispositivo y su firma
     showAsk         llegó una oferta: Accept o Decline
     showReceiving   tras Accept, mientras llega la partida
     showRestore     volver a la partida de antes de la última recibida
     showResult      cómo terminó, con OK

   La red y las partidas son las de src/transfer, en C (sender.c,
   receiver.c), por src/platform/android/transfer.c. Mientras hay un envío o
   una recepción abiertos, tick llama a sendPoll o receivePoll cada TICK_MS
   en el thread de la interfaz.

   El bloqueo de multicast, mientras está abierta: sin él, muchos teléfonos
   tiran los broadcast que llegan por Wi-Fi. */
package com.lowlevel1989.tamatama;

import android.app.Activity;
import android.content.Context;
import android.net.wifi.WifiManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.text.TextUtils;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

public class TransferActivity extends Activity {
    private static final int TICK_MS = 50;

    static {
        System.loadLibrary("SDL2");
        System.loadLibrary("SDL2_mixer");
        System.loadLibrary("tama");
    }

    /* src/platform/android/transfer.c */
    private static native void start(String files);
    private static native String[] versions(boolean backup);
    private static native boolean restore(String md5);
    private static native String[] sendOpen();
    private static native void sendClose();
    private static native boolean sendPoll();
    private static native String[] sendDevices();
    private static native String sendStart(String signature, String md5);
    private static native String sendResult();
    private static native void sendCancel();
    private static native String[] receiveOpen();
    private static native void receiveClose();
    private static native void receiveListen(boolean on);
    private static native int receivePoll();
    private static native String[] receiveOffer();
    private static native void receiveAccept();
    private static native void receiveDecline();
    private static native void receiveCancel();
    private static native String receiveResult();
    private static native String receiveNotice();

    /* Lo que devuelve receivePoll: src/transfer/receiver.h. */
    private static final int OFFER = 1, CANCELLED = 2, DONE = 3;

    /* Qué hace tick y qué hace volver. */
    private enum Mode { HOME, VERSIONS, DEVICES, SENDING, RECEIVE, ASK, RECEIVING, RESTORE, RESULT }

    private final Handler handler = new Handler(Looper.getMainLooper());
    private final Runnable tick = this::tick;
    private WifiManager.MulticastLock multicast;
    private TextView subtitle, notice;
    private LinearLayout content;
    private Mode mode = Mode.HOME;
    private boolean sending, receiving; /* sendOpen o receiveOpen abiertos */
    private String me, signature;       /* los de este dispositivo */
    private String version, md5, summary; /* la versión elegida */
    private String shownDevices = "";
    private Runnable afterResult;

    @Override
    protected void onCreate(Bundle saved) {
        super.onCreate(saved);
        start(getFilesDir().getAbsolutePath());
        WifiManager wifi = (WifiManager) getApplicationContext().getSystemService(Context.WIFI_SERVICE);
        if (wifi != null) {
            multicast = wifi.createMulticastLock("tamatama");
            multicast.setReferenceCounted(false);
            multicast.acquire();
        }
        buildScreen();
        showHome();
    }

    @Override
    protected void onDestroy() {
        handler.removeCallbacks(tick);
        closeAll();
        if (multicast != null && multicast.isHeld()) multicast.release();
        super.onDestroy();
    }

    /* ---- la pantalla ---- */

    private int dp(float value) { return Ui.dp(this, value); }

    private void buildScreen() {
        LinearLayout screen = new LinearLayout(this);
        screen.setOrientation(LinearLayout.VERTICAL);
        screen.setBackgroundColor(Ui.CREAM);
        screen.setPadding(dp(20), dp(40), dp(20), dp(24));
        screen.addView(Ui.text(this, "Transfer Save", 36, Ui.PINK, true), Ui.wide());
        subtitle = Ui.text(this, "", 20, Ui.GREEN, true);
        subtitle.setPadding(0, dp(4), 0, dp(20));
        screen.addView(subtitle, Ui.wide());
        ScrollView scroll = new ScrollView(this);
        scroll.setFocusable(false);
        Ui.noFocusHighlight(scroll);
        content = new LinearLayout(this);
        content.setOrientation(LinearLayout.VERTICAL);
        scroll.addView(content, Ui.wide());
        screen.addView(scroll, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 0, 1));
        notice = Ui.text(this, "", 14, Ui.CORAL, true);
        notice.setPadding(0, dp(8), 0, dp(8));
        screen.addView(notice, Ui.wide());
        setContentView(screen);
        Ui.creamStatusBar(this);
    }

    /* Una pantalla nueva: el subtítulo, sin líneas ni opciones. */
    private void screen(Mode next, String title) {
        mode = next;
        subtitle.setText(title);
        content.removeAllViews();
        notice.setText("");
    }

    private void line(String text) {
        TextView view = Ui.text(this, text, 17, Ui.BLACK, false);
        view.setPadding(0, dp(3), 0, dp(3));
        content.addView(view, Ui.wide());
    }

    /* La firma: grande y en rosa, que es lo que se compara. */
    private void signature(String text) {
        TextView view = Ui.text(this, text, 44, Ui.PINK, true);
        view.setLetterSpacing(0.15f);
        view.setPadding(0, dp(6), 0, dp(6));
        content.addView(view, Ui.wide());
    }

    private void gap() {
        View view = new View(this);
        content.addView(view, new LinearLayout.LayoutParams(1, dp(20)));
    }

    private View item(String text, boolean outline, View.OnClickListener action) {
        View view = Ui.row(this, text, outline, action);
        content.addView(view);
        return view;
    }

    /* ---- las pantallas ---- */

    private void showHome() {
        closeAll();
        screen(Mode.HOME, "pass your save to another device");
        line("on the same network: a PC, a handheld or a phone with Tamatama");
        gap();
        item("Send Save", true, v -> showVersions(false)).requestFocus();
        item("Receive Save", true, v -> showReceive());
        item("Restore Backup", true, v -> showVersions(true));
    }

    /* backup: para Restore Backup, las versiones con backup. */
    private void showVersions(boolean backup) {
        String[] list = versions(backup);
        if (list.length == 0) {
            showResult(backup ? "restore backup" : "send save",
                    backup ? "there is no backup yet" : "there are no saves to send", this::showHome);
            return;
        }
        screen(Mode.VERSIONS, backup ? "restore backup" : "send save");
        line("choose the version");
        gap();
        for (int i = 0; i < list.length; i += 4) {
            final String name = list[i], game = list[i + 1], resume = list[i + 2], date = list[i + 3];
            View row = item(name, false, v -> {
                version = name;
                md5 = game;
                summary = resume;
                if (backup) showRestore(date);
                else showDevices();
            });
            if (i == 0) row.requestFocus();
        }
    }

    private void showDevices() {
        if (!sending) {
            String[] mine = sendOpen();
            if (mine == null) {
                showResult("send save", "cannot use the network", this::showHome);
                return;
            }
            sending = true;
            me = mine[0];
            signature = mine[1];
        }
        screen(Mode.DEVICES, "send save");
        line(version);
        if (!summary.isEmpty()) line(summary);
        line("this device: " + me);
        signature(signature);
        line("send it to:");
        shownDevices = null;
        refreshDevices();
        schedule();
    }

    /* Rehace la lista si ha cambiado: cada fila, firma y nombre. */
    private void refreshDevices() {
        String[] list = sendDevices();
        String key = TextUtils.join("\n", list);
        if (key.equals(shownDevices)) return;
        shownDevices = key;
        /* las 5 primeras vistas son las líneas de showDevices */
        int lines = summary.isEmpty() ? 4 : 5;
        while (content.getChildCount() > lines) content.removeViewAt(lines);
        for (int i = 0; i < list.length; i += 2) {
            final String sig = list[i], name = list[i + 1];
            View row = item(sig + "  " + name, false, v -> startSending(sig, name));
            if (i == 0) row.requestFocus();
        }
        notice.setText(list.length == 0 ? "looking for devices in Receive Save..." : "");
    }

    private void startSending(String sig, String name) {
        String problem = sendStart(sig, md5);
        if (problem != null) {
            showResult("send save", problem, this::showDevices);
            return;
        }
        showSending(sig, name);
    }

    private void showSending(String sig, String name) {
        screen(Mode.SENDING, "send save");
        line("waiting for " + name);
        signature(sig);
        line("to accept it");
        gap();
        item("Cancel", true, v -> cancelSending()).requestFocus();
    }

    private void cancelSending() {
        sendCancel();
        showResult("send save", "cancelled", this::showDevices);
    }

    private void showReceive() {
        if (!receiving) {
            String[] mine = receiveOpen();
            if (mine == null) {
                showResult("receive save", "cannot use the network", this::showHome);
                return;
            }
            receiving = true;
            me = mine[0];
            signature = mine[1];
        }
        screen(Mode.RECEIVE, "receive save");
        line("this device: " + me);
        signature(signature);
        line("waiting for a save...");
        line("choose this device in Send Save");
        String last = receiveNotice();
        notice.setText(last != null ? last : "");
        receiveListen(true);
        schedule();
    }

    private void showAsk() {
        receiveListen(false);
        String[] offer = receiveOffer();
        screen(Mode.ASK, "receive save");
        line("a save from " + offer[0]);
        signature(offer[1]);
        line(offer[2]);
        line(offer[3] + (offer[4].isEmpty() ? "" : "  " + offer[4]));
        TextView warning = Ui.text(this, "your save will be kept as backup", 14, Ui.CORAL, false);
        warning.setPadding(0, dp(8), 0, 0);
        content.addView(warning, Ui.wide());
        gap();
        item("Accept", true, v -> {
            receiveAccept();
            showReceiving();
        }).requestFocus();
        item("Decline", true, v -> {
            receiveDecline();
            showReceive();
        });
    }

    private void showReceiving() {
        screen(Mode.RECEIVING, "receive save");
        line("receiving...");
    }

    private void showRestore(String date) {
        screen(Mode.RESTORE, "restore backup");
        line(version);
        line("back to the save before the last one received:");
        TextView when = Ui.text(this, date + (summary.isEmpty() ? "" : "  " + summary), 19, Ui.PINK, true);
        when.setPadding(0, dp(6), 0, dp(6));
        content.addView(when, Ui.wide());
        TextView warning = Ui.text(this, "the current one will be the backup", 14, Ui.CORAL, false);
        content.addView(warning, Ui.wide());
        gap();
        item("Restore", true, v -> {
            boolean ok = restore(md5);
            showResult("restore backup", ok ? "backup restored" : "cannot restore the backup", this::showHome);
        }).requestFocus();
        item("Back", true, v -> showHome());
    }

    /* Cómo terminó: con OK, then. La red sigue atendiendo mientras se ve. */
    private void showResult(String title, String message, Runnable then) {
        screen(Mode.RESULT, title);
        line(message);
        gap();
        afterResult = then;
        item("OK", true, v -> then.run()).requestFocus();
    }

    /* ---- la red ---- */

    private void schedule() {
        handler.removeCallbacks(tick);
        handler.postDelayed(tick, TICK_MS);
    }

    private void tick() {
        if (sending) {
            boolean finished = sendPoll();
            if (mode == Mode.DEVICES) refreshDevices();
            if (mode == Mode.SENDING && finished) {
                String result = sendResult();
                boolean sent = "save sent".equals(result);
                showResult("send save", result, sent ? this::showHome : this::showDevices);
            }
        }
        if (receiving) {
            int event = receivePoll();
            if (mode == Mode.RECEIVE && event == OFFER) showAsk();
            else if (mode == Mode.ASK && event == CANCELLED) showReceive();
            else if (mode == Mode.RECEIVING && event == DONE) {
                String result = receiveResult();
                boolean received = "save received".equals(result);
                showResult("receive save", received ? result + ": " + version() : result,
                        received ? this::showHome : this::showReceive);
            }
            if (mode == Mode.RECEIVE) {
                String last = receiveNotice();
                notice.setText(last != null ? last : "");
            }
        }
        if (sending || receiving) handler.postDelayed(tick, TICK_MS);
    }

    /* La versión de la oferta recibida, para el resultado. */
    private String version() {
        String[] offer = receiveOffer();
        return offer[2];
    }

    private void closeAll() {
        handler.removeCallbacks(tick);
        if (sending) sendClose();
        if (receiving) receiveClose();
        sending = receiving = false;
    }

    /* Volver: a la pantalla de antes, cancelando lo que esté en marcha. */
    @Override
    public void onBackPressed() {
        switch (mode) {
            case HOME:
                super.onBackPressed();
                break;
            case SENDING:
                cancelSending();
                break;
            case ASK:
                receiveDecline();
                showReceive();
                break;
            case RECEIVING:
                receiveCancel();
                showReceive();
                break;
            case RESULT:
                if (afterResult != null) afterResult.run();
                break;
            default:
                showHome();
        }
    }
}
