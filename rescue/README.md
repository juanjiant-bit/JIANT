# FM-1 Rescue — unbrick an FM-1 with only an RP2040

[English below](#english)

Repara un M-VAVE FM-1 en **hard brick** (no enciende, no aparece por USB) **sin Python ni terminal**.
El RP2040 lleva dentro el firmware oficial V15 y hace todo él solo; el PC solo sirve para copiar un archivo
(o ni eso: después basta un cargador USB).

1. Abre **`fm1-rescue.html`** (Chrome, Edge o Firefox; funciona también como archivo local), elige tu placa
   (RP2040-Zero o XIAO RP2040) y el firmware: un paquete **JIANT** (`.fwsc` del instalador) o el `FM-1.fwsc`
   **V15** oficial. Si el FM-1 venía con la **V16**, usa JIANT (arranca con la cabecera de la V16; la V15 no
   está comprobada con ella). Pulsa **Crear fm1-rescue.uf2**.
2. Mantén **BOOT** y conecta el RP2040 al PC; arrastra `fm1-rescue.uf2` a la unidad `RPI-RP2`.
3. Desconéctalo. FM-1 **apagado**; cables: verde → `0`, blanco → `1`, negro → `GND`, rojo sin conectar.
4. Conecta el RP2040 a un PC o cargador (LED amarillo parpadeando) y **enciende el FM-1**.
5. LED azul = trabajando; **verde fijo** = listo: apaga el FM-1, quita los cables, enciéndelo (arranca V15).
   Rojo parpadeando = error: apaga el FM-1, reconecta el RP2040 y repite.

**Sin ordenador:** la app Android **FM-1 Rescue** (`android/rescue`, ver su [LÉEME](../android/rescue/LEEME.md)) hace
todo desde el móvil: graba el RP2040 por OTG y muestra el progreso de la reparación.

Comparado con la guía con Python: no hay que instalar Python, ni `setup`, ni `wizard`, ni escribir `WRITE`,
ni puertos serie; el orden "primero el RP2040, después encender el FM-1" lo marca el LED.

## Qué hace (y qué no)

- Envía el USB_KEY al encender (o la *soft key* si el FM-1 ya corre la V15 de fábrica) → modo UBOOT de la ROM.
- Sube `wl82loader.bin`, comprueba chip key `0x980F` y flash ID `0x856014` / `0xB36014`.
- Escribe **solo los sectores de 4 KiB de `0x4000..0x92FFF` que difieren** de la V15, cada uno borrado, escrito y
  releído; al final relee toda la zona y la compara.
- **Nunca** escribe la cabecera de arranque (`0x0000..0x3FFF`) ni los datos (`0x93000..`: ajustes, sonidos,
  canciones). No hace la copia de seguridad doble del método con Python: si quieres un volcado previo, usa la
  placa sin V15 (es un FM-1 Transporter normal) con su web app o `fm1t.py`.
- Acepta la V15 oficial (hash de la región `6edf3c37…`) o un paquete de JIANT hecho con `tools/fm1pkg_make.py`
  (identidad `FM-1_9xx`, `flash.bin` de exactamente `0x93000`, con el loader de Felucca). La V16 no cabe.
- Con la V15, el `.uf2` final no se distribuye (contiene firmware de M-VAVE). Con JIANT sí se podría (GPL).

**Estado:** probado en PC (lógica del firmware real contra un FM-1 simulado, `firmware/test/`), **aún no
probado en hardware**. Úsalo con la batería cargada y bajo tu responsabilidad.

## English

Unbrick an FM-1 with an RP2040 and no Python: open `fm1-rescue.html`, pick the board and the official V15
`FM-1.fwsc`, make `fm1-rescue.uf2`, copy it to the board in BOOT mode, wire green → GP0 (D+), white → GP1 (D-),
black → GND, power the board (computer or charger), switch the FM-1 on, wait for steady green. Same safety
rules as above: only differing sectors of `0x4000..0x92FFF`, each verified; head and data never written.
Not yet tested on hardware.

## Files

| | |
|---|---|
| `fm1-rescue.html` | the page (generated, self-contained: builder + both base firmwares inlined) |
| `builder.js` | `.fwsc` → V15 region, payload, merged UF2 (browser and Node) |
| `make_uf2.mjs` | command-line builder: `node make_uf2.mjs BASE.uf2 FM-1.fwsc wl82loader.bin OUT.uf2` |
| `base/` | prebuilt rescue firmware for RP2040-Zero and XIAO RP2040 (no loader, no M-VAVE code) |
| `firmware/` | FM-1 Transporter `14fd4df` + `src/payload.c`, `src/autorescue.c`, WS2812 LED for the Zero |
| `firmware/test/` | host test of `payload.c` + `autorescue.c` against a mock FM-1 |
| `build.sh` | rebuilds `base/` and the page; with `FM1_V15` and `FM1_LOADER` also runs the host test |

Payload layout: `firmware/src/payload.h` (at 1 MiB in the RP2040 flash). Without a payload the firmware is
a plain FM-1 Transporter; its data port also answers `rescue` with the rescue state and progress (on core 0,
also while a restore runs), which the Android app (`android/rescue`) polls.

## Credits and licence

MIT (`firmware/LICENSE`). FM-1 Transporter by Leo Kuroshita / Hügelton Instruments (MIT), Pico-PIO-USB by
sekigon-gonnoc (MIT, `firmware/lib/`), `.fwsc` parsing and V15 hashes from Quixotic7's MvaveFM1Unbricker (MIT),
`wl82loader.bin` from kagaimiq's jl-uboot-tool (MIT, downloaded at build time, not stored here), WS2812 PIO
program from pico-examples (BSD-3-Clause). Procedure after GoodPerson42's "FM1 Unbricking for Dummies".
