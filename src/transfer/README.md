Transfer Save: pasa la partida de una versión a otro dispositivo de la misma
red (un PC, una consola o un teléfono con Tamatama), sin teclado y sin abrir
puertos.

El núcleo, sin pantallas (lo usan el PC, las consolas y Android):
- sender.c, receiver.c: el envío y la recepción, con su estado.
- net.c: el socket UDP, los anuncios y el formato de los paquetes.
- saves.c: leer, resumir, guardar con backup y restaurar las partidas.
- identity.c: el nombre (usuario@hostname (sistema); en Android, el
  modelo) y la firma de 4 caracteres, nueva cada vez que se abre Send Save
  o Receive Save.

Las pantallas del PC y las consolas (Android tiene las suyas, en Java:
TransferActivity, por src/platform/android/transfer.c):
- transfer.h: lo que usa el resto del programa (el menú, --transfer).
- transfer.c: la pantalla de Transfer Save y Restore Backup.
- send.c, receive.c: Send Save y Receive Save, sobre sender.c y
  receiver.c.
- screens.c: qué tiene cada pantalla (textos y opciones), una función
  cada una. Se dibujan con shared/screen.c, como el menú.

El protocolo: paquetes UDP de texto en el puerto 47474, todos con la misma
primera línea, TAMA 1 <tipo> <firma de quien lo manda> [palabras], y a veces
algo detrás:

    HELLO S|R <nombre>   cada segundo, a cada dirección de la red y por
                         broadcast; S en Send Save, R en Receive Save
    HERE S|R <nombre>    la respuesta a HELLO
    OFFER <n>            el número de la oferta (1, 2...: una nueva tras
                         un Decline no es una repetición), y detrás, en
                         líneas: name, game (el md5), file,
                         size, date (unix) y summary
    WAIT                 la oferta ha llegado: se pregunta
    OK                   Accept
    NO <motivo>          declined, version, format, busy o cancelled
    DATA <bytes> <crc32> y detrás, la partida
    DONE / FAIL <motivo> guardada, o no (format, crc, write)
    CANCEL               el que envía ya no espera

El que envía repite OFFER o DATA cada segundo hasta que le contestan; el que
recibe contesta a cada repetición con su última respuesta. Así no importa
que se pierda un paquete.

Un ejemplo entero: el PC (192.168.1.10, en Send Save, firma Y875) pasa la
partida de Classic v1.0.2 a una consola (192.168.1.20, en Receive Save,
firma G2Q3). Cada bloque es un paquete; \n es el salto de línea.

Los dos se anuncian cada segundo, y el otro contesta:

    consola -> 192.168.1.10:47474 (y a cada dirección de la red)
      TAMA 1 HELLO G2Q3 R root@H700 (ROCKNIX)
    PC -> 192.168.1.20:47474
      TAMA 1 HERE Y875 S lowlevel@pc (Fedora Linux)

Send Save ya lista "G2Q3  root@H700 (ROCKNIX)". Al elegirla, la oferta:

    PC -> consola
      TAMA 1 OFFER Y875 1\n
      name lowlevel@pc (Fedora Linux)\n
      game 395c4bbdf2e9b9f66714ee523a72a492\n
      file Tama2Movie_gamestatus_0.dat\n
      size 768\n
      date 1794385080\n
      summary Day 34\n
    consola -> PC
      TAMA 1 WAIT G2Q3

La consola pregunta Accept / Decline. Mientras, el PC repite OFFER cada
segundo y la consola contesta WAIT a cada una. Con Accept:

    consola -> PC
      TAMA 1 OK G2Q3
    PC -> consola
      TAMA 1 DATA Y875 768 1a2b3c4d\n
      <los 768 bytes de la partida>
    consola -> PC
      TAMA 1 DONE G2Q3

1a2b3c4d es el CRC32 de los 768 bytes, en hex. Si no coincide, la consola
contesta TAMA 1 FAIL G2Q3 crc y no escribe nada. Con Decline, en vez de OK:

    consola -> PC
      TAMA 1 NO G2Q3 declined

Y si la consola no tiene esa versión, en vez de WAIT:

    consola -> PC
      TAMA 1 NO G2Q3 version

Por qué no hay que abrir puertos: un cortafuegos deja pasar la respuesta a lo
que su equipo ha mandado. Los dos lados mandan HELLO a cada dirección de su
red (hasta 1024; si la red es más grande, el /24 propio) desde el puerto 47474
al 47474, así que lo que llega del otro ya cuenta como respuesta. Solo falla
en las redes que no dejan hablar entre sí a los dispositivos (las de
invitados, algunas de hoteles).

Lo que protege: solo se escucha con Receive Save abierta, nada se escribe
sin Accept, solo pasa entre la misma versión y el mismo fichero, y la
partida de antes queda como backup (el fichero con .backup detrás). Lo mal
formado se descarta.
