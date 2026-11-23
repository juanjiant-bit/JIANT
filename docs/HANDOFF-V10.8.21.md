# JIANT FM — estado y pasos para terminar la publicación de V10.8.21

Documento para retomar el trabajo con otra herramienta si se corta esta sesión.

## Estado al 23/11/2026

- **Beta publicada:** V10.8.21 (`17dc2256`, rama `codex/v66-clean`), en https://juanjiant-bit.github.io/JIANT/preview/emu/ y en el instalador beta.
- **Pública:** V10.7.81 (más las clásicas de `web/CLASSIC`).
- **Flash:** 2.176 B libres de 584.336. **RAM:** 2.616 B libres de 98.304.
- **En curso:** pruebas de CPU de peor caso y preparación de los documentos de lanzamiento (README, manuales, CHANGELOG, cheatsheets).

## Qué trae V10.8.21 respecto de V10.7.81

- **Modo canción:** JAM, TAKE, LOCK con YOU, REC por pista con UNDO y ayudas en pantalla.
- **SEQ > STEP FX:** las teclas tocan componentes en hold y se graban con REC. El knob PRESETS elige el paso. Aparece un cartel grande mientras se mantiene una tecla; también en los punch-ins.
- **Colores por pista:** barra superior TRACK N / ENGINE. Colores: T1 amarillo, T2 naranja, T3 rojo, T4 magenta, T5 violeta, T6 azul.
- **Nombres completos:** ECOSYSTEM, GENERATOR, MIXER, etc.
- **Macros:** K1 elige la macro, los destinos se suman y hay 16 rutas. Las macros pasan a ser destinos de la mod matrix.
- **Rediseños:**
  - Efectos en estilo HUD, DELAY Túnel y SENDS en columnas.
  - MIXER HUD, ENV en capas, LFO satélite y FOCUS con barra térmica.
  - ARRANGE y GENERATOR con «Partitura viva».
  - SCL KEY con nombre de escala y grados; SCL CHORD con el acorde real, intervalos y aviso en pistas DRUM.
- **Arranque y audio:** inicio «Seis hilos» con la firma suavizada y la versión visible, y arreglo del click al encender.
- **DRONE CLAY:** secuenciador por pista en SEQ > CLAY (drone grave, 4 moduladores, gestos), traído de JIANT BYTE.
- **Quitado:** GLO > LIVE, STRETCH, MUTATE, el dado de secuencia, GLOBAL COMP, ENV > TRACKS y las ayudas de enseñanza de un solo uso.
  - Los proyectos viejos siguen abriendo: los campos se leen y se ignoran.

## Pendiente antes de hacerla pública

1. **Resultado de las pruebas de CPU de peor caso:** 6 pistas pesadas con FX, DRONE CLAY en las 6, punch-ins, macros, modo canción y GEN. Hay que pasarlo a Juan.
2. **Confirmar en el equipo que no hay click al encender** (V10.8.19 en adelante).
3. **Documentos:** README, `docs/MANUAL.md`, `docs/MANUAL.es.md`, CHANGELOG y cheatsheets (PNG/PDF regenerados con las herramientas del repo).
4. **Cambio público:** hacerlo solo con el OK de Juan (ver abajo).

## Cómo se publica una beta (split-beta)

1. Esperar que el CI de `voice-user-check.yml` en `codex/v66-clean` termine en verde.
2. Del log del job `check`, tomar el SHA256 de la línea `build/felucca.fwsc`. Del log del job `emulator-candidate`, el de la línea `build/emu/felucca.wasm`.
3. En `main`, escribir `web/SPLIT_BETA.json` con `source` (sha completo), `run_id`, `version` ("V10.8.x-<sha7>-preview"), `publish_attempt` 1, `sha256` y `emulator_sha256`. Escribir `web/PREVIEW` con el sha completo.
4. Hacer commit con el mensaje empezando por `[private-preview]` y push a `main`.
5. Esperar que https://juanjiant-bit.github.io/JIANT/preview/emu/SOURCE_COMMIT muestre el sha.
6. No se puede republicar con el mismo nombre de versión: el workflow lo rechaza.
7. Desde el commit `09ca6fca`, `emulator.yml` repone `preview/emu` cuando se reconstruye el sitio desde `main`.

## Cómo se hace pública (con el OK de Juan)

Seguir lo que se hizo con V10.7.81:
- `web/STABLE` apunta al commit aprobado; el workflow `emulator.yml` construye el instalador público desde ahí.
- `web/CLASSIC` lista las versiones anteriores que se siguen ofreciendo.
- `split-beta.yml` compara referencias estables con `make_site.base_of`.
- El emulador público se fija con `web/EMULATOR`. Hoy sigue en `841f7dd` (V10.7.23); para mostrar el emulador nuevo hay que moverlo, también con el OK de Juan.

## Coordinación

- **JIANT BYTE:** sesión `session_01UVqsRpkaVp4GfeSdxxE1z3`, repo `juanjiant-bit/JIANT-BYTE`, rama `byte-cpu-light`. Hay que pasarle los cambios de interfaz y de funciones.
- **Editor web / JIANT Studio:** sesión `session_01UVqsRnhKjiABTzc6Ety3dd`. El espejo de pantalla está en `main` con el interruptor `FELUCCA_SCREEN_MIRROR`. En `codex/v66-clean` queda apagado hasta que haya lugar: hacen falta 928 B de flash.

## Próximos trabajos (después del lanzamiento)

- **PUNCH SEQ «Grilla de 4»** (página en el submenú de OCT+), que reemplaza a PUNCH LANE. BYTE pasa el código.
- **Teclas de DRONE CLAY con tarjeta:** la versión liviana, sin textos, ocupa unos 1 KB.
- **Optimización de CPU de BYTE:** la cadena del master procesada por bloque.
- **Más de 2 componentes por paso:** la tabla de extras, que necesita RAM.
