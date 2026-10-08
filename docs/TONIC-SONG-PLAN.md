# FELUCCA TONIC — Plan del song mode (estructura de SLOOP)

Base: el song mode de SLOOP 2.4.1 (`isod89/sloop-fm1`, GPL-3.0-only, mismo linaje que Felucca), analizado en
`docs/TONIC-AUDIT.md` §4. Se implementa en pasos, cada uno con sus tests y un commit.

## Lo que hay hoy en Felucca 1.1.5.1

- `firmware/src/song_chain.c`: la canción encadena **los patrones** de los 4 proyectos guardados (los sonidos
  son los actuales). 16 filas `{slot A–D, repeat 1–16}`; `repeat` cuenta vueltas del patrón del track 1.
- La chain se guarda dentro de cada proyecto (FUN6+) y el editor la lee y escribe por SysEx (v6).
- UI: página SONG (SEQ sostenido la abre), PLAY en esa página arranca la canción.

## Implementación (4a + 4b)

`firmware/src/song_chain.c` (estado y filas), `song_play.c` (ISR: reloj de compases, cambio de sección en el compás) y
`song_main.c` (main loop: prepara la sección siguiente en `sec_stage`, guarda la música en `song_keep` y la devuelve al
terminar). La copia vieja de patrones (`chain.source`, 12,5 KB de `.bss`) ya no existe. Una fila es `{sección, compases}`.

## Pasos

### 4a. Arranger por compases — HECHO
- Portar `arranger.h` de SLOOP tal cual (con crédito): reloj de compases (muestras × BPM, conserva el resto),
  entradas `{sección, compases 1–64}` y loop.
- La fila de chain pasa de `{slot, repeat}` a `{sección, compases}`. Conversión de chains guardadas:
  compases = repeat × compases del patrón de esa sección.
- Página SONG: KNOB de compases en lugar de repeticiones. Protocolo del editor: la fila lleva compases (se sube
  la versión del protocolo).

### 4b. Sección = proyecto completo — HECHO
- Las 4 secciones viven en RAM (como `proj_slot[4]` de SLOOP, ~16 KB del pool; hay 64 KB libres).
- Al cambiar de sección cambian patrones **y sonidos** (engine, parámetros, patch FM6). La sección siguiente se
  prepara en el main loop y el ISR solo la aplica en el compás.
- Song backup: tocar la canción aparta el loop y lo devuelve al parar (SLOOP `song_backup` / `song_restore`).

### 4c. Live sections, quick chain y SONG REC (capa de canción) — HECHO
- Con el botón de la capa sostenido:
  - teclas 1–4: saltar a A–D en el próximo compás (parado: carga la sección);
  - teclas 5–8: guardar el loop en A–D (RAM al instante, flash con el transporte parado);
  - varias teclas 1–4 sin soltar el botón: **quick chain** (hasta 8, en loop, cada una dura su patrón);
  - D5: loop / song; E5: SONG REC (graba sección + compases tocados en la chain); G5: página SONG;
  - OCT+: guardar el loop en la sección que suena; OCT−: volver a ella como estaba guardada (en el compás).

### 4d. 8 canciones
- Sacar los slots de samples de usuario (SAMPLE USR1–3, GRAIN USR, SLICE USR, `slice_store.c`, subida y
  grabación del editor, comandos SysEx de samples).
- Flash: 8 canciones × 4 secciones × A/B = 64 sectores + índice A/B = 66 de 68 disponibles.
- Selector de canción (solo con el transporte parado), nombre por canción, backup / restore del editor.

### 4e. Escenas (Fase 5 de la spec)
- Cada fila de chain suma mutes de grupo (1 B), macros (4 B) y punch-in FX (1 B).

## Decisión de controles

La capa de canción es **SAVE sostenido**, como en SLOOP y como pide la spec ("mantener SAVE y tocar A B B C").
El undo de Felucca se sacó: en su lugar, dentro de la capa, OCT+ guarda la sección que suena y OCT− la recupera
(`ui_layer.c` layer_oct, `song_main.c` section_store / section_recall). Tocar SAVE y soltarlo sigue abriendo la
página de guardado.
