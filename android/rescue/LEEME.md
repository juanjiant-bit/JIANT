# FM-1 Rescue para Android — LÉEME

[English below](#english)

Repara un **M-VAVE FM-1 en hard brick** (pantalla negra, no aparece por USB) con un **RP2040** y **tu móvil
Android**. No hace falta ordenador, ni Python, ni copiar archivos: la app graba el RP2040 por el cable OTG y
después te va diciendo qué hacer mientras repara el FM-1.

> **Estado:** versión 0.1, **sin probar todavía en hardware real**. El núcleo (lectura de la V15, la imagen que
> se graba en el RP2040 y el protocolo con el bootrom) está probado en el ordenador contra un RP2040 simulado
> y el resultado es idéntico byte a byte al de la página web (`rescue/fm1-rescue.html`). Úsala con la batería
> del FM-1 cargada y bajo tu responsabilidad.

## Qué necesitas

| | |
|---|---|
| Móvil Android 7 o más nuevo | con USB OTG (casi todos los actuales) |
| Adaptador **OTG** USB-C (o micro-USB) a USB-A, o un cable USB-C a USB-C | para enchufar el RP2040 al móvil |
| **Waveshare RP2040-Zero** o **Seeed XIAO RP2040** | la placa de la guía de GoodPerson42 |
| Cable USB-C con 4 hilos sueltos | negro, rojo, blanco, verde |
| Nada más | JIANT, la V15 oficial y el loader van dentro de la app: no hace falta internet |

## Instalar

Descarga `fm1-rescue.apk`: pestaña **Actions** del repositorio → ejecución **android** → **Artifacts** →
`fm1-rescue` (dentro del zip está `app-debug.apk`). Ábrelo en el móvil y permite "instalar apps de origen
desconocido" si Android lo pide.

## Usar

1. **Elige qué instalar** (paso 1 de la app):
   - **JIANT** (incluido, recomendado): funciona tanto en FM-1 que venían con la **V15** como con la **V16**,
     porque arranca con la cabecera de arranque de las dos.
   - **V15 oficial de M-VAVE** (incluida, comprobada por su hash). Con un FM-1 que venía en V16 **no está
     comprobado** que arranque: en ese caso usa JIANT.
   - "Elegir otro .fwsc": otra versión de JIANT o tu propio archivo de la V15 (cualquier otro se rechaza).
2. **Elige tu placa:** RP2040-Zero o XIAO RP2040.
3. **Cableado**, con el FM-1 **apagado** (el rojo no se conecta):

   | Hilo | RP2040-Zero | XIAO RP2040 |
   |---|---|---|
   | verde (D+) | `0` | `D6` |
   | blanco (D−) | `1` | `D7` |
   | negro (GND) | `GND` | `GND` |
   | rojo (5 V) | — | — |

   Enchufa el USB-C de esos hilos al FM-1.
4. **Conecta el RP2040 al móvil** con el adaptador OTG y acepta el aviso de acceso USB (marca "usar siempre" para
   que no vuelva a salir). No hace falta pulsar BOOT si la placa trae MicroPython, un programa de demostración, el
   firmware de rescate o el FM-1 Transporter: la app la pasa sola a modo de grabación. Si aun así no la detecta,
   desconéctala y vuelve a enchufarla **manteniendo BOOT** pulsado.
5. La app **graba el RP2040 sola** (unos 15 segundos, con barra de progreso) y lo reinicia.
6. Cuando la app diga **"Enciende el FM-1 ahora"**, enciéndelo. Verás "Reparando el FM-1…" con el progreso
   (1–3 minutos). **No desconectes nada.**
7. **"¡Listo!"** → apaga el FM-1, quita los cables y vuelve a encenderlo: arranca con lo que elegiste (JIANT o
   V15). Desde ahí puedes cambiar de firmware con el instalador web de JIANT (incluido volver a la V16 oficial).

Para reparar otro FM-1 basta con desconectar y volver a conectar el RP2040.

## Si algo sale mal

| La app dice | Qué hacer |
|---|---|
| "Permite el acceso USB" | Acepta el aviso de Android. |
| "No se pudo grabar el RP2040" | Desconéctalo y vuelve a conectarlo con BOOT pulsado. Prueba otro adaptador OTG. |
| "No responde al reinicio" | El programa que trae la placa no admite el reinicio automático: conéctala con BOOT pulsado. |
| "Apaga el FM-1" | El FM-1 estaba encendido con otro firmware. Apágalo, reconecta el RP2040, enciéndelo. |
| Error: "Se perdió la conexión…" | Revisa los cables; prueba a intercambiar verde y blanco. Batería cargada. |
| Error: "Esto no parece un FM-1" | No se escribió nada. Revisa que sea un FM-1. |
| Error: "Una escritura no se completó…" | Apaga el FM-1, reconecta el RP2040 y repite: retoma donde quedó. |

El LED del RP2040 dice lo mismo sin mirar el móvil: amarillo parpadeando = esperando que enciendas el FM-1,
azul = trabajando, verde fijo = listo, rojo parpadeando rápido = error.

## Qué hace exactamente

- **En el RP2040** (por PICOBOOT, el protocolo del bootrom que usa `picotool`): borra, escribe y **relee** cada
  sector del firmware de rescate (`rescue/firmware`, basado en el FM-1 Transporter) más la V15 y el loader, y lo
  reinicia.
- **En el FM-1** (lo hace el RP2040): lo pone en modo de arranque de la ROM, comprueba que es un FM-1 (chip
  `0x980F`, flash `0x856014`/`0xB36014`) y escribe **solo los sectores de `0x4000..0x92FFF` que difieren** del
  firmware elegido, cada uno verificado, con una relectura final completa.
- **Nunca** toca la cabecera de arranque (`0x0000..0x3FFF`) ni tus datos (`0x93000..`: ajustes, sonidos,
  canciones). A diferencia del método con Python, **no** guarda una copia completa del FM-1 antes.
- El móvil solo alimenta el RP2040 y lee su progreso.
- **JIANT** va dentro del APK: el paquete público que ofrece el instalador web (`fetch_firmware.py` lo mete al
  compilar; GPL-3.0, con la licencia Apache-2.0 de los archivos del SDK de JieLi que lleva).
- **La V15** también va dentro: `fetch_firmware.py` la toma al compilar de
  [Quixotic7/FM1Backups](https://github.com/Quixotic7/FM1Backups) en un commit fijado y solo la acepta si es el
  archivo original de M-VAVE (sha256 `db1642b2…`); la app vuelve a comprobar el hash de su región de firmware.
  No se guarda en git. Es software de M-VAVE: incluirla es decisión del autor de la app. Si una compilación no
  la trae, la app la descarga de ese mismo sitio.
- El loader de JieLi va dentro (jl-uboot-tool, MIT, licencia en `assets/wl82loader-LICENSE.txt`).
- Si el FM-1 tenía otro firmware personalizado (Felucca, Skiff…) y con JIANT no arranca, repite el rescate
  eligiendo la V15: los datos de otros firmwares pueden confundir a JIANT.

### ¿Y la V16?

No se escribe directamente: ocupa hasta `0xA8FFF` (pisaría el almacén de canciones desde `0x97000`) y trae su
propia cabecera de arranque, que este método nunca toca. Para un FM-1 que venía en V16, el rescate con **JIANT**
deja intacta su cabecera V16 y arranca; después, si quieres la V16 oficial, "Volver al oficial" del instalador.

## Para desarrolladores

| | |
|---|---|
| `app/src/main/java/fm/jiant/rescue/RescueImage.java` | `.fwsc` → región V15, payload, sectores a grabar (igual que `rescue/builder.js`) |
| `Picoboot.java` | protocolo PICOBOOT del bootrom del RP2040 |
| `RescueStatus.java` | la respuesta de `rescue` del firmware |
| `MainActivity.java` | la pantalla, USB OTG y permisos |
| `test/run.sh` | prueba en el ordenador con un RP2040 simulado (necesita la V15, la V16 y el loader) |

Compilar: `gradle -p android/rescue assembleDebug` (JDK 17, Gradle 8.9); el firmware para cada placa sale de
`rescue/base/` (`rescue/build.sh`). Licencia MIT; créditos en `rescue/README.md`.

## English

FM-1 Rescue for Android unbricks an FM-1 with an RP2040 and the phone: firmware to restore: JIANT (bundled, the default; runs under both the V15 and the V16 boot head) or the
the official V15 (bundled too, hash-checked); JieLi's loader is bundled. Pick the board, wire green → GP0, white → GP1, black → GND (FM-1 off, red unconnected), hold BOOT and
plug the RP2040 into the phone with an OTG adapter. The app programs the board over PICOBOOT, reboots it, and
then shows the rescue's progress read from the board: "switch the FM-1 on now", "repairing…", "done". Only
the differing sectors of `0x4000..0x92FFF` are written, each verified; the boot head and user data are never
touched. A board running something else (MicroPython, a demo, a Pico SDK program) is sent to BOOTSEL by the
app (reset interface or 1200-baud touch), so BOOT is usually not needed. Version 0.1, **not yet tested on hardware**.
