/* port/natives/web.c: la vista web del juego. El botón "Official site" de
   L.i.f.e. y Angel pide abrir la web oficial (OnSetWebView, con la dirección
   y el rectángulo donde ponerla). Aquí no hay vista web: se abre la página
   de Tamatama en el navegador (platform_open_url), y el juego sigue. */
#include "common.h"
#include "platform.h"

#define SITE "https://github.com/lowlevel-1989/tamatama"

/* OnSetWebView(String url, float x, float y, float w, float h): r0 es la
   dirección que pide el juego; el rectángulo no hace falta. */
void native_OnSetWebView(Regs *r) {
    const JniObject *url = to_local(r->r[0]);
    log_msg("[web] the game asks for %s: opening %s\n", url ? (const char *)url->data : "?", SITE);
    if (!platform_open_url(SITE)) log_msg("[web] cannot open it here\n");
    r->r[0] = 0;
}

/* Cerrar la vista web: no hay ninguna. */
void native_OnClearWebView(Regs *r) { (void)r; }
