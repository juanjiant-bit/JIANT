# JIANT FM: arte de la interfaz (fuente)

Las 20 pantallas del concepto JIANT FM ([docs/jiant-fm-ui-concept.jpg](../../docs/jiant-fm-ui-concept.jpg))
recortadas a la pantalla del FM-1: 240 × 240, 16 colores. Por ahora **no se compilan** en el firmware; el plan
y las medidas están en [docs/TONIC-UI.md](../../docs/TONIC-UI.md).

| Archivo | Qué es |
| --- | --- |
| `art/NN_nombre.png` | las 20 pantallas, 240 × 240 |
| `gen_ui_art.py` | PNG → `ui_art_data.h`: paleta de 16 colores (RGB565 con bytes invertidos, como `gfx.c`), 4 bits por píxel, deflate crudo |
| `ui_art.c` | descompresor deflate y `ui_art_draw()` / `ui_art_rect()` sobre el canvas de `gfx.c` |
| `host_test.c`, `preview.py` | test en la PC: decodifica las 20 por el camino de dibujo real y las compara byte a byte |

Regenerar y probar (necesita Pillow y numpy):

```sh
python3 assets/ui-art/gen_ui_art.py assets/ui-art/art build/ui_art/ui_art_data.h --dump build/ui_art/expect
cp assets/ui-art/ui_art.c build/ui_art/
cc -O2 -Wall -std=c99 -I build/ui_art -I firmware/src -o build/ui_art/host_test assets/ui-art/host_test.c
build/ui_art/host_test build/ui_art/expect build/ui_art/out      # "all screens match"
```

Código bajo GPL-3.0-only, como el resto del firmware.
