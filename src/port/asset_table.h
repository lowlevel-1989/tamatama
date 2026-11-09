/* port/asset_table.h: cómo se escriben los assets al extraer el .apk. Una línea
   por extensión: ASSET_RENAME(".como_viene", ".como_se_guarda"). Lo incluye
   shared/apk.c. Los .tgd del juego son PNG con otro nombre: guardados como
   .png, cualquiera puede abrirlos con un visor. */
ASSET_RENAME(".tgd", ".png")
