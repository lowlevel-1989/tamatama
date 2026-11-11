/* Ui: lo que comparten las pantallas de Android (MainActivity y
   TransferActivity): los colores del menú de --menu (src/shared/colors.h),
   los textos y las filas grandes para el touch, con el recuadro azul al
   tocarlas o con el foco del mando. */
package com.lowlevel1989.tamatama;

import android.app.Activity;
import android.graphics.Typeface;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.StateListDrawable;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.TextView;

final class Ui {
    static final int CREAM = 0xfffffddc, PINK = 0xffeb008b, GREEN = 0xff60c668, BLUE = 0xffb9e3f9,
            CORAL = 0xfff38060, BLACK = 0xff000000;

    private Ui() {}

    static int dp(Activity activity, float value) {
        return Math.round(TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, value,
                activity.getResources().getDisplayMetrics()));
    }

    static TextView text(Activity activity, String value, float sp, int color, boolean bold) {
        TextView view = new TextView(activity);
        view.setText(value);
        view.setTextSize(TypedValue.COMPLEX_UNIT_SP, sp);
        view.setTextColor(color);
        view.setTypeface(Typeface.MONOSPACE, bold ? Typeface.BOLD : Typeface.NORMAL);
        view.setGravity(Gravity.CENTER);
        return view;
    }

    static LinearLayout.LayoutParams wide() {
        return new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT);
    }

    /* El fondo de una fila: el recuadro azul de menu.c al tocarla o con el
       foco. En reposo, nada, o el borde verde de un botón (outline). */
    private static Drawable rowBackground(Activity activity, boolean outline) {
        GradientDrawable chosen = new GradientDrawable();
        chosen.setColor(BLUE);
        chosen.setCornerRadius(dp(activity, 14));
        GradientDrawable idle = new GradientDrawable();
        idle.setColor(0);
        idle.setCornerRadius(dp(activity, 14));
        if (outline) idle.setStroke(dp(activity, 2), GREEN);
        StateListDrawable states = new StateListDrawable();
        states.addState(new int[] {android.R.attr.state_pressed}, chosen);
        states.addState(new int[] {android.R.attr.state_focused}, chosen);
        states.addState(new int[] {}, idle);
        return states;
    }

    /* Sin el gris con que Android marca el foco: ya lo marca el azul. */
    static void noFocusHighlight(View view) {
        if (android.os.Build.VERSION.SDK_INT >= 26) view.setDefaultFocusHighlightEnabled(false);
    }

    /* Una opción: texto grande, centrado, con sitio de sobra para el touch. */
    static TextView row(Activity activity, String value, boolean outline, View.OnClickListener action) {
        TextView view = text(activity, value, 19, BLACK, outline);
        view.setMinHeight(dp(activity, 60));
        view.setPadding(dp(activity, 12), dp(activity, 14), dp(activity, 12), dp(activity, 14));
        view.setBackground(rowBackground(activity, outline));
        noFocusHighlight(view);
        view.setFocusable(true);
        view.setClickable(true);
        view.setOnClickListener(action);
        LinearLayout.LayoutParams params = wide();
        params.setMargins(0, dp(activity, 4), 0, dp(activity, 4));
        view.setLayoutParams(params);
        return view;
    }

    /* La barra de estado en crema, con los iconos oscuros. */
    static void creamStatusBar(Activity activity) {
        activity.getWindow().setStatusBarColor(CREAM);
        activity.getWindow().getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR);
    }
}
