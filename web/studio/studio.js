// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
// JIANT Studio (index.html): one page for JIANT FM and JIANT BYTE. What it does itself: finds the FM-1 and says which
// firmware it runs (INFO: BYTE's engines include MATH), shows its screen big (78 SCREEN / 79 SCREEN_DATA, as the editor's
// mirror) or the emulator's, and records audio (the FM-1's USB audio: stereo, or the 8 channels of USB > STEMS; or the
// emulator's output) and MIDI. The installer, the editor and the emulator are the sites' own pages in frames (one origin:
// /JIANT/ and /JIANT-BYTE/ on juanjiant-bit.github.io), dressed in the firmware's look.
// Install safety: the BroadcastChannel "jiant-midi" (as every MIDI page of both sites): while an installer writes, this
// page lets the FM-1 go (link closed, screen off) and says "released"; the editor and the emulator in the frames do the
// same on their own.
import { scrDecode, scrEncode, wavFile, zipStored, midiFile, SCR } from "./studio-lib.js";
import { Daw } from "./daw.js";
import { Player, anim } from "./learn.js";
import { captureBackup, restoreBackup, readBackup, songsNotRestored, BACKUP_IDS, bkU32, bkR32, bkCrc, bkPack, bkUnpack } from "./fm1backup.js";

const $ = (id) => document.getElementById(id);
const QS = new URLSearchParams(location.search);
const MOCK = QS.get("mock") === "1";

/* ------------------------------------------------------------------------------------------------- the words */
const TEXT = {
  es: {
    lvNow: "· En pantalla:", lvLayer: "capa", lvLayerWhat: "Estás manteniendo %s: las teclas y perillas hacen otra cosa mientras lo sostenés.", lvPressed: "apretado", lvButton: "Apretaste %s.",
    lvChip: "● En vivo", lvLead: "Tocá tu FM-1 y mirá acá: cada perilla que girás, el motor que elegís, los pasos y el play se explican solos, mientras pasan.",
    lvStartBtn: "Seguir mi FM-1", lvStopBtn: "Dejar de seguir", lvOff: "Conectá la FM-1 (Inicio › Buscar) o prendé el emulador, y tocá «Seguir mi FM-1».",
    lvNoDev: "No hay FM-1 conectada ni emulador prendido.", lvWaiting: "Siguiendo %s: tocá algo…", lvTouch: "Siguiendo: girá una perilla, cambiá de sonido o dale play.",
    lvTrack: "Pista", lvGlobal: "Global", lvSound: "Sonido", lvPreset: "preset", lvStep: "Paso", lvBar: "compás",
    lvUnknown: "Un parámetro de esta página.", lvReload: "Cambiaste de sonido: el motor y su preset en la pista elegida.", lvStepWhat: "Editaste un paso de la secuencia de esta pista.",
    lvPlay: "La música está sonando: el secuenciador avanza paso a paso.", lvStop: "Se detuvo la reproducción.", lvPunch: "Mantené FX y tocá teclas: un efecto suena solo mientras lo sostenés.",
    lvPunchOn: "un efecto activo", lvPunchOff: "ninguno", lvMutate: "MUTATE cambió el patrón de esta pista.", lvPage: "La página que está en pantalla y sus perillas.", lvMore: "Ver en la guía:",
    tourOpen: "Guía rápida", tourNext: "Siguiente", tourPrev: "Anterior", tourSkip: "Saltear", tourDone: "Listo",
    tourWelcomeT: "Bienvenido a JIANT Studio", tourWelcome: "Todo para tu FM-1 en una página: instalar el firmware, editarlo, ver su pantalla en grande, tocar el emulador, grabar, guardar tus proyectos y aprender cómo funciona. Te muestro dónde está cada cosa.",
    tourWelcomeH: "Son unos pasos; podés saltearlo y volver cuando quieras con el botón ? de arriba.",
    tourFwT: "JIANT FM o JIANT BYTE", tourFw: "Elegí el firmware: FM (síntesis) o BYTE (fórmulas). Studio cambia sus herramientas, sus manuales y su estética según cuál elijas.", tourFwH: "Si conectás una FM-1, Studio lo detecta y lo elige solo.",
    tourTargetT: "FM-1 o Emulador", tourTarget: "¿Con qué trabajás? Con tu FM-1 por USB, o con el emulador: el firmware real corriendo en el navegador.", tourTargetH: "El editor, la pantalla, grabar y la guía en vivo usan lo que elijas acá.",
    tourDevT: "Tu FM-1", tourDev: "Acá ves si hay una FM-1 conectada y qué firmware y versión tiene.", tourDevH: "Verde: conectada. Si algo no responde, desconectá y volvé a buscar.",
    tourFindT: "Buscar la FM-1", tourFind: "Conectá la FM-1 con un cable USB de datos y tocá Buscar: Studio la encuentra por MIDI.", tourFindH: "Funciona en Chrome o Edge (computadora) y Chrome en Android.",
    tourV_installT: "Instalar", tourV_install: "Instala o actualiza el firmware, con un backup antes. Primero leés las advertencias y aceptás; después son tres pasos.", tourV_installH: "Mientras instala, todo lo demás suelta la FM-1 para no interrumpirla.",
    tourV_editorT: "Editor", tourV_editor: "Cada sonido y ajuste de la FM-1 en la pantalla: motores, presets, perillas, secuencias, proyectos.", tourV_editorH: "Lo que cambiás acá cambia en el aparato al instante, y al revés.",
    tourV_screenT: "Pantalla", tourV_screen: "La pantalla de la FM-1 en grande, en vivo, incluso en el celular a pantalla completa.", tourV_screenH: "Tocá Mostrar; se gira sola en horizontal o vertical.",
    tourV_emuT: "Emulador", tourV_emu: "El firmware real en el navegador, para tocar con el mouse, el teclado de la computadora o un teclado MIDI.", tourV_emuH: "Tiene pantalla completa; elegí qué entrada MIDI lo toca.",
    tourV_recT: "Grabar", tourV_rec: "Graba el master o cada pista por separado (stems), en WAV y MIDI, con pistas como en un DAW.", tourV_recH: "De la FM-1 por USB (modo STEMS) o directamente del emulador.",
    tourV_projT: "Proyectos", tourV_proj: "Tus ranuras de proyecto A–D y los backups a mano, guardados en este navegador.", tourV_projH: "Hacé un backup antes de instalar o probar algo nuevo.",
    tourV_learnT: "Cómo funciona", tourV_learn: "Cada motor, secuenciador, efecto, modulador y generador, explicado con animaciones. Y En vivo: tocás la FM-1 y Studio te explica qué moviste.", tourV_learnH: "Empezá por el Índice o por ● En vivo.",
    tourV_docsT: "Manuales", tourV_docs: "El manual interactivo, el manual completo y las hojas de ayuda de cada firmware.", tourV_docsH: "Las hojas de ayuda se pueden imprimir.",
    tourV_rescueT: "Rescate", tourV_rescue: "Si una FM-1 no enciende o no aparece por USB, una placa RP2040 la puede revivir: un tutorial paso a paso.", tourV_rescueH: "Solo para FM-1 que no prenden; si prende, usá Instalar.",
    tourV_portsT: "Otros aparatos", tourV_ports: "JIANT en el SMK-37 y la MPC Sample, con su emulador y su manual, y los plugins que vienen.", tourV_portsH: "Probalos en el navegador sin tener el aparato.",
    tourLangT: "Idioma", tourLang: "Cambiá entre español e inglés.",
    tourEndT: "Listo", tourEnd: "Esta guía vuelve cuando quieras con este botón.", tourEndH: "Para empezar: conectá tu FM-1 y tocá Buscar, o elegí Emulador y prendelo.",
    siteLink: "El sitio de %s: qué es, sus pantallas y su historia →", vLearn: "Cómo funciona", lrIndex: "Índice", lrIndexLead: "Todo, de un vistazo: tocá cualquier elemento para abrir su página. Con ← y → pasás al siguiente.", lrTitleFm: "JIANT FM por dentro", lrTitleByte: "JIANT BYTE por dentro", lrTry: "Probarlo en el emulador", lrWhere: "En la FM-1: ", lrSince: "desde %s", lrNone: "Esta guía todavía se está escribiendo.",
    vRescue: "Rescate", rsTitle: "Rescatar un FM-1", rsBeta: "experimental · sin probar en hardware",
    rsLead: "Para un FM-1 que no enciende o que la computadora no ve por USB (hard brick). Una placa RP2040 de pocos dólares hace todo sola: se conecta al USB-C del FM-1 con tres cables y le vuelve a escribir el firmware. Sin Python ni terminal: Studio arma el archivo para la placa.",
    rsFirstT: "Antes de empezar", rsFirst: "Si el FM-1 enciende y la computadora lo ve (aunque esté trabado o suene raro), no hace falta esto: usá Instalar, que lo actualiza por USB.", rsToInstall: "Ir a Instalar",
    rs1: "Lo que necesitás", rsNeedBoard: "Una placa RP2040: Waveshare RP2040-Zero, Seeed XIAO RP2040 o Raspberry Pi Pico (o una parecida).",
    rsNeedUsb: "Un cable USB-C que puedas cortar (o un conector USB-C macho con cables), para llegar a sus hilos: verde (D+), blanco (D−), negro (GND) y rojo (5 V, que no se usa).",
    rsNeedWires: "Soldador o cables dupont para unir esos tres hilos a la placa.", rsNeedBatt: "El FM-1 con la batería cargada.", rsNeedPc: "Una computadora con Chrome, Edge o Firefox (o un cargador USB para el paso 6).",
    rs2: "Tu placa y los cables", rsCable: "Cable cortado: verde = D+, blanco = D−, negro = GND, rojo = 5 V. El rojo queda sin conectar (aislalo con cinta). Si la placa no logra hablar con el FM-1, probá cambiar verde y blanco entre sí.",
    rsWireZero: "RP2040-Zero: verde → pin 0, blanco → pin 1, negro → GND. Su LED es de colores (la tabla del paso 6).",
    rsWireXiao: "XIAO RP2040: verde → D6, blanco → D7, negro → GND. Su LED es de colores (la tabla del paso 6).",
    rsWirePico: "Raspberry Pi Pico (y las tipo Pico): verde → GP0 (pin 1), blanco → GP1 (pin 2), negro → GND (pin 3). Su LED es de un solo color: parpadeo lento = esperando, rápido = error, fijo = listo.",
    rs3: "El firmware que va a recibir", rsFwFile: "Otro archivo…", rsFwHelp: "Lo más simple: JIANT FM o JIANT BYTE, el paquete publicado, que Studio baja solo. También sirve el FM-1.fwsc de la V15 oficial de M-VAVE (no la V16: no entra). Si tu FM-1 venía con la V16, usá JIANT. Nada sale de tu navegador.",
    rsFwGet: "Bajando el paquete publicado de %s…", rsFwOk: "Comprobado: %s", rsFwBad: "No sirve: %s", rsLdGet: "Bajando el loader de JieLi (jl-uboot-tool, MIT)…", rsLdOk: "Loader de JieLi comprobado", rsLdBad: "No se pudo bajar el loader de JieLi: %s. Reintentá en un rato.",
    rs4: "Crear el archivo para la placa", rsMakeHelp: "Junta el firmware de rescate de tu placa, el firmware elegido y el loader en un solo fm1-rescue.uf2, y lo descarga.", rsMake: "Crear fm1-rescue.uf2", rsMade: "Listo: fm1-rescue.uf2 descargado (%s, %n KB)", rsNeedFw: "Falta el firmware (paso 3).",
    rs5: "Grabar la placa", rs6: "Conectar y rescatar", rs7: "Después",
    rsAfter: "Apagá el FM-1, sacá los cables y encendelo: arranca con el firmware elegido. Desde ahí lo podés actualizar o cambiar como siempre, con Instalar. Tus ajustes, sonidos y canciones no se tocan.",
    rsLedT: "Qué dice el LED",
    rsAndroidT: "Sin computadora: la app FM-1 Rescue", rsAndroid: "Hace lo mismo desde un teléfono Android: graba la placa por OTG y muestra el avance del rescate. Todavía es una versión de prueba: se baja de GitHub (Actions › android › Artifacts › fm1-rescue) con una cuenta iniciada.",
    rsAndroidHow: "Cómo se usa", rsAndroidApk: "Bajar el APK", rsMore: "Cómo funciona",
    rsSafe: "Solo se escribe la zona del firmware (0x4000–0x92FFF), sector por sector, cada uno releído y comparado; la cabecera de arranque y tus datos no se tocan. Sin firmware dentro, la placa queda como un FM-1 Transporter común. Basado en FM-1 Transporter (Hügelton Instruments), MvaveFM1Unbricker (Quixotic7), jl-uboot-tool (kagaimiq) y la guía de GoodPerson42. Usalo con la batería cargada y bajo tu responsabilidad: todavía no se probó en un FM-1 real.",
    vProj: "Proyectos", projTitle: "Proyectos y backups",
    projLead: "Tus proyectos y backups. Un backup guarda todo (la música actual, las canciones con sus secciones, los sonidos de usuario y los ajustes) en este navegador y, si querés, en un archivo. Restaurarlo lo devuelve al aparato tal cual. Funciona con la FM-1 conectada o con el emulador.",
    bkNow: "Hacer backup ahora", bkImport: "Importar archivo…", bkAlsoFile: "Descargar también el archivo", bkLibTitle: "Backups", bkLibHelp: "Guardados en este navegador (no se suben a ningún lado). Descargá los importantes: si se borran los datos del navegador, se pierden.",
    bkEmpty: "Todavía no hay backups. «Hacer backup ahora» guarda el primero.", bkDownload: "Descargar", bkRestore: "Restaurar", bkDelete: "Borrar",
    bkWorking: "Haciendo el backup… ", bkRestoring: "Restaurando… ", bkDone: "Backup guardado: %s", bkRestored: "Backup restaurado en %s", bkRestoredBut: "Restaurado; estas canciones no entraron en este firmware: %s",
    bkFail: "No se pudo: %s", bkNoDev: "Conectá la FM-1 (Inicio › Buscar) o prendé el emulador.", bkConfirm: "¿Restaurar «%s» en %s? Reemplaza la música actual, las canciones, los sonidos de usuario y los ajustes. No desconectes el USB.",
    bkDelConfirm: "¿Borrar «%s» de este navegador?", bkOther: "Backup de otro firmware: se restaura con su propio editor", bkImported: "Importado: %s", bkBadFile: "Ese archivo no es un backup de JIANT",
    bkMusic: "música actual", bkSongs: "%n canciones", bkSections: "%n secciones", bkTarget: "el emulador",
    slotsTitle: "Proyectos A–D", slotsHelp: "Las cuatro ranuras de proyecto del aparato. Cargar trae el proyecto a la música actual; Guardar escribe la música actual en la ranura.",
    slotsRead: "Leer ranuras", slotUsed: "con proyecto", slotEmpty: "vacía", slotUnknown: "sin leer", slotLoad: "Cargar", slotSave: "Guardar acá",
    slotLoadConfirm: "¿Cargar el proyecto %s? Reemplaza la música actual (guardala antes si la querés).", slotSaveConfirm: "¿Guardar la música actual en %s? Reemplaza el proyecto que tiene.",
    slotLoaded: "Proyecto %s cargado", slotSaved: "Música actual guardada en %s",
    vDocs: "Manuales", docsLeadFm: "Todo para aprender JIANT FM: el manual interactivo (tocá cualquier control del FM-1 y mirá qué hace), el manual completo y las dos hojas de ayuda para tener a mano.",
    docsLeadByte: "Todo para aprender JIANT BYTE: el manual interactivo, el manual completo y sus hojas de ayuda. Lo que todavía no está publicado dice «en camino».",
    docInter: "Manual interactivo", docInterText: "El FM-1 dibujado: tocá un control y mirá qué hace solo, mantenido y en combinación, con la pantalla a la que lleva. Tutorial paso a paso, buscador de comandos y recetas.",
    docInterTextByte: "El panel de JIANT BYTE: cada control, sus capas y combinaciones, las fórmulas y los motores, con la pantalla a la que lleva.",
    docOpenHere: "Abrir acá", docNewTab: "Abrir en otra pestaña", docFull: "Manual completo", docFullText: "Cada botón, perilla y mensaje con el nombre que muestra el aparato; índice que sigue la lectura. Para leer o imprimir.",
    docCtl: "Hoja de ayuda · Controles", docCtlText: "Las seis reglas de navegación, cada botón, capa, combinación y página en una hoja.",
    docSnd: "Hoja de ayuda · Sonido", docSndText: "Los motores, sus parámetros y los efectos en una hoja.", docView: "Ver", docPdf: "PDF", docPng: "Imagen",
    docSoon: "en camino", docNew: "Lo nuevo", docNewText: "Qué cambió en la última versión, con capturas.", docMap: "Mapa de navegación", docMapText: "Cada sección, sus grupos y páginas en el orden en que se recorren.",
    docPorts: "Manuales de otros aparatos", docPortsText: "SMK-37, MPC Sample, el plugin de MPC y el plugin VST3 · AU · LV2.",
    other: "English", vPorts: "Otros aparatos", vHome: "Inicio", vInstall: "Instalar", vEditor: "Editor", vScreen: "Pantalla", vEmu: "Emulador", vRec: "Grabar",
    tFm1: "FM-1", tEmu: "Emulador", target: "Destino",
    eyebrow: "Un firmware libre para la M-VAVE FM-1",
    leadFm: "Síntesis, secuencias, mutación y escena en vivo. Instalalo, editalo, mirá su pantalla en grande y grabá, todo desde acá.",
    leadByte: "Bytebeat y floatbeat: cada sonido, batería y animación es una fórmula del tiempo. Instalalo, editalo y grabá desde acá.",
    needBrowser: "Chrome o Edge", needCable: "Un cable USB de datos", needPhone: "Celular Android con USB-C (OTG)",
    s1Title: "Conectá la FM-1", s1Text: "Enchufala directo a esta computadora o celular, sin hub, y prendela. Cuando el navegador pida usar dispositivos MIDI, elegí Permitir.",
    find: "Buscar FM-1", forget: "Soltar la FM-1",
    s2Title: "Instalá o actualizá", s2Text: "Elegí el firmware. Antes de escribir, el instalador hace un backup y todas las demás herramientas sueltan la FM-1 solas.",
    fmPitch: "Seis pistas vivas, once motores, secuenciador de 64 pasos, canciones, generador y efectos.",
    bytePitch: "Fórmulas como voces: MATH, WAVE y DRUM-X con fórmulas. LIFE: las pistas se escuchan entre sí.",
    s3Title: "Usala", s3Text: "Todo con la misma conexión. Arriba elegís si trabajás con la FM-1 por USB o con el emulador.",
    toolEditor: "Sonidos, pasos, pistas, proyectos y backups desde la computadora.",
    toolScreen: "La pantalla de la FM-1 en grande, a pantalla completa en el celular.",
    toolEmu: "La FM-1 en el navegador, con sonido. Tocala con un teclado MIDI.",
    toolRec: "Master o stems en WAV, y lo que tocaste en MIDI.",
    hSafeQ: "¿Es seguro tener el editor o el emulador abiertos mientras instalo?",
    hSafeA: "Sí. Cuando el instalador empieza a escribir, avisa a todas las pestañas y marcos de JIANT FM y JIANT BYTE: el editor se desconecta, la pantalla se apaga y el emulador deja de escuchar MIDI, y el instalador espera a que cada uno confirme antes de mandar el primer byte. Nadie se vuelve a conectar hasta que termina. Cerrá otros programas de música que usen la FM-1 (un DAW, otro editor): esos no se enteran.",
    hStopQ: "Si la instalación se cortó", hStopA: "La FM-1 queda en modo actualización: volvé a enchufarla (o prendela) en esta misma página y la instalación sigue desde donde quedó.",
    hDeadQ: "Si la FM-1 no arranca", hDeadA: "Abrí Instalar: el instalador la encuentra en modo actualización y la recupera. Si ni así responde, hace falta un FM-1-transporter (está explicado en el instalador).",
    hPhoneQ: "¿Funciona en el celular?", hPhoneA: "En Android con Chrome, enchufando la FM-1 al USB-C del teléfono (un cable o adaptador OTG). En iPhone no: Safari no tiene Web MIDI.",
    devNone: "sin FM-1", devLooking: "buscando…", devGone: "FM-1 desconectada", devBusy: "instalando en otra pestaña", devMock: "(simulada)",
    found: "Encontrada: ", foundNot: "No aparece ninguna FM-1 con JIANT. Si tiene otro firmware o el oficial, instalá JIANT FM o JIANT BYTE desde el paso 2.",
    noMidi: "Este navegador no tiene Web MIDI: usá Chrome o Edge (en iPhone no hay).", denied: "El navegador no dio permiso para MIDI.",
    nowBusy: "Un instalador está escribiendo: la FM-1 quedó libre para él.",
    instTitleFm: "JIANT FM", instTitleByte: "JIANT BYTE",
    instSafe: "Mientras el instalador escribe, el editor, la pantalla y el emulador de esta página (y de otras pestañas) sueltan la FM-1 y esperan. No desenchufes ni apagues la FM-1 hasta que termine.",
    edTarget: "Destino: ", edEmuNote: "Editando el emulador: el editor habla con la FM-1 del navegador (pestaña Emulador). Prendelo ahí primero.",
    scrTitle: "La pantalla, en grande", scrStart: "Mostrar", scrStop: "Detener", scrFull: "Pantalla completa", scrExitFull: "Salir",
    scrSharp: "Píxeles nítidos", scrWaitFm1: "Conectá la FM-1 (Inicio) y tocá Mostrar.", scrWaitEmu: "Prendé el emulador en la pestaña Emulador y tocá Mostrar.",
    scrNo: "Este firmware no tiene espejo de pantalla: actualizá JIANT FM o JIANT BYTE.",
    scrNoFw: "Tu FM-1 tiene %s, que todavía no trae el espejo de pantalla: llega con la próxima versión del firmware. Mientras tanto, elegí Emulador arriba y mirá la pantalla del emulador.",
    scrNoShort: "sin espejo de pantalla (llega en la próxima versión)",
    scrHelpFm1: "La FM-1 manda solo lo que cambia: una pantalla quieta no cuesta nada. Mientras se ve acá, la interfaz de la FM-1 puede ir un poco más lenta en páginas muy animadas; el sonido no cambia.",
    scrHelpEmu: "La pantalla del emulador, cuadro a cuadro.",
    scrOrient: "La pantalla de la FM-1 es cuadrada (240 × 240). En vertical ocupa todo el ancho; en horizontal, todo el alto, con los controles al costado. A pantalla completa queda centrada y lo más grande posible en las dos posiciones: girá el celular como te quede cómodo.",
    scrRate: " KB/s", scrFps: " cuadros/s", scrWaiting: "esperando la imagen…",
    emuMidi: "MIDI que toca el emulador", emuMidiAll: "Todas las entradas", emuMidiNone: "Ninguna", emuMidiOff: "(prendé el emulador)",
    recTitle: "Grabar", recAudio: "Audio", recInput: "Entrada", recStart: "Grabar", recStop: "Detener",
    recProfile: "Modo USB de la FM-1 (MENU › SYSTEM › USB)", profAuto: "Detectar por los canales que llegan",
    profMaster: "AUDIO · STEREO · IN+OUT: el master (2 canales)", profStems: "STEMS: 8 canales (T1–T6, FX L R)",
    profStemsIn: "STEMS+IN: 10 canales (T1–T6, FX L R, VOX, DRY)",
    profHelp_auto: "Studio mira cuántos canales llegan y los nombra: 2 el master, 8 STEMS, 10 STEMS+IN. Después de cambiar el modo en la FM-1, desenchufala y enchufala.",
    profHelp_master: "En la FM-1: MENU › SYSTEM › USB › AUDIO (o STEREO, o IN+OUT) y reenchufá. Llega el master estéreo, como suena.",
    profHelp_stems: "En la FM-1: MENU › SYSTEM › USB › STEMS y reenchufá. Cada pista en mono antes del master (sin limitador ni MASTER) y la vuelta de efectos en estéreo: su suma es la mezcla. USB LEVEL en −6 dB o menos deja margen.",
    profHelp_stemsin: "En la FM-1: MENU › SYSTEM › USB › STEMS+IN y reenchufá. Como STEMS más dos canales de voz: VOX (la voz como la mezcló la última pista) y DRY (la voz como llegó); lo que suena en este dispositivo entra a la FM-1 como voz.",
    recMonitor: "Probar entrada", recMonitorStop: "Cerrar entrada", recPlay: "▶ Escuchar la toma", recPlayStop: "■ Parar",
    recDawEmpty: "Tocá Probar entrada: aparece un carril por cada canal que llega, con su nivel. Grabar dibuja cada pista mientras suena.",
    recFewerLong: "Llegan %got canales y el modo de la FM-1 manda %need. Chrome en Android entrega como mucho 2 (son T1 y T2, o el master): el navegador no puede leer el audio USB de otra forma (protege las interfaces de audio). Para los stems en vivo: Chrome o Edge en una computadora (Mac, Windows o Linux), o la app JIANT Probe en el celular; o los stems de una canción desde un backup (abajo), exactos y sin límite.",
    laneTrack: "pista, mono", lane_L: "master izquierdo", lane_R: "master derecho", lane_FXL: "efectos, izquierda", lane_FXR: "efectos, derecha",
    lane_VOX: "la voz mezclada", lane_DRY: "la voz como llegó", recMute: "Silenciar", recSolo: "Solo",
    stWhat: "Qué", stLive: "La sección que suena (8 compases)", stGo: "Stems desde la FM-1", stFile: "…o desde un backup",
    stReading: "Leyendo el backup de la FM-1… ", stRender: "Armando los stems: ", stDone: "Listo: las pistas están arriba.", stNeedDev: "Conectá la FM-1 (Inicio) o elegí un archivo de backup.",
    recSaved: "Toma guardada: está abajo, en Tomas.",
    recSrcFm1: "La FM-1 por USB es una entrada de audio: estéreo (USB > AUDIO), u 8 canales en USB > STEMS (las 6 pistas y los efectos por separado). Elegila acá.",
    recSrcEmu: "Graba la salida del emulador (prendelo en la pestaña Emulador).",
    recEmuStems: "Emulador JIANT · stems (8 canales)", recEmuMaster: "Emulador JIANT · master (estéreo)", recEmuMidi: "Emulador JIANT · MIDI OUT",
    recEmuOff: "Prendé el emulador en la pestaña Emulador (Prender) y volvé.", recEmuOld: "El emulador publicado todavía no entrega stems ni MIDI: llegan con su próxima versión. Mientras tanto: «master (estéreo)».", recEmuStemsHelp: "Del emulador, como USB STEMS: cada pista sola (T1–T6) y la vuelta de efectos (FX L R), antes del master; se graban juntas y se exportan como WAV por pista, el ZIP y la mezcla.",
    recEmuMidiHelp: "Las notas que manda MIDI OUT del emulador (MENU › CONTROL › MIDI OUT: KEYS las teclas, ALL también el secuenciador y el arpegiador).",
    recNoDev: "No hay entradas de audio (dale permiso al micrófono para verlas).", recAsk: "Pedir permiso de audio",
    recMidi: "MIDI", recMidiHelp: "Mientras grabás, también se guarda lo que entra por MIDI: lo que tocás en la FM-1 (MENU > MIDI OUT) o en un teclado.",
    recMidiIn: "Entrada MIDI", recMidiNone: "(ninguna)",
    recStems: "Stems de una canción", recStemsHelp: "Exactos y sin límite de canales, también en el celular: Studio lee la canción de la FM-1 por USB (o un archivo de backup) y la toca en el emulador una vez por pista. Cada pista en su WAV, alineadas al sample.",
    recStemsGo: "Abrir el emulador",
    recTakes: "Tomas", recNone: "Todavía no grabaste nada.", recWav: "WAV master", recStemsZip: "Stems (ZIP)", recMid: "MIDI",
    recChannels: " canales", recNotes: " notas",
    portsTitle: "JIANT FM en otros aparatos",
    portsLead: "El mismo firmware, de las mismas fuentes, en otras máquinas: una canción suena igual en todas. En las CPU grandes suma los motores SAMPLE y GRAIN, DRUM-X con samples y 8 voces por pista. Todos gratis y GPL-3.0; algunos todavía buscan quién los pruebe.",
    shotSmk: "el panel del SMK-37 con JIANT en el emulador", shotMpc: "la MPC Sample con JIANT en el emulador",
    shotDaw: "el editor del plugin: pantalla en vivo y pistas", shotDawSound: "el editor del plugin: el sonido",
    shotDawSamples: "el editor del plugin: el explorador de samples", portsManuals: "Manuales",
    portsHow: "Cómo funcionan", portTry: "Probar en el navegador", portDownload: "Descargar", portManual: "Manual", portInstall: "Cómo instalar",
    portSource: "Código", portBuild: "Compilar", portSoon: "Instalador: próximamente",
    stOk: "funciona", stSoon: "próximamente", portSoonBuy: "Próximamente a la venta", stPreview: "preview: buscamos testers", stDev: "en desarrollo", stCi: "compilado en CI",
    pSmkTitle: "M-VAVE SMK-37 Pro / Elite", pSmkText: "JIANT directo en el chip del SMK-37 (el mismo AC791N que la FM-1): 37 teclas, 16 pads RGB, 8 encoders y 4 faders como panel. Usa el mismo editor web.",
    pSmk1: "Pads con color por pista", pSmk2: "El mismo editor y los mismos sonidos que la FM-1",
    pMpcTitle: "Akai MPC Sample", pMpcText: "JIANT como el instrumento entero de la MPC Sample: su pantalla, sus pads, botones y perillas como el panel de la FM-1, arrancando desde la microSD.",
    pMpc1: "SAMPLE, GRAIN y DRUM-X con samples", pMpc2: "16 slots de samples de 60 s, grabación desde la entrada",
    pPlugMpcTitle: "Plugin para MPC", pPlugMpcText: "JIANT FM como instrumento dentro de las MPC standalone (MPC OS 3.10.1 o más nuevo) y de hosts VST2 de escritorio.",
    pPlugMpc1: "ARM32, ARM64 y escritorio", pPlugMpc2: "Pasa el simulador de host del SDK; falta probarlo en una MPC",
    pDawTitle: "VST3 · AU · LV2 · app", pDawText: "Para tu DAW o el iPad (AUM, GarageBand, Logic, Cubasis…), con un editor hecho para DAW: la pantalla en vivo, las seis pistas, los presets y parámetros de cada sonido, y un navegador de samples con arrastrar y soltar.",
    pDaw1: "Hasta 8 JIANT en una sesión, cada uno el suyo", pDaw2: "La app standalone responde al editor web (con un cable MIDI virtual)",
    pDaw3: "VST3 (macOS, Windows, Linux), AU y AUv3 (macOS, iPadOS), LV2 (Linux), standalone",
    foot: "JIANT Studio, de Juan Jiant",
  },
  en: {
    lvNow: "· On screen:", lvLayer: "layer", lvLayerWhat: "You are holding %s: the keys and knobs do something else while you hold it.", lvPressed: "pressed", lvButton: "You pressed %s.",
    lvChip: "● Live", lvLead: "Play your FM-1 and watch here: every knob you turn, the engine you pick, the steps and play explain themselves as they happen.",
    lvStartBtn: "Follow my FM-1", lvStopBtn: "Stop following", lvOff: "Connect the FM-1 (Home › Find) or switch the emulator on, and press “Follow my FM-1”.",
    lvNoDev: "No FM-1 connected and no emulator on.", lvWaiting: "Following %s: touch something…", lvTouch: "Following: turn a knob, change the sound or press play.",
    lvTrack: "Track", lvGlobal: "Global", lvSound: "Sound", lvPreset: "preset", lvStep: "Step", lvBar: "bar",
    lvUnknown: "A parameter of this page.", lvReload: "You changed the sound: the engine and its preset on the selected track.", lvStepWhat: "You edited a step of this track's sequence.",
    lvPlay: "The music is playing: the sequencer moves step by step.", lvStop: "Playback stopped.", lvPunch: "Hold FX and play keys: an effect sounds only while you hold it.",
    lvPunchOn: "an effect on", lvPunchOff: "none", lvMutate: "MUTATE changed this track's pattern.", lvPage: "The page on screen and its knobs.", lvMore: "See it in the guide:",
    tourOpen: "Quick tour", tourNext: "Next", tourPrev: "Back", tourSkip: "Skip", tourDone: "Done",
    tourWelcomeT: "Welcome to JIANT Studio", tourWelcome: "Everything for your FM-1 in one page: install the firmware, edit it, see its screen big, play the emulator, record, keep your projects and learn how it works. Here is where each thing is.",
    tourWelcomeH: "A few steps; skip it and come back whenever you want with the ? button at the top.",
    tourFwT: "JIANT FM or JIANT BYTE", tourFw: "Pick the firmware: FM (synthesis) or BYTE (formulas). Studio changes its tools, manuals and look to match.", tourFwH: "Connect an FM-1 and Studio picks it by itself.",
    tourTargetT: "FM-1 or Emulator", tourTarget: "What are you working with? Your FM-1 over USB, or the emulator: the real firmware running in the browser.", tourTargetH: "The editor, the screen, recording and the live guide use what you pick here.",
    tourDevT: "Your FM-1", tourDev: "Here you see whether an FM-1 is connected, and its firmware and version.", tourDevH: "Green: connected. If something does not answer, forget it and find it again.",
    tourFindT: "Find the FM-1", tourFind: "Connect the FM-1 with a USB data cable and press Find: Studio finds it over MIDI.", tourFindH: "Works in Chrome or Edge (computer) and Chrome on Android.",
    tourV_installT: "Install", tourV_install: "Installs or updates the firmware, with a backup first. First you read the warnings and accept; then three steps.", tourV_installH: "While it installs, everything else lets go of the FM-1 so nothing interrupts it.",
    tourV_editorT: "Editor", tourV_editor: "Every sound and setting of the FM-1 on screen: engines, presets, knobs, sequences, projects.", tourV_editorH: "What you change here changes on the device at once, and the other way round.",
    tourV_screenT: "Screen", tourV_screen: "The FM-1's screen, big and live, even full screen on your phone.", tourV_screenH: "Press Show; it turns by itself, landscape or portrait.",
    tourV_emuT: "Emulator", tourV_emu: "The real firmware in the browser, played with the mouse, the computer keyboard or a MIDI keyboard.", tourV_emuH: "It goes full screen; choose which MIDI input plays it.",
    tourV_recT: "Record", tourV_rec: "Records the master or each track apart (stems), as WAV and MIDI, with tracks as in a DAW.", tourV_recH: "From the FM-1 over USB (STEMS mode) or straight from the emulator.",
    tourV_projT: "Projects", tourV_proj: "Your project slots A–D and backups by hand, kept in this browser.", tourV_projH: "Back up before installing or trying something new.",
    tourV_learnT: "How it works", tourV_learn: "Every engine, sequencer, effect, modulator and generator, explained with animations. And Live: play the FM-1 and Studio tells you what you moved.", tourV_learnH: "Start with the Index or ● Live.",
    tourV_docsT: "Manuals", tourV_docs: "The interactive manual, the full manual and the cheat sheets of each firmware.", tourV_docsH: "The cheat sheets print well.",
    tourV_rescueT: "Rescue", tourV_rescue: "If an FM-1 does not turn on or show over USB, an RP2040 board can bring it back: a step-by-step tutorial.", tourV_rescueH: "Only for FM-1s that do not start; if it starts, use Install.",
    tourV_portsT: "Other devices", tourV_ports: "JIANT on the SMK-37 and the MPC Sample, with their emulators and manuals, and the plugins coming.", tourV_portsH: "Try them in the browser without the device.",
    tourLangT: "Language", tourLang: "Switch between Spanish and English.",
    tourEndT: "Done", tourEnd: "This tour comes back whenever you want with this button.", tourEndH: "To start: connect your FM-1 and press Find, or pick Emulator and switch it on.",
    siteLink: "%s's site: what it is, its screens and its story →", vLearn: "How it works", lrIndex: "Index", lrIndexLead: "Everything at a glance: tap any element to open its page. ← and → go to the next one.", lrTitleFm: "Inside JIANT FM", lrTitleByte: "Inside JIANT BYTE", lrTry: "Try it in the emulator", lrWhere: "On the FM-1: ", lrSince: "since %s", lrNone: "This guide is still being written.",
    vRescue: "Rescue", rsTitle: "Rescue an FM-1", rsBeta: "experimental · not tried on hardware yet",
    rsLead: "For an FM-1 that does not turn on, or that the computer does not see over USB (hard brick). A few-dollar RP2040 board does it all by itself: wired to the FM-1's USB-C with three wires, it writes the firmware again. No Python, no terminal: Studio makes the board's file.",
    rsFirstT: "Before you start", rsFirst: "If the FM-1 turns on and the computer sees it (even stuck or sounding wrong), you do not need this: use Install, which updates it over USB.", rsToInstall: "Go to Install",
    rs1: "What you need", rsNeedBoard: "An RP2040 board: Waveshare RP2040-Zero, Seeed XIAO RP2040 or Raspberry Pi Pico (or a similar one).",
    rsNeedUsb: "A USB-C cable you can cut (or a USB-C plug with wires), to reach its wires: green (D+), white (D−), black (GND) and red (5 V, unused).",
    rsNeedWires: "A soldering iron or dupont wires to join those three wires to the board.", rsNeedBatt: "The FM-1 with its battery charged.", rsNeedPc: "A computer with Chrome, Edge or Firefox (or a USB charger for step 6).",
    rs2: "Your board and the wires", rsCable: "A cut cable: green = D+, white = D−, black = GND, red = 5 V. The red one stays unconnected (tape it). If the board cannot talk to the FM-1, try swapping green and white.",
    rsWireZero: "RP2040-Zero: green → pin 0, white → pin 1, black → GND. Its LED is in colours (the table in step 6).",
    rsWireXiao: "XIAO RP2040: green → D6, white → D7, black → GND. Its LED is in colours (the table in step 6).",
    rsWirePico: "Raspberry Pi Pico (and Pico-style boards): green → GP0 (pin 1), white → GP1 (pin 2), black → GND (pin 3). Its LED has one colour: slow blink = waiting, fast = error, steady = done.",
    rs3: "The firmware it will get", rsFwFile: "Another file…", rsFwHelp: "Simplest: JIANT FM or JIANT BYTE, the published package, which Studio downloads by itself. M-VAVE's official V15 FM-1.fwsc works too (not V16: it does not fit). If your FM-1 came with V16, use JIANT. Nothing leaves your browser.",
    rsFwGet: "Downloading the published %s package…", rsFwOk: "Checked: %s", rsFwBad: "Not usable: %s", rsLdGet: "Downloading JieLi's loader (jl-uboot-tool, MIT)…", rsLdOk: "JieLi's loader checked", rsLdBad: "JieLi's loader could not be downloaded: %s. Try again in a while.",
    rs4: "Make the board's file", rsMakeHelp: "Puts your board's rescue firmware, the chosen firmware and the loader into one fm1-rescue.uf2, and downloads it.", rsMake: "Make fm1-rescue.uf2", rsMade: "Done: fm1-rescue.uf2 downloaded (%s, %n KB)", rsNeedFw: "The firmware is missing (step 3).",
    rs5: "Flash the board", rs6: "Connect and rescue", rs7: "Afterwards",
    rsAfter: "Switch the FM-1 off, remove the wires and switch it on: it boots the chosen firmware. From there you can update or change it as always, with Install. Your settings, sounds and songs are not touched.",
    rsLedT: "What the LED says",
    rsAndroidT: "No computer: the FM-1 Rescue app", rsAndroid: "Does the same from an Android phone: flashes the board over OTG and shows the rescue's progress. Still a test version: it downloads from GitHub (Actions › android › Artifacts › fm1-rescue) while signed in.",
    rsAndroidHow: "How to use it", rsAndroidApk: "Get the APK", rsMore: "How it works",
    rsSafe: "Only the firmware area (0x4000–0x92FFF) is written, sector by sector, each read back and compared; the boot head and your data are not touched. Without a firmware inside, the board is a plain FM-1 Transporter. Built on FM-1 Transporter (Hügelton Instruments), MvaveFM1Unbricker (Quixotic7), jl-uboot-tool (kagaimiq) and GoodPerson42's guide. Use it with a charged battery and at your own risk: it has not been tried on a real FM-1 yet.",
    vProj: "Projects", projTitle: "Projects and backups",
    projLead: "Your projects and backups. A backup keeps everything (the current music, the songs and their sections, the user sounds and the settings) in this browser and, if you want, in a file. Restoring it puts it back on the device as it was. Works with the FM-1 connected or with the emulator.",
    bkNow: "Back up now", bkImport: "Import a file…", bkAlsoFile: "Also download the file", bkLibTitle: "Backups", bkLibHelp: "Kept in this browser (nothing is uploaded). Download the ones that matter: clearing the browser's data loses them.",
    bkEmpty: "No backups yet. “Back up now” keeps the first.", bkDownload: "Download", bkRestore: "Restore", bkDelete: "Delete",
    bkWorking: "Backing up… ", bkRestoring: "Restoring… ", bkDone: "Backup kept: %s", bkRestored: "Backup restored on %s", bkRestoredBut: "Restored; these songs did not fit this firmware: %s",
    bkFail: "Could not: %s", bkNoDev: "Connect the FM-1 (Home › Find) or switch the emulator on.", bkConfirm: "Restore “%s” on %s? It replaces the current music, the songs, the user sounds and the settings. Keep USB connected.",
    bkDelConfirm: "Delete “%s” from this browser?", bkOther: "Another firmware's backup: restore it with its own editor", bkImported: "Imported: %s", bkBadFile: "That file is not a JIANT backup",
    bkMusic: "current music", bkSongs: "%n songs", bkSections: "%n sections", bkTarget: "the emulator",
    slotsTitle: "Projects A–D", slotsHelp: "The device's four project slots. Load brings a project into the current music; Save writes the current music into the slot.",
    slotsRead: "Read the slots", slotUsed: "holds a project", slotEmpty: "empty", slotUnknown: "not read", slotLoad: "Load", slotSave: "Save here",
    slotLoadConfirm: "Load project %s? It replaces the current music (save it first if you want it).", slotSaveConfirm: "Save the current music into %s? It replaces the project there.",
    slotLoaded: "Project %s loaded", slotSaved: "Current music saved into %s",
    vDocs: "Manuals", docsLeadFm: "Everything to learn JIANT FM: the interactive manual (touch any control of the FM-1 and see what it does), the full manual and the two cheat sheets to keep at hand.",
    docsLeadByte: "Everything to learn JIANT BYTE: the interactive manual, the full manual and its cheat sheets. What is not published yet says “on its way”.",
    docInter: "Interactive manual", docInterText: "The FM-1 drawn: touch a control to see what it does alone, held and in combination, with the screen it leads to. A step-by-step tutorial, a command search and recipes.",
    docInterTextByte: "JIANT BYTE's panel: every control, its layers and combos, the formulas and the engines, with the screen it leads to.",
    docOpenHere: "Open here", docNewTab: "Open in a new tab", docFull: "Full manual", docFullText: "Every button, knob and message named as the device shows it; contents that follow your reading. To read or print.",
    docCtl: "Cheat sheet · Controls", docCtlText: "The six navigation rules, every button, layer, combo and page on one sheet.",
    docSnd: "Cheat sheet · Sound", docSndText: "The engines, their parameters and the effects on one sheet.", docView: "View", docPdf: "PDF", docPng: "Image",
    docSoon: "on its way", docNew: "What's new", docNewText: "What changed in the latest version, with screenshots.", docMap: "Navigation map", docMapText: "Every section, its groups and pages in the order you walk them.",
    docPorts: "Manuals for other devices", docPortsText: "SMK-37, MPC Sample, the MPC plugin and the VST3 · AU · LV2 plugin.",
    other: "Español", vPorts: "Other devices", vHome: "Home", vInstall: "Install", vEditor: "Editor", vScreen: "Screen", vEmu: "Emulator", vRec: "Record",
    tFm1: "FM-1", tEmu: "Emulator", target: "Target",
    eyebrow: "A free firmware for the M-VAVE FM-1",
    leadFm: "Synthesis, sequencing, mutation and performance. Install it, edit it, see its screen big and record, all from here.",
    leadByte: "Bytebeat and floatbeat: every sound, drum and animation is a formula of time. Install it, edit it and record from here.",
    needBrowser: "Chrome or Edge", needCable: "A USB data cable", needPhone: "Android phone with USB-C (OTG)",
    s1Title: "Connect the FM-1", s1Text: "Plug it straight into this computer or phone, without a hub, and switch it on. When the browser asks to use MIDI devices, choose Allow.",
    find: "Find FM-1", forget: "Let the FM-1 go",
    s2Title: "Install or update", s2Text: "Pick the firmware. Before writing, the installer makes a backup and every other tool lets the FM-1 go by itself.",
    fmPitch: "Six living tracks, eleven engines, a 64-step sequencer, songs, a generator and effects.",
    bytePitch: "Formulas as voices: MATH, WAVE and DRUM-X with formulas. LIFE: the tracks hear each other.",
    s3Title: "Use it", s3Text: "All over one connection. Up top you choose the FM-1 over USB or the emulator.",
    toolEditor: "Sounds, steps, tracks, projects and backups from the computer.",
    toolScreen: "The FM-1's screen big, full screen on a phone.",
    toolEmu: "The FM-1 in the browser, with sound. Play it from a MIDI keyboard.",
    toolRec: "Master or stems as WAV, and what you played as MIDI.",
    hSafeQ: "Is it safe to keep the editor or the emulator open while installing?",
    hSafeA: "Yes. When the installer starts writing it tells every JIANT FM and JIANT BYTE tab and frame: the editor disconnects, the screen stops and the emulator stops listening to MIDI, and the installer waits until each one confirms before the first byte goes out. Nothing reconnects until it has finished. Close other music software using the FM-1 (a DAW, another editor): those are not told.",
    hStopQ: "If an installation stopped", hStopA: "The FM-1 stays in update mode: plug it back in (or switch it on) on this same page and the install carries on where it stopped.",
    hDeadQ: "If the FM-1 won't start", hDeadA: "Open Install: the installer finds it in update mode and recovers it. If it still does not answer, an FM-1-transporter is needed (explained in the installer).",
    hPhoneQ: "Does it work on a phone?", hPhoneA: "On Android with Chrome, with the FM-1 plugged into the phone's USB-C (an OTG cable or adapter). Not on iPhone: Safari has no Web MIDI.",
    devNone: "no FM-1", devLooking: "looking…", devGone: "FM-1 unplugged", devBusy: "installing in another tab", devMock: "(simulated)",
    found: "Found: ", foundNot: "No FM-1 with JIANT answers. If it runs other or official firmware, install JIANT FM or JIANT BYTE in step 2.",
    noMidi: "This browser has no Web MIDI: use Chrome or Edge (none on iPhone).", denied: "The browser did not allow MIDI.",
    nowBusy: "An installer is writing: the FM-1 was let go for it.",
    instTitleFm: "JIANT FM", instTitleByte: "JIANT BYTE",
    instSafe: "While the installer writes, the editor, the screen and the emulator on this page (and in other tabs) let the FM-1 go and wait. Do not unplug or switch off the FM-1 until it has finished.",
    edTarget: "Target: ", edEmuNote: "Editing the emulator: the editor talks to the FM-1 in the browser (Emulator tab). Switch it on there first.",
    scrTitle: "The screen, big", scrStart: "Show", scrStop: "Stop", scrFull: "Full screen", scrExitFull: "Exit",
    scrSharp: "Sharp pixels", scrWaitFm1: "Connect the FM-1 (Home) and press Show.", scrWaitEmu: "Switch the emulator on in the Emulator tab and press Show.",
    scrNo: "This firmware has no screen mirror: update JIANT FM or JIANT BYTE.",
    scrNoFw: "Your FM-1 runs %s, which has no screen mirror yet: it comes with the next firmware release. Meanwhile pick Emulator up top and see the emulator's screen.",
    scrNoShort: "no screen mirror (comes in the next release)",
    scrHelpFm1: "The FM-1 sends only what changes: a screen at rest costs nothing. While it shows here the FM-1's UI may run a little slower on very animated pages; the sound does not change.",
    scrHelpEmu: "The emulator's screen, frame by frame.",
    scrOrient: "The FM-1's screen is square (240 × 240). In portrait it fills the width; in landscape, the height, with the controls beside it. Full screen keeps it centred and as big as it gets either way: turn the phone as you like.",
    scrRate: " KB/s", scrFps: " frames/s", scrWaiting: "waiting for the picture…",
    emuMidi: "MIDI that plays the emulator", emuMidiAll: "Every input", emuMidiNone: "None", emuMidiOff: "(switch the emulator on)",
    recTitle: "Record", recAudio: "Audio", recInput: "Input", recStart: "Record", recStop: "Stop",
    recProfile: "The FM-1's USB mode (MENU › SYSTEM › USB)", profAuto: "Detect by the channels that come in",
    profMaster: "AUDIO · STEREO · IN+OUT: the master (2 channels)", profStems: "STEMS: 8 channels (T1–T6, FX L R)",
    profStemsIn: "STEMS+IN: 10 channels (T1–T6, FX L R, VOX, DRY)",
    profHelp_auto: "Studio counts the channels that come in and names them: 2 the master, 8 STEMS, 10 STEMS+IN. After changing the mode on the FM-1, unplug it and plug it in again.",
    profHelp_master: "On the FM-1: MENU › SYSTEM › USB › AUDIO (or STEREO, or IN+OUT), then plug it in again. The stereo master comes in, as it sounds.",
    profHelp_stems: "On the FM-1: MENU › SYSTEM › USB › STEMS, then plug it in again. Each track mono before the master (no limiter, no MASTER knob) and the effects' return in stereo: their sum is the mix. USB LEVEL at −6 dB or lower leaves headroom.",
    profHelp_stemsin: "On the FM-1: MENU › SYSTEM › USB › STEMS+IN, then plug it in again. STEMS plus two voice channels: VOX (the voice as the last track mixed it) and DRY (the voice as it came); what this device plays goes into the FM-1 as the voice.",
    recMonitor: "Test the input", recMonitorStop: "Close the input", recPlay: "▶ Play the take", recPlayStop: "■ Stop",
    recDawEmpty: "Press Test the input: a lane shows for each channel that comes in, with its level. Record draws each track as it plays.",
    recFewerLong: "%got channels come in and the FM-1's mode sends %need. Chrome on Android hands over at most 2 (T1 and T2, or the master): a web page cannot read USB audio any other way (browsers protect audio interfaces). For live stems: Chrome or Edge on a computer (Mac, Windows or Linux), or the JIANT Probe app on the phone; or a song's stems from a backup (below), exact and unlimited.",
    laneTrack: "track, mono", lane_L: "master left", lane_R: "master right", lane_FXL: "effects, left", lane_FXR: "effects, right",
    lane_VOX: "the voice as mixed", lane_DRY: "the voice as it came", recMute: "Mute", recSolo: "Solo",
    stWhat: "What", stLive: "The section playing (8 bars)", stGo: "Stems from the FM-1", stFile: "…or from a backup",
    stReading: "Reading the FM-1's backup… ", stRender: "Making the stems: ", stDone: "Done: the tracks are above.", stNeedDev: "Connect the FM-1 (Home) or pick a backup file.",
    recSaved: "Take saved: it is below, under Takes.",
    recSrcFm1: "The FM-1 over USB is an audio input: stereo (USB > AUDIO), or 8 channels in USB > STEMS (the 6 tracks and the effects apart). Pick it here.",
    recSrcEmu: "Records the emulator's output (switch it on in the Emulator tab).",
    recEmuStems: "JIANT emulator · stems (8 channels)", recEmuMaster: "JIANT emulator · master (stereo)", recEmuMidi: "JIANT emulator · MIDI OUT",
    recEmuOff: "Switch the emulator on in the Emulator tab (Switch on) and come back.", recEmuOld: "The published emulator does not give stems or MIDI yet: they come with its next version. Meanwhile: “master (stereo)”.", recEmuStemsHelp: "From the emulator, as USB STEMS: each track alone (T1–T6) and the effects' return (FX L R), before the master; recorded together, exported as a WAV per track, the ZIP and the mix.",
    recEmuMidiHelp: "The notes the emulator's MIDI OUT sends (MENU › CONTROL › MIDI OUT: KEYS the keys, ALL the sequencer and the arpeggiator too).",
    recNoDev: "No audio inputs (allow the microphone to list them).", recAsk: "Ask for audio permission",
    recMidi: "MIDI", recMidiHelp: "While recording, what comes in over MIDI is kept too: what you play on the FM-1 (MENU > MIDI OUT) or on a keyboard.",
    recMidiIn: "MIDI input", recMidiNone: "(none)",
    recStems: "A song's stems", recStemsHelp: "Exact and with no channel limit, on a phone too: Studio reads the song from the FM-1 over USB (or a backup file) and plays it in the emulator once a track. Each track its own WAV, sample-aligned.",
    recStemsGo: "Open the emulator",
    recTakes: "Takes", recNone: "Nothing recorded yet.", recWav: "WAV master", recStemsZip: "Stems (ZIP)", recMid: "MIDI",
    recChannels: " channels", recNotes: " notes",
    portsTitle: "JIANT FM on other devices",
    portsLead: "The same firmware from the same sources on other machines: a song sounds the same on every one. On the bigger CPUs it adds the SAMPLE and GRAIN engines, DRUM-X with samples and 8 voices a track. All free and GPL-3.0; some are still looking for testers.",
    shotSmk: "the SMK-37's panel running JIANT in the emulator", shotMpc: "the MPC Sample running JIANT in the emulator",
    shotDaw: "the plugin's editor: the live screen and tracks", shotDawSound: "the plugin's editor: the sound",
    shotDawSamples: "the plugin's editor: the sample browser", portsManuals: "Manuals",
    portsHow: "How they work", portTry: "Try it in the browser", portDownload: "Download", portManual: "Manual", portInstall: "How to install",
    portSource: "Source", portBuild: "Build", portSoon: "Installer: coming",
    stOk: "works", stSoon: "coming soon", portSoonBuy: "Coming soon, for sale", stPreview: "preview: testers wanted", stDev: "in development", stCi: "built by CI",
    pSmkTitle: "M-VAVE SMK-37 Pro / Elite", pSmkText: "JIANT bare metal on the SMK-37's chip (the FM-1's AC791N): 37 keys, 16 RGB pads, 8 encoders and 4 faders as its panel. The same web editor.",
    pSmk1: "Pads lit in each track's colour", pSmk2: "The FM-1's editor and sounds",
    pMpcTitle: "Akai MPC Sample", pMpcText: "JIANT as the MPC Sample's whole instrument: its screen, its pads, buttons and knobs as the FM-1's panel, started from the microSD card.",
    pMpc1: "SAMPLE, GRAIN and DRUM-X with samples", pMpc2: "16 sample slots of 60 s, recording from the input",
    pPlugMpcTitle: "MPC plugin", pPlugMpcText: "JIANT FM as an instrument inside the MPC standalones (MPC OS 3.10.1 or newer) and desktop VST2 hosts.",
    pPlugMpc1: "ARM32, ARM64 and desktop", pPlugMpc2: "Passes the SDK's host simulator; not yet tried on an MPC",
    pDawTitle: "VST3 · AU · LV2 · app", pDawText: "For your DAW or the iPad (AUM, GarageBand, Logic, Cubasis…), with an editor made for a DAW: the live screen, the six tracks, each sound's presets and parameters, and a sample browser with drag and drop.",
    pDaw1: "Up to 8 JIANTs in one session, each its own", pDaw2: "The standalone app answers the web editor (through a virtual MIDI cable)",
    pDaw3: "VST3 (macOS, Windows, Linux), AU and AUv3 (macOS, iPadOS), LV2 (Linux), standalone",
    foot: "JIANT Studio, by Juan Jiant",
  },
};
let lang = (() => { try { const l = localStorage.getItem("jiant.studio.lang"); if (TEXT[l]) return l; } catch (_) {} return /^es/i.test(navigator.language || "") ? "es" : "en"; })();
const t = (k) => TEXT[lang][k] ?? TEXT.en[k] ?? k;
function applyLang() {
  document.documentElement.lang = lang;
  document.querySelectorAll("[data-t]").forEach((e) => { e.textContent = t(e.dataset.t); });
  document.querySelectorAll("[data-t-label]").forEach((e) => e.setAttribute("aria-label", t(e.dataset.tLabel)));
  $("lang").textContent = t("other");
  render();
}
$("lang").addEventListener("click", () => { lang = lang === "es" ? "en" : "es"; try { localStorage.setItem("jiant.studio.lang", lang); } catch (_) {} applyLang(); });

/* ------------------------------------------------------------------------------------- where the tools live */
/* The sites: /JIANT/ (JIANT FM) and /JIANT-BYTE/ (JIANT BYTE) under one origin; this page is /JIANT/studio/. Anywhere else
 * (a copy on disk) the tools come from the published sites (then they cannot hear this page: another origin). */
const ROOT = (() => { const m = location.pathname.match(/^(.*?)\/JIANT\//); return location.protocol.startsWith("http") && m ? m[1] : null; })();
const SITE = ROOT !== null ? `${ROOT}/` : "https://juanjiant-bit.github.io/";
const TOOLS = {
  fm: { base: `${SITE}JIANT/`, installer: "webapp/installer/", editor: "webapp/editor/", emu: "emu/" },
  byte: { base: `${SITE}JIANT-BYTE/`, installer: "webapp/installer/", editor: "webapp/editor/", emu: "emu/" },
};
const toolUrl = (fw, tool, q = "") => TOOLS[fw].base + TOOLS[fw][tool] + q;

/* ------------------------------------------------------------------------------------------------ the state */
const state = {
  view: "home", fw: QS.get("fw") === "byte" ? "byte" : "fm", target: QS.get("target") === "emu" ? "emu" : "fm1",
  dev: null,                    // {link, input, output, version, kind ("fm" | "byte"), engines}
  looking: false, busy: false,  // busy: an installer writes (jiant-midi)
  found: "",                    // the last result line
};
try { const f = localStorage.getItem("jiant.studio.fw"); if (!QS.get("fw") && (f === "fm" || f === "byte")) state.fw = f; } catch (_) {}

/* ------------------------------------------------------------------------------------- the USB link (SysEx) */
const HDR = [0x7D, 0x46, 0x4C];
const CMD = { INFO: 1, PING: 25, SCREEN: 78, SCREEN_DATA: 79 };
/* the pushes the live guide reads (EDITOR_PROTOCOL.md, WATCH): CHANGED 23, RELOAD 24, STEP_CHANGED 26, TRACK_CHANGED 32,
   TRANSPORT 40, PUNCH 41, MUTATE 42, and (when the firmware has it) UI 43 */
const LIVE_PUSH = new Set([23, 24, 26, 32, 40, 41, 42, 43]);
const frame = (cmd, args = []) => [0xF0, ...HDR, cmd & 0x7F, ...args, 0xF7];
function unframe(d) {
  if (!d || d.length < 6 || d[0] !== 0xF0 || d[d.length - 1] !== 0xF7 || d[1] !== HDR[0] || d[2] !== HDR[1] || d[3] !== HDR[2]) return null;
  return { cmd: d[4], a: Array.from(d.subarray ? d.subarray(5, d.length - 1) : d.slice(5, -1)) };
}
/* one request at a time, each matched by its command (the device answers in order); pushes (79) to onPush */
class Link {
  constructor(output, onPush) { this.output = output; this.onPush = onPush; this.q = []; this.cur = null; this.lastSent = 0; this.closed = false; }
  request(cmd, args = [], timeout = 600, match = null) {
    if (this.closed) return Promise.reject(new Error("closed"));
    return new Promise((res, rej) => { this.q.push({ cmd, args, res, rej, timeout, match }); this.pump(); });
  }
  pump() {
    if (this.cur || !this.q.length || this.closed) return;
    const c = this.cur = this.q.shift();
    try { this.output.send(frame(c.cmd, c.args)); this.lastSent = Date.now(); } catch (e) { this.finish(null, e); return; }
    c.timer = setTimeout(() => this.finish(null, new Error("timeout")), c.timeout);
  }
  finish(a, err) {
    const c = this.cur;
    if (!c) return;
    clearTimeout(c.timer); this.cur = null;
    if (err) c.rej(err); else c.res(a);
    this.pump();
  }
  receive(data) {
    const f = unframe(data);
    if (!f) return;
    if (f.cmd === CMD.SCREEN_DATA) { this.onPush(f); return; }
    if (LIVE_PUSH.has(f.cmd)) { livePush(f, this); return; }    // (the live guide: what moved on the FM-1)
    if (this.cur && f.cmd === this.cur.cmd && (f.cmd !== CMD.SCREEN || f.a[0] === this.cur.args[0]) && (!this.cur.match || this.cur.match(f.a))) this.finish(f.a);
  }
  close() { this.closed = true; for (const c of [this.cur, ...this.q].filter(Boolean)) { clearTimeout(c.timer); c.rej(new Error("closed")); } this.cur = null; this.q = []; }
}
/* INFO: version string, NENGINES, P_COUNT, G_COUNT, NSTEP, P_E0, then the engines' names. JIANT BYTE's include MATH */
function parseInfo(a) {
  let i = 0;
  const str = () => { let s = ""; while (i < a.length && a[i]) s += String.fromCharCode(a[i++]); i++; return s; };
  const version = str(), n = a[i], pe0 = a[i + 4]; i += 5;
  const engines = [];
  for (let k = 0; k < n && i < a.length; k++) engines.push(str());
  /* JIANT BYTE names itself ("JIANT BYTE v0.16"); JIANT FM does not ("JIANT V10.8.24"). (Not by its engines: JIANT FM has
     MATH too since V10.8.21) */
  return { version, engines, pe0, kind: /\bBYTE\b/i.test(version) ? "byte" : "fm", jiant: /^(JIANT|FELUCCA)\b/i.test(version) };
}
const PORT_RE = /felucca|jiant|skiff|fm[ _-]*1|smk[ _-]*37|m[ _-]*vave|cuvave|composite|sinco|usb[ _-]*midi|usb audio/i;
let access = null;
async function midiAccess() {
  if (access) return access;
  if (MOCK) return (access = mockAccess());
  if (!navigator.requestMIDIAccess) throw Object.assign(new Error("nomidi"), { code: "noMidi" });
  try { access = await navigator.requestMIDIAccess({ sysex: true }); }
  catch (e) { throw Object.assign(new Error("denied"), { code: "denied" }); }
  access.onstatechange = (e) => {
    const p = e.port;
    if (state.dev && p && p.state === "disconnected" && (p.id === state.dev.input.id || p.id === state.dev.output.id)) { letGo(); state.found = t("devGone"); render(); }
    renderMidiInputs();
  };
  return access;
}
async function find() {
  if (state.looking || state.busy) return;
  state.looking = true; state.found = ""; render();
  try {
    const acc = await midiAccess();
    const live = (m) => [...m.values()].filter((p) => p.state !== "disconnected");
    let ins = live(acc.inputs).filter((p) => PORT_RE.test(p.name || "")), outs = live(acc.outputs).filter((p) => PORT_RE.test(p.name || ""));
    if (!ins.length || !outs.length) { ins = live(acc.inputs); outs = live(acc.outputs); }
    const pairs = ins.flatMap((input) => outs.map((output) => ({ input, output, s: input.name === output.name ? 1 : 0 }))).sort((x, y) => y.s - x.s);
    for (const { input, output } of pairs) {
      const link = new Link(output, onScreenData);
      input.onmidimessage = (e) => link.receive(e.data);
      try {
        const info = parseInfo(await link.request(CMD.INFO, [], 900));
        if (!info.jiant) throw new Error("not JIANT");
        letGo();
        /* does it mirror its screen? (78 SCREEN, op 0: firmware before the mirror sends no reply) */
        let screen = null;                                 // (null: it did not say; Show tries anyway)
        for (let k = 0; k < 2 && screen === null; k++) {
          try { const a = await link.request(CMD.SCREEN, [0], 1200); screen = a[1] === 0; } catch (_) {}
        }
        state.dev = { link, input, output, ...info, screen };
        if (state.fw !== info.kind) setFw(info.kind);
        state.found = t("found") + `${devName(state.dev)}${MOCK ? " " + t("devMock") : ""}` + (state.dev.screen === false ? " · " + t("scrNoShort") : "");
        return;
      } catch (_) { link.close(); input.onmidimessage = null; }
    }
    state.found = t("foundNot");
  } catch (e) { state.found = t(e.code || "foundNot"); }
  finally { state.looking = false; render(); }
}
/* "JIANT BYTE v0.16", "JIANT FM V10.8.21": the firmware's name once (its INFO says "JIANT BYTE v0.16" or "JIANT V10.8.21") */
const devName = (d) => `${d.kind === "byte" ? "JIANT BYTE" : "JIANT FM"} ${d.version.replace(/^(JIANT|FELUCCA)\s+/i, "").replace(/^(BYTE|FM)\s+/i, "")}`;
function letGo() {
  const d = state.dev;
  if (!d) return;
  screenStop(false);
  d.link.close();
  d.input.onmidimessage = midiTap;                       // (the recorder keeps listening to what the FM-1 plays)
  state.dev = null;
}
$("find").addEventListener("click", find);
$("forget").addEventListener("click", () => { letGo(); state.found = ""; render(); });

/* -------------------------------------------------------------------- install safety: the jiant-midi channel */
let busyUntil = 0, busyTimer = 0, midiBc = null;
try {
  const bc = midiBc = new BroadcastChannel("jiant-midi");
  bc.onmessage = (e) => {
    const m = e.data || {};
    if (m.type === "who") bc.postMessage({ type: "studio", connected: !!state.dev });
    if (m.type !== "installer") return;
    clearTimeout(busyTimer);
    if (m.busy) {
      busyUntil = Date.now() + 60000; state.busy = true;
      letGo(); monitorStop();
      state.found = t("nowBusy");
      bc.postMessage({ type: "released", who: "studio" });
      busyTimer = setTimeout(() => { state.busy = false; render(); }, 60000);
    } else { state.busy = false; busyUntil = 0; }
    render();
  };
} catch (_) {}

/* ------------------------------------------------------------------------------------- views, firmware, target */
const VIEWS = ["home", "install", "editor", "screen", "emu", "rec", "proj", "learn", "docs", "rescue", "ports"];
function go(view, push = true) {
  if (!VIEWS.includes(view)) view = "home";
  state.view = view;
  for (const v of VIEWS) $("v-" + v).hidden = v !== view;
  if (push && !(view === "learn" && /^#learn\//.test(location.hash))) history.replaceState(null, "", location.pathname + location.search + "#" + view);
  showFrames();
  render();
  window.scrollTo({ top: 0 });
}
document.addEventListener("click", (e) => {
  const g = e.target.closest("[data-go]");
  if (g) { e.preventDefault(); go(g.dataset.go); return; }
  const f = e.target.closest("[data-fw]");
  if (f) { setFw(f.dataset.fw); return; }
  const i = e.target.closest("[data-install]");
  if (i) { setFw(i.dataset.install); go("install"); return; }
  const tg = e.target.closest("[data-target]");
  if (tg) { setTarget(tg.dataset.target); }
});
function setFw(fw) {
  if (fw !== "fm" && fw !== "byte") return;
  const changed = fw !== state.fw;
  state.fw = fw;
  try { localStorage.setItem("jiant.studio.fw", fw); } catch (_) {}
  document.documentElement.dataset.skin = fw;
  document.querySelector('meta[name="theme-color"]').content = fw === "byte" ? "#000000" : "#050409";
  if (changed) for (const k of Object.keys(frames)) frames[k].fw = null;   // the tools of the other firmware: loaded anew
  showFrames();
  render();
}
function setTarget(target) {
  state.target = target === "emu" ? "emu" : "fm1";
  if (frames.editor.fw) frames.editor.fw = null;                            // the editor anew, for the other target
  screenStop(false);
  showFrames();
  render();
}

/* ------------------------------------------------------------------------- the tools in frames, in our look */
const frames = {
  install: { el: $("f-install"), fw: null, tool: "installer" },
  editor: { el: $("f-editor"), fw: null, tool: "editor" },
  emu: { el: $("f-emu"), fw: null, tool: "emu" },
};
const VIEW_FRAME = { install: "install", editor: "editor", emu: "emu" };
function showFrames() {
  const k = VIEW_FRAME[state.view];
  // the emulator stays loaded once it was (it keeps playing; the screen and the recorder read it)
  if (k) loadFrame(k);
  if (state.view === "screen" && state.target === "emu") loadFrame("emu");
}
function loadFrame(k) {
  const f = frames[k];
  if (f.fw === state.fw && f.el.src) return;
  f.fw = state.fw;
  const q = k === "editor" && state.target === "emu" ? "?emu=1" : MOCK && k === "editor" ? "?mock=1" : "";
  f.el.src = toolUrl(state.fw, f.tool, q);
}
for (const [k, f] of Object.entries(frames)) f.el.addEventListener("load", () => { dress(f.el); if (k === "editor") editorLoaded(); });
/* the firmware's look inside a tool's frame (same origin only): its colour tokens and type, over the page's own */
const DRESS = {
  fm: `:root,:root:not([data-theme="light"]){--bg:#050409;--fg:#efe6c8;--dim:#a39bb6;--line:rgba(239,230,200,.18)}
       body{background:#050409;color:#efe6c8;font-family:"Instrument Sans",system-ui,sans-serif}`,
  byte: `:root,:root:not([data-theme="light"]){--bg:#000;--fg:#f2f0f7;--dim:#9d94b8;--line:rgba(167,123,255,.35);
         --jiant-panel:#0b0912;--jiant-field:#05040a;--jiant-cyan:#a77bff;--jiant-yellow:#ffb21a;--jiant-mag:#ff3b3b;--jiant-orange:#ff7a1a;
         --jiant-display:"Silkscreen",monospace;--jiant-body:"IBM Plex Mono",monospace;--jiant-mono:"VT323",monospace;
         --jiant-heat:linear-gradient(90deg,#ff3b3b,#ff7a1a 40%,#ffb21a 60%,#a77bff)}
         body{background:#000;color:#f2f0f7;font-family:"IBM Plex Mono",monospace}
         h1,h2,.jiant-wordmark{font-family:"Silkscreen",monospace}`,
};
/* inside Studio a tool's own wordmark, links to the other tools, support line and footer are Studio's already */
const EMBEDDED = `h1.jiant-wordmark,.jiant-wordmark,p.kofi,.kofi,body>main>footer,body>footer,main>p.dim:first-of-type{display:none!important}
  main{padding-top:8px!important} a{color:inherit}`;
function dress(iframe) {
  try {
    const d = iframe.contentDocument;
    if (!d || !d.head) return;
    let s = d.getElementById("studio-skin");
    if (!s) {
      s = d.createElement("style"); s.id = "studio-skin"; d.head.append(s);
      const l = d.createElement("link"); l.rel = "stylesheet";
      l.href = "https://fonts.googleapis.com/css2?family=Michroma&family=IBM+Plex+Mono:wght@400;500;600&family=Instrument+Sans:wght@400;500;600&family=Silkscreen&family=VT323&display=swap";
      d.head.append(l);
    }
    s.textContent = DRESS[state.fw] + EMBEDDED;
    d.documentElement.dataset.skin = state.fw;          // (the editor's own design layer, editor.html #jiant-skin: the firmware's look)
  } catch (_) { /* another origin: its own look */ }
}
function editorLoaded() {
  // the FM-1 found here, or the emulator running: the editor (just loaded, so not connected) connects at once, as its own
  // button would
  if (state.busy || (state.target === "fm1" ? !state.dev : !emuOn())) return;
  try { const b = frames.editor.el.contentDocument.getElementById("connect"); if (b && !b.disabled) b.click(); } catch (_) {}
}

/* ------------------------------------------------------------------------------------------------ the screen */
const scr = { on: false, src: null, ctx: $("scr").getContext("2d"), img: null, lut: null, fb: new Uint16Array(SCR.W * SCR.H),
  dirty: null, raf: 0, timer: 0, bytes: 0, frames: 0, t0: 0, rate: 0, fps: 0, got: false, lock: null, copy: 0 };
scr.img = scr.ctx.createImageData(SCR.W, SCR.H);
scr.lut = (() => { const l = new Uint32Array(65536); for (let c = 0; c < 65536; c++) l[c] = 0xFF000000 | ((c & 31) * 255 / 31 | 0) << 16 | (((c >> 5) & 63) * 255 / 63 | 0) << 8 | (((c >> 11) & 31) * 255 / 31 | 0); return l; })();
function onScreenData(f) {
  if (!scr.on || scr.src !== "fm1") return;
  const box = scrDecode(f.a, scr.fb);
  if (!box) return;
  scr.bytes += f.a.length + 6;
  if (!scr.got) { scr.got = true; $("scrwait").hidden = true; }   // (the first picture: the waiting line goes)
  const d = scr.dirty;
  scr.dirty = !d ? box : { x: Math.min(d.x, box.x), y: Math.min(d.y, box.y),
    w: Math.max(d.x + d.w, box.x + box.w) - Math.min(d.x, box.x), h: Math.max(d.y + d.h, box.y + box.h) - Math.min(d.y, box.y) };
  if (!scr.raf) scr.raf = requestAnimationFrame(paint);
}
function paint() {
  scr.raf = 0;
  const d = scr.dirty;
  if (!d) return;
  scr.dirty = null; scr.frames++;
  const px = new Uint32Array(scr.img.data.buffer);
  for (let y = d.y; y < d.y + d.h; y++) for (let i = y * SCR.W + d.x, e = i + d.w; i < e; i++) px[i] = scr.lut[scr.fb[i]];
  scr.ctx.putImageData(scr.img, 0, 0, d.x, d.y, d.w, d.h);
}
function emuCanvas() {
  try { return frames.emu.el.contentDocument && frames.emu.el.contentDocument.getElementById("screen"); } catch (_) { return null; }
}
async function screenStart() {
  if (scr.on) return;
  scr.src = state.target; scr.got = false; scr.bytes = 0; scr.frames = 0; scr.t0 = Date.now();
  scr.fb.fill(0); scr.ctx.fillStyle = "#000"; scr.ctx.fillRect(0, 0, SCR.W, SCR.H);
  if (scr.src === "emu") {
    loadFrame("emu");
    scr.on = true;
    const step = () => {
      if (!scr.on) return;
      const c = emuCanvas();
      if (c && c.width) { scr.ctx.drawImage(c, 0, 0, SCR.W, SCR.H); if (!scr.got) { scr.got = true; $("scrwait").hidden = true; } scr.frames++; }
      scr.copy = requestAnimationFrame(step);
    };
    step();
  } else {
    const d = state.dev;
    if (!d) { render(); return; }
    scr.on = true;
    try {
      const a = await d.link.request(CMD.SCREEN, [1]);
      if (a[1] || !a[2]) { scr.on = false; $("scrstat").textContent = t("scrNo"); render(); return; }
    } catch (e) { scr.on = false; $("scrstat").textContent = t("scrNo"); render(); return; }
    clearInterval(scr.timer);
    scr.timer = setInterval(async () => {
      if (!scr.on || !state.dev || document.hidden) return;
      try { const a = await state.dev.link.request(CMD.SCREEN, [2]); if (scr.on && !a[2]) await state.dev.link.request(CMD.SCREEN, [1]); } catch (_) {}
    }, 1000);
  }
  wake();
  render();
}
function screenStop(tell = true) {
  if (!scr.on) return;
  scr.on = false;
  clearInterval(scr.timer); cancelAnimationFrame(scr.copy);
  if (tell && scr.src === "fm1" && state.dev) state.dev.link.request(CMD.SCREEN, [4]).catch(() => {});
  if (scr.lock) { scr.lock.release().catch(() => {}); scr.lock = null; }
  render();
}
async function wake() {
  try { if (scr.on && !scr.lock && navigator.wakeLock && !document.hidden) { scr.lock = await navigator.wakeLock.request("screen"); scr.lock.addEventListener("release", () => { scr.lock = null; }); } } catch (_) {}
}
document.addEventListener("visibilitychange", () => {
  if (document.hidden || !scr.on) return;
  wake();
  if (scr.src === "fm1" && state.dev) state.dev.link.request(CMD.SCREEN, [3]).catch(() => {});   // (it may have stopped meanwhile)
});
setInterval(() => {
  if (!scr.on) return;
  const dt = (Date.now() - scr.t0) / 1000;
  if (dt >= 1) { scr.rate = scr.bytes / dt; scr.fps = scr.frames / dt; scr.bytes = 0; scr.frames = 0; scr.t0 = Date.now(); }
  if (state.view === "screen") $("scrstat").textContent = !scr.got ? t("scrWaiting") :
    scr.src === "fm1" ? `${(scr.rate / 1024).toFixed(1)}${t("scrRate")} · ${scr.fps.toFixed(0)}${t("scrFps")}` : `${scr.fps.toFixed(0)}${t("scrFps")}`;
}, 500);

const fullscreen = () => {
  const s = $("stage");
  if (document.fullscreenElement || document.webkitFullscreenElement) (document.exitFullscreen || document.webkitExitFullscreen).call(document);
  else (s.requestFullscreen || s.webkitRequestFullscreen).call(s)?.catch?.(() => {});
};
/* Show: the screen on, and full screen at once (the same tap: browsers allow full screen only from one); Exit leaves full
 * screen with the picture still here, Stop ends it */
$("scrgo").addEventListener("click", () => {
  if (scr.on) { screenStop(); return; }
  if (state.target === "emu" || (state.dev && state.dev.screen === true)) fullscreen();   // (unsure: the page first, Full screen after)   // (only a picture that will come goes full screen)
  screenStart();
});
$("scrfull").addEventListener("click", async () => { if (!scr.on) await screenStart(); fullscreen(); });
$("fsexit").addEventListener("click", fullscreen);
const sharp = () => {
  const on = $("stage").classList.toggle("smooth") === false;
  $("scrsharp").setAttribute("aria-pressed", String(on));
  try { localStorage.setItem("jiant.studio.smooth", on ? "0" : "1"); } catch (_) {}
};
$("scrsharp").addEventListener("click", sharp); $("fssharp").addEventListener("click", sharp);
try { if (localStorage.getItem("jiant.studio.smooth") === "1") { $("stage").classList.add("smooth"); $("scrsharp").setAttribute("aria-pressed", "false"); } } catch (_) {}
let fade = 0;
const showBar = () => { $("fsbar").classList.remove("faded"); clearTimeout(fade); fade = setTimeout(() => $("fsbar").classList.add("faded"), 2500); };
$("stage").addEventListener("pointerdown", showBar);
document.addEventListener("fullscreenchange", () => { if (document.fullscreenElement) showBar(); });
if (QS.has("screen")) { state.view = "screen"; }

/* ------------------------------------------------------------------------------------------------ recording */
/* JIANT's USB audio profiles (MENU > SYSTEM > USB, then unplug and plug again): what each sends to the computer or the
 * phone, channel by channel (docs/MANUAL.md "Stems", "USB audio in", "Stems with a voice"; android/README.md) */
const PROFILES = {
  master: { ch: 2, lanes: [["L", 0, -1], ["R", 0, 1]] },
  stems: { ch: 8, lanes: [["T1", 1], ["T2", 2], ["T3", 3], ["T4", 4], ["T5", 5], ["T6", 6], ["FX L", 7, -1], ["FX R", 7, 1]] },
  stemsin: { ch: 10, lanes: [["T1", 1], ["T2", 2], ["T3", 3], ["T4", 4], ["T5", 5], ["T6", 6], ["FX L", 7, -1], ["FX R", 7, 1], ["VOX", 8], ["DRY", 9]] },
};
const LANE_COLORS = { fm: ["#efe6c8", "#9d97ff", "#d39bff", "#ff9fd2", "#ff9a8a", "#ffc07a", "#fff3a8", "#3fe0e6", "#7be0a8", "#a39bb6"],
  byte: ["#f2f0f7", "#ff7a1a", "#a77bff", "#ff3b3b", "#6bc8ff", "#ffd23f", "#f2f0f7", "#ff6fae", "#6bff9e", "#9d94b8"] };
const rec = { on: false, mon: false, stream: null, ac: null, node: null, chunks: [], ch: 2, rate: 44100, t0: 0, midi: [], takes: [], clock: 0,
  profile: "master", engines: [] };
const daw = new Daw($("daw"), { get mute() { return t("recMute"); }, get solo() { return t("recSolo"); } });
const profileOf = (ch) => { const p = $("recprof").value; return p !== "auto" ? p : ch >= 10 ? "stemsin" : ch >= 8 ? "stems" : "master"; };
/* the six tracks' engines, for the lanes' names (27 TRACK: selected, NTRK, then per track engine, preset, level v14,
   mute, armed) */
async function trackNames(emu = false) {
  const d = state.dev, L = emu ? projLink() : d && state.target === "fm1" ? { link: d.link } : null;
  if (!L) return [];
  try {
    const engines = emu ? parseInfo(await L.link.request(CMD.INFO, [], 900)).engines : d.engines;
    const a = await L.link.request(27, [], 600), n = a[1] | 0;
    return Array.from({ length: n }, (_, k) => engines[a[2 + k * 6]] || "");
  } catch (_) { return []; }
}
function lanesFor(profile, ch) {
  const P = PROFILES[profile], col = LANE_COLORS[state.fw];
  const lanes = P.lanes.slice(0, Math.max(ch, 1)).map(([name, c, pan], i) => {
    const e = /^T\d$/.test(name) ? rec.engines[+name[1] - 1] : "", eng = e && e !== "-" ? e : "";
    return { name: eng ? `${name} · ${eng}` : name, sub: /^T\d$/.test(name) ? t("laneTrack") : t("lane_" + name.replace(" ", "")), color: col[c] || col[0], pan: pan || 0 };
  });
  for (let i = lanes.length; i < ch; i++) lanes.push({ name: `CH ${i + 1}`, sub: "", color: col[9], pan: 0 });
  return lanes;
}
function profileHelp() {
  const p = $("recprof").value, d = $("recdev").value;
  $("profhelp").textContent = d === "emu-stems" ? t("recEmuStemsHelp") + ($("midisrc").value === "emu-midi" ? " " + t("recEmuMidiHelp") : "") :
    d === "emu" ? t("recSrcEmu") : t("profHelp_" + p);
}
$("recprof").addEventListener("change", () => { profileHelp(); if (rec.mon && !rec.on) monitorStop(); });
/* the input open and shown in the lanes (levels only) until Record, which keeps what comes in */
/* the emulator's tap (emu/index.html jiantEmu.tap): its stems as the input (8 channels, STEMS' lanes) and/or its MIDI
   OUT, each at its sample (rec.at0: the take's first sample) */
const emuApi = () => { try { const e = frames.emu.el.contentWindow.jiantEmu; return e && e.on() && e.tap ? e : null; } catch (_) { return null; } };
const emuWhyNot = () => t(emuOn() ? "recEmuOld" : "recEmuOff");
function onTap(m) {
  if (rec.src === "emu-stems") {
    if (rec.on) { if (rec.at0 == null) rec.at0 = m.at; rec.chunks.push(m.planes); daw.live(m.planes, rec.rate); }
    else daw.meters(m.planes.map((p) => { let v = 0; for (let i = 0; i < p.length; i += 8) { const a = Math.abs(p[i]); if (a > v) v = a; } return v; }));
  }
  if (rec.on && rec.emuMidi) for (const e of m.midi) {
    if (e.data[0] < 0x80 || e.data[0] >= 0xF0) continue;
    const t0 = rec.at0 != null ? (e.at - rec.at0) / 44.1 : performance.now() - rec.t0;
    if (t0 >= 0) rec.midi.push({ t: t0, data: e.data });
  }
}
async function monitorStart() {
  if (rec.mon || state.busy) return true;
  let stream;
  if ($("recdev").value === "emu-stems") {
    loadFrame("emu");
    const e = emuApi();
    if (!e) { $("recstat").textContent = emuWhyNot(); return false; }
    rec.tapStop = e.tap(onTap);
    rec.engines = state.target === "emu" ? await trackNames(true) : [];
    Object.assign(rec, { mon: true, src: "emu-stems", stream: null, ac: null, node: null, ch: 8, rate: 44100, profile: "stems" });
    daw.setLanes(lanesFor("stems", 8));
    $("recstat").textContent = `8${t("recChannels")} · 44100 Hz`;
    $("recwarn").hidden = true;
    render();
    return true;
  }
  rec.src = $("recdev").value;
  const want = PROFILES[$("recprof").value === "auto" ? "stemsin" : $("recprof").value].ch;
  try {
    if (rec.src === "emu") {
      loadFrame("emu");
      const w = frames.emu.el.contentWindow;
      stream = w && w.jiantEmu && w.jiantEmu.stream && w.jiantEmu.stream();
      if (!stream) { $("recstat").textContent = t("recSrcEmu"); return false; }
    } else {
      const id = $("recdev").value;
      if (MOCK && id === "mock-stems") stream = "mock";
      else {
      if (!id || id === "ask") { await renderAudioInputs(true); return false; }
      const base = { deviceId: { exact: id }, echoCancellation: false, noiseSuppression: false, autoGainControl: false, sampleRate: { ideal: 44100 } };
      try { stream = await navigator.mediaDevices.getUserMedia({ audio: { ...base, channelCount: want > 2 ? { min: 2, ideal: want } : { exact: 2 } } }); }
      catch (_) { stream = await navigator.mediaDevices.getUserMedia({ audio: { ...base, channelCount: { ideal: want } } }); }
      }
    }
  } catch (e) { $("recstat").textContent = e.message; return false; }
  const ac = new AudioContext({ latencyHint: "playback" });
  const src = stream === "mock" ? mockStems(ac) : ac.createMediaStreamSource(stream);
  const ch = stream === "mock" ? 10 : Math.max(1, Math.min(16, stream.getAudioTracks()[0]?.getSettings?.().channelCount || src.channelCount || 2));
  await ac.audioWorklet.addModule("recorder-worklet.js");
  const node = new AudioWorkletNode(ac, "studio-recorder", { numberOfInputs: 1, numberOfOutputs: 0, channelCount: ch, channelCountMode: "explicit", channelInterpretation: "discrete" });
  src.connect(node);
  rec.engines = await trackNames();
  const profile = rec.src === "emu" ? "master" : profileOf(ch);
  Object.assign(rec, { mon: true, stream, ac, node, ch, rate: ac.sampleRate, profile });
  daw.setLanes(lanesFor(profile, ch));
  node.port.onmessage = (e) => {
    const c = e.data.chunk;
    if (!c) return;
    if (rec.on) { rec.chunks.push(c); daw.live(c, rec.rate); }
    else daw.meters(c.map((p) => { let m = 0; for (let i = 0; i < p.length; i += 8) { const v = Math.abs(p[i]); if (v > m) m = v; } return m; }));
  };
  const need = PROFILES[profile].ch, got = ch;
  $("recstat").textContent = `${got}${t("recChannels")} · ${rec.rate} Hz`;
  const warn = $("recwarn");
  warn.hidden = !(rec.src !== "emu" && (got < need || ($("recprof").value !== "auto" && got < PROFILES[$("recprof").value].ch)));
  warn.textContent = t("recFewerLong").replace("%got", got).replace("%need", PROFILES[$("recprof").value === "auto" ? profile : $("recprof").value].ch);
  render();
  return true;
}
function monitorStop() {
  if (!rec.mon) return;
  if (rec.on) recStop();
  rec.mon = false;
  if (rec.tapStop) { rec.tapStop(); rec.tapStop = null; }
  if (rec.stream && rec.src !== "emu" && rec.stream !== "mock") rec.stream.getTracks().forEach((x) => x.stop());
  if (rec.ac) rec.ac.close().catch(() => {});
  render();
}
$("recmon").addEventListener("click", () => (rec.mon ? monitorStop() : monitorStart()));
async function recStart() {
  if (rec.on || state.busy) return;
  if (!rec.mon && !(await monitorStart())) return;
  daw.stop();
  daw.setLanes(lanesFor(rec.profile, rec.ch));
  Object.assign(rec, { on: true, chunks: [], t0: performance.now(), midi: [], at0: null, emuMidi: false });
  const mid = $("midisrc").value;
  if (mid === "emu-midi") {                                // the emulator's MIDI OUT: by its tap (the stems' own, or one of its own)
    rec.emuMidi = true;
    if (!rec.tapStop) { const e = emuApi(); if (e) rec.tapStop = e.tap(onTap); else $("recstat").textContent = emuWhyNot(); }
  } else if (mid && access) {
    const inp = access.inputs.get(mid);
    if (inp) {
      midiTapInput = inp;
      if (state.dev && state.dev.input.id === inp.id) { const prev = inp.onmidimessage; inp.onmidimessage = (e) => { midiTap(e); prev && prev(e); }; }
      else inp.addEventListener("midimessage", midiTap);
    }
  }
  rec.clock = setInterval(() => {
    const s = Math.floor((performance.now() - rec.t0) / 1000);
    $("recclock").textContent = `${String(Math.floor(s / 60)).padStart(2, "0")}:${String(s % 60).padStart(2, "0")}`;
  }, 200);
  render();
}
function recStop() {
  if (!rec.on) return;
  rec.on = false;
  clearInterval(rec.clock);
  try { rec.node.port.postMessage("flush"); } catch (_) {}
  setTimeout(() => {
    if (midiTapInput) { midiTapInput.removeEventListener("midimessage", midiTap); midiTapInput = null; }
    if (rec.tapStop && rec.src !== "emu-stems") { rec.tapStop(); rec.tapStop = null; }   // (a tap for its MIDI only)
    const take = makeTake();
    if (take) daw.show(take.planes, rec.rate);
    $("recclock").textContent = "00:00";
    $("recstat").textContent = t("recSaved");
    render();
  }, 200);                                                 // (the worklet's last chunk)
}
$("recgo").addEventListener("click", () => (rec.on ? recStop() : recStart()));
$("recplay").addEventListener("click", () => (daw.playing ? daw.stop() : daw.play()));
daw.onState = () => render();
/* ---- a song's stems from a backup (the FM-1's, read over USB, or a file): the emulator's own renderer (emu/stems.js,
   in a Worker) plays it once a track; the result shown in the lanes as a take */
async function stemsFrom(archiveOrNull) {
  const base = TOOLS[state.fw].base + "emu/", say = (x) => { $("ststat").textContent = x; };
  try {
    const BK = await import(base + "fm1backup.js");
    let archive = archiveOrNull;
    if (!archive) {
      const d = state.dev;
      if (!d || state.busy) { say(t("stNeedDev")); return; }
      archive = await BK.captureBackup((r, o) => d.link.request(r[0], r[1], (o && o.timeout) || 2000), d.version,
        (done, total) => say(t("stReading") + Math.round(done / Math.max(1, total) * 100) + " %"));
    }
    const backup = BK.readBackup(archive), wasm = await (await fetch(base + "felucca.wasm")).arrayBuffer();
    const v = $("stsong").value, opts = v === "live" ? { mode: "live", bars: 8 } : { mode: "song", song: +v };
    const out = await new Promise((res, rej) => {
      const w = new Worker(base + "stems.js", { type: "module" });
      w.onmessage = (e) => { const m = e.data; if (m.type === "progress") say(t("stRender") + `${m.step} / ${m.steps}`); else { w.terminate(); m.type === "done" ? res(m) : rej(new Error(m.message)); } };
      w.onerror = (e) => { w.terminate(); rej(new Error(e.message || "stems")); };
      w.postMessage({ wasm, backup, opts }, [wasm]);
    });
    const files = unzipStored(new Uint8Array(out.zip)), col = LANE_COLORS[state.fw], planes = [], lanes = [];
    for (const f of files) {
      const { l, r, rate } = readWav(f.data), m = new Float32Array(l.length);
      for (let i = 0; i < l.length; i++) m[i] = (l[i] + r[i]) * 0.5;
      const k = /^T(\d)/.exec(f.name), n = k ? +k[1] : 0;
      planes.push(m); lanes.push({ name: f.name.replace(".wav", ""), sub: k ? t("laneTrack").replace(", mono", "") : "MIX", color: k ? col[n] : col[0], pan: 0 });
      rec.rate = rate;
    }
    const name = `${state.fw === "byte" ? "JIANT-BYTE" : "JIANT-FM"}-${v === "live" ? "live" : "song" + (+v + 1)}-stems`;
    const take = { name, secs: out.seconds, planes, files: [{ label: t("recStemsZip"), name: name + ".zip", blob: new Blob([out.zip], { type: "application/zip" }) },
      ...files.map((f) => ({ label: f.name.replace(".wav", ""), name: `${name}-${f.name}`, blob: new Blob([f.data], { type: "audio/wav" }), small: true }))] };
    rec.takes.unshift(take);
    daw.setLanes(lanes); daw.show(planes, rec.rate);
    say(t("stDone"));
    render();
  } catch (e) { say(String(e.message || e)); }
}
$("stgo").addEventListener("click", () => stemsFrom(null));
$("stfile").addEventListener("change", async () => { const f = $("stfile").files[0]; if (f) stemsFrom(JSON.parse(await f.text())); $("stfile").value = ""; });
/* the renderer's ZIP is stored (no compression) and its WAVs 16-bit stereo: read back without a library */
function unzipStored(z) {
  const out = [], dv = new DataView(z.buffer, z.byteOffset, z.byteLength);
  for (let o = 0; o + 30 <= z.length && dv.getUint32(o, true) === 0x04034B50;) {
    const size = dv.getUint32(o + 18, true), nl = dv.getUint16(o + 26, true), xl = dv.getUint16(o + 28, true);
    const name = new TextDecoder().decode(z.subarray(o + 30, o + 30 + nl)), start = o + 30 + nl + xl;
    out.push({ name, data: z.slice(start, start + size) }); o = start + size;
  }
  return out;
}
function readWav(b) {
  const dv = new DataView(b.buffer, b.byteOffset, b.byteLength), ch = dv.getUint16(22, true), rate = dv.getUint32(24, true);
  let o = 12; while (o + 8 <= b.length && String.fromCharCode(...b.subarray(o, o + 4)) !== "data") o += 8 + dv.getUint32(o + 4, true);
  const n = Math.floor(dv.getUint32(o + 4, true) / (2 * ch)), l = new Float32Array(n), r = new Float32Array(n);
  for (let i = 0, p = o + 8; i < n; i++, p += 2 * ch) { l[i] = dv.getInt16(p, true) / 32768; r[i] = ch > 1 ? dv.getInt16(p + 2, true) / 32768 : l[i]; }
  return { l, r, rate };
}

function makeTake() {
  const n = rec.chunks.reduce((s, c) => s + c[0].length, 0);
  if (!n && !rec.midi.length) return null;
  const ch = rec.ch, planes = Array.from({ length: ch }, () => new Float32Array(n));
  let o = 0;
  for (const c of rec.chunks) { for (let k = 0; k < ch; k++) planes[k].set(c[k] || c[0], o); o += c[0].length; }
  const stamp = new Date().toISOString().replace(/[:T]/g, "-").slice(0, 19);
  const name = `${state.fw === "byte" ? "JIANT-BYTE" : "JIANT-FM"}-${state.target === "emu" ? "emu" : "fm1"}-${stamp}`;
  const take = { name, secs: n / rec.rate, files: [], planes };
  const lanes = lanesFor(rec.profile, ch);
  if (n) {
    if (rec.profile !== "master" && ch >= 8) {               // STEMS / STEMS+IN: each track, the effects, the voice; their sum
      const sum = [new Float32Array(n), new Float32Array(n)];
      for (let i = 0; i < n; i++) {
        let c = 0; for (let k = 0; k < 6; k++) c += planes[k][i] * 0.5;
        if (ch >= 10) c += planes[8][i] * 0.5;              // (VOX: the voice as the track mixed it, not in its stem)
        sum[0][i] = c + planes[6][i]; sum[1][i] = c + planes[7][i];
      }
      const file = (l) => l.name.replace(/[^A-Za-z0-9]+/g, "-").replace(/-+$/, "");
      const entries = [];
      for (let k = 0; k < Math.min(6, ch); k++) entries.push({ name: `${name}/${file(lanes[k])}.wav`, data: wavFile([planes[k]], rec.rate) });
      entries.push({ name: `${name}/FX.wav`, data: wavFile([planes[6], planes[7]], rec.rate) });
      if (ch >= 10) { entries.push({ name: `${name}/VOX.wav`, data: wavFile([planes[8]], rec.rate) }); entries.push({ name: `${name}/DRY.wav`, data: wavFile([planes[9]], rec.rate) }); }
      entries.push({ name: `${name}/MIX.wav`, data: wavFile(sum, rec.rate) });
      take.files.push({ label: t("recStemsZip"), name: `${name}-stems.zip`, blob: new Blob([zipStored(entries)], { type: "application/zip" }) });
      take.files.push({ label: t("recWav"), name: `${name}.wav`, blob: new Blob([wavFile(sum, rec.rate)], { type: "audio/wav" }) });
      for (const e of entries.slice(0, -1)) take.files.push({ label: e.name.split("/")[1].replace(".wav", ""), name: e.name.replace("/", "-"), blob: new Blob([e.data], { type: "audio/wav" }), small: true });
    } else {
      take.files.push({ label: t("recWav"), name: `${name}.wav`, blob: new Blob([wavFile(planes.slice(0, 2), rec.rate)], { type: "audio/wav" }) });
    }
  }
  if (rec.midi.length) {
    take.files.push({ label: `${t("recMid")} (${rec.midi.filter((m) => (m.data[0] & 0xF0) === 0x90 && m.data[2]).length}${t("recNotes")})`, name: `${name}.mid`,
      blob: new Blob([midiFile(rec.midi)], { type: "audio/midi" }) });
  }
  rec.takes.unshift(take);
  rec.chunks = [];
  return take;
}
let midiTapInput = null;
function midiTap(e) {
  const d = e.data;
  if (!rec.on || !d || !d.length || d[0] < 0x80 || d[0] >= 0xF0) return;
  rec.midi.push({ t: performance.now() - rec.t0, data: Array.from(d) });
}
async function renderAudioInputs(ask = false) {
  const sel = $("recdev"), was = sel.value;
  if (rec.mon) return;                                     // (the input in use stays as it is)
  sel.disabled = false;
  const emuOpts = () => [new Option(t("recEmuStems"), "emu-stems"), new Option(t("recEmuMaster"), "emu")];
  if (state.target === "emu") {
    sel.replaceChildren(...emuOpts());
    sel.value = was === "emu" ? "emu" : "emu-stems";
    return;
  }
  try {
    if (ask) (await navigator.mediaDevices.getUserMedia({ audio: true })).getTracks().forEach((x) => x.stop());
    const ins = (await navigator.mediaDevices.enumerateDevices()).filter((d) => d.kind === "audioinput");
    const opts = ins.map((d, i) => new Option(d.label || `${t("recInput")} ${i + 1}`, d.deviceId));
    if (!opts.length || !ins.some((d) => d.label)) opts.push(new Option(t("recAsk"), "ask"));
    if (MOCK) opts.unshift(new Option("JIANT FM (simulated: STEMS+IN, 10 ch)", "mock-stems"));
    sel.replaceChildren(...opts, ...emuOpts());
    const fm1 = MOCK ? { deviceId: "mock-stems" } : ins.find((d) => /fm-?1|jiant|felucca|m-?vave/i.test(d.label));
    sel.value = [...sel.options].some((o) => o.value === was) ? was : fm1 ? fm1.deviceId : sel.options[0].value;
  } catch (_) { sel.replaceChildren(new Option(t("recNoDev"), "")); }
}
$("recdev").addEventListener("change", () => { if ($("recdev").value === "ask") renderAudioInputs(true); profileHelp(); });
$("midisrc").addEventListener("change", profileHelp);
function renderMidiInputs() {
  const sel = $("midisrc"), was = sel.value;
  const ins = access ? [...access.inputs.values()].filter((p) => p.state !== "disconnected") : [];
  sel.replaceChildren(new Option(t("recMidiNone"), ""), new Option(t("recEmuMidi"), "emu-midi"), ...ins.map((p) => new Option(p.name || p.id, p.id)));
  const fm1 = ins.find((p) => PORT_RE.test(p.name || ""));
  sel.value = was && [...sel.options].some((o) => o.value === was) ? was : state.target === "emu" ? "emu-midi" : fm1 ? fm1.id : "";
}
$("midisrc").addEventListener("focus", async () => { if (!access && !MOCK) { try { await midiAccess(); } catch (_) {} renderMidiInputs(); } });
function renderTakes() {
  const box = $("takes");
  if (!rec.takes.length) { box.replaceChildren(Object.assign(document.createElement("p"), { className: "small dim", textContent: t("recNone") })); return; }
  box.replaceChildren(...rec.takes.map((tk) => {
    const row = document.createElement("div"); row.className = "take";
    const nm = document.createElement("span"); nm.className = "name"; nm.textContent = `${tk.name} · ${tk.secs.toFixed(1)} s`;
    row.append(nm, ...tk.files.map((f) => {
      const a = document.createElement("a"); a.className = "btn ghost"; a.textContent = f.label; a.download = f.name;
      a.href = f.url || (f.url = URL.createObjectURL(f.blob)); return a;
    }));
    return row;
  }));
}

/* -------------------------------------------------------------------------------------------- other devices */
/* JIANT FM's ports (ports/README.md): each one's state, what it is, and its links: the emulator (emu/devices.html, the
 * same origin: shown here in a frame), the download (the releases), its manual. The words are TEXT's (p<Name>...) */
const GH = "https://github.com/juanjiant-bit/JIANT/blob/main/", RELEASES = "https://github.com/juanjiant-bit/JIANT/releases";
const MAN = "../docs/guide/";            // the manuals in HTML (tools/gen_manual_html.py), this site's
const OUTLINE = {   // a device's outline where there is no picture (the pads, the keys)
  smk: '<svg viewBox="0 0 320 120" fill="none" stroke="currentColor" stroke-width="2"><rect x="4" y="4" width="312" height="112" rx="10"/><rect x="16" y="14" width="44" height="34" rx="3"/>' +
       Array.from({ length: 8 }, (_, i) => `<rect x="${72 + i * 26}" y="16" width="20" height="13" rx="2"/><rect x="${72 + i * 26}" y="34" width="20" height="13" rx="2"/>`).join("") +
       Array.from({ length: 22 }, (_, i) => `<rect x="${16 + i * 13.3}" y="62" width="11" height="46" rx="2"/>`).join("") + "</svg>",
  mpc: '<svg viewBox="0 0 240 150" fill="none" stroke="currentColor" stroke-width="2"><rect x="4" y="4" width="232" height="142" rx="12"/><rect x="16" y="16" width="86" height="54" rx="4"/>' +
       Array.from({ length: 16 }, (_, i) => `<rect x="${118 + (i % 4) * 28}" y="${16 + Math.floor(i / 4) * 31}" width="24" height="27" rx="4"/>`).join("") +
       '<circle cx="30" cy="96" r="10"/><circle cx="60" cy="96" r="10"/><circle cx="90" cy="96" r="10"/><rect x="18" y="118" width="84" height="8" rx="4"/></svg>',
  plug: '<svg viewBox="0 0 240 150" fill="none" stroke="currentColor" stroke-width="2"><rect x="4" y="4" width="232" height="142" rx="8"/><path d="M4 26h232"/><circle cx="16" cy="15" r="3"/><circle cx="28" cy="15" r="3"/>' +
       '<rect x="18" y="40" width="60" height="92" rx="3"/><rect x="88" y="40" width="64" height="64" rx="3"/><circle cx="178" cy="54" r="12"/><circle cx="210" cy="54" r="12"/><rect x="164" y="80" width="60" height="52" rx="3"/></svg>',
};
const PORTS = [
  // shots: the port running (its emulator's panel in the browser, the plugin's editor), the first one on the card
  { id: "smk37", name: "pSmkTitle", text: "pSmkText", points: ["pSmk1", "pSmk2"], state: ["stDev", "soon"], pic: OUTLINE.smk,
    shots: [["img/smk37.webp", "shotSmk"]],
    links: [["portTry", "#try:smk37"], ["portManual", MAN + "smk37.html"], ["portBuild", GH + "docs/SMK37.md"], ["portSoon", null]] },
  { id: "mpc", name: "pMpcTitle", text: "pMpcText", points: ["pMpc1", "pMpc2"], state: ["stPreview", "soon"], pic: OUTLINE.mpc,
    shots: [["img/mpc.webp", "shotMpc"]], whole: true,           // (the unit stands upright: all of it shown)
    links: [["portTry", "#try:mpc"], ["portManual", MAN + "mpc-sample.html"], ["portDownload", RELEASES], ["portInstall", MAN + "mpc-sample.html#release"]] },
  // (the plugins: coming soon, for sale; no download here)
  { id: "mpcplug", name: "pPlugMpcTitle", text: "pPlugMpcText", points: ["pPlugMpc1"], state: ["stSoon", "soon"], pic: OUTLINE.mpc,
    links: [["portManual", MAN + "ports.html#the-mpc-plugin"], ["portSoonBuy", null]] },
  { id: "daw", name: "pDawTitle", text: "pDawText", points: ["pDaw1", "pDaw2", "pDaw3"], state: ["stSoon", "soon"],
    shots: [["img/plugin.webp", "shotDaw"], ["img/plugin-sound.webp", "shotDawSound"], ["img/plugin-samples.webp", "shotDawSamples"]],
    links: [["portManual", MAN + "plugin.html"], ["portSoonBuy", null]] },
];
let docsFor = "";
function renderPorts() {
  const el = (tag, cls, text) => { const e = document.createElement(tag); if (cls) e.className = cls; if (text != null) e.textContent = text; return e; };
  $("portlist").replaceChildren(...PORTS.map((p) => {
    const card = el("article", "card port");
    const pic = el("div", "pic");
    let thumbs = null;
    if (p.shots) {
      // the shot (a tap: larger), and under it a thumbnail each when there are more
      const im = new Image(); im.loading = "lazy"; if (p.whole) im.style.objectFit = "contain";
      const show = (k) => { im.src = p.shots[k][0]; im.alt = t(p.shots[k][1]); pic.dataset.k = k;
        if (thumbs) [...thumbs.children].forEach((b, i) => b.setAttribute("aria-pressed", String(i === k))); };
      const open = el("button", "picopen"); open.type = "button"; open.append(im);
      open.addEventListener("click", () => openShot(p, +pic.dataset.k || 0));
      pic.append(open);
      if (p.shots.length > 1) {
        thumbs = el("div", "thumbs");
        thumbs.append(...p.shots.map(([src, cap], k) => {
          const b = el("button"); b.type = "button"; b.title = t(cap);
          const ti = new Image(); ti.src = src; ti.alt = ""; ti.loading = "lazy"; b.append(ti);
          b.addEventListener("click", () => show(k));
          return b;
        }));
      }
      show(0);
    } else { pic.innerHTML = p.pic; pic.setAttribute("aria-hidden", "true"); }
    const meta = el("div", "meta"); meta.append(el("span", "state " + p.state[1], t(p.state[0])));
    const ul = el("ul"); ul.append(...p.points.map((k) => el("li", null, t(k))));
    const links = el("div", "links");
    links.append(...p.links.map(([k, href], i) => {
      if (!href) return el("span", "btn ghost na", t(k));
      const a = el("a", "btn" + (i ? " ghost" : ""), t(k)); a.href = href;
      if (href.startsWith("#try:")) a.addEventListener("click", (e) => { e.preventDefault(); tryDevice(href.slice(5), t(p.name)); });
      else { a.target = "_blank"; a.rel = "noopener"; }
      return a;
    }));
    card.append(pic, ...(thumbs ? [thumbs] : []), el("h2", null, t(p.name)), meta, el("p", null, t(p.text)), ul, links);
    return card;
  }));
}
/* a port's shots, large: a dialog, its caption, ← → between them */
function openShot(p, k) {
  const dlg = $("shotdlg"), img = $("shotimg"), cap = $("shotcap");
  const show = () => { img.src = p.shots[k][0]; img.alt = p.title || t(p.shots[k][1]); cap.textContent = p.title || `${t(p.name)} · ${t(p.shots[k][1])}`;
    $("shotprev").hidden = $("shotnext").hidden = p.shots.length < 2; };
  dlg._step = (d) => { k = (k + d + p.shots.length) % p.shots.length; show(); };
  show();
  dlg.showModal();
}
$("shotprev").addEventListener("click", () => $("shotdlg")._step(-1));
$("shotnext").addEventListener("click", () => $("shotdlg")._step(1));
$("shotclose").addEventListener("click", () => $("shotdlg").close());
$("shotdlg").addEventListener("click", (e) => { if (e.target === $("shotdlg")) $("shotdlg").close(); });
$("shotdlg").addEventListener("keydown", (e) => { if (e.key === "ArrowLeft") $("shotdlg")._step(-1); else if (e.key === "ArrowRight") $("shotdlg")._step(1); });
/* the device's emulator (the JIANT FM site's emu/devices.html, its panel picked by the hash) in the frame below */
function tryDevice(dev, name) {
  const f = $("f-devices"), url = `${TOOLS.fm.base}emu/devices.html#${dev}`;
  $("devtry").hidden = false; $("devtrytitle").textContent = `${t("portTry")} · ${name}`;
  if (f.src !== new URL(url, location.href).href) f.src = url;
  $("devtry").scrollIntoView({ behavior: "smooth", block: "start" });
}
$("f-devices").addEventListener("load", () => {
  const f = $("f-devices");
  dress(f);
  try {   // inside Studio: straight to the devices and their panel (its own bar, title, plugins note and footer are Studio's)
    const d = f.contentDocument, st = d.createElement("style");
    st.textContent = "main>header.top,main>section:first-of-type,section.plugins,main>footer,body>footer{display:none!important}";
    d.head.append(st);
    // the frame as tall as the page in it: the whole device and its screen larger, no scrolling inside
    const fit = () => { f.style.height = `${Math.max(480, Math.ceil(d.body.getBoundingClientRect().bottom + f.contentWindow.scrollY) + 16)}px`; };
    new f.contentWindow.ResizeObserver(fit).observe(d.body); fit();
  } catch (_) {}
});
$("devtryclose").addEventListener("click", () => { $("devtry").hidden = true; $("f-devices").src = "about:blank"; });

/* ------------------------------------------------------------------------------------- projects and backups */
/* By hand: the device's project slots A-D (9 PROJECT: 0 load, 1 save, 2 query) and complete backups (65-67, fm1backup.js:
 * the installer's and the editor's), from the FM-1 or the emulator (its editor handler answers the same). Backups are
 * kept in this browser (IndexedDB) and, if asked, downloaded; restoring one tells the other tools on jiant-midi to let go
 * of the FM-1 first (as the installer does) */
const proj = { slots: [null, null, null, null], working: false, list: [], emuLink: null };
const SLOT = ["A", "B", "C", "D"];
function projLink() {
  if (state.target === "fm1") return state.dev ? { link: state.dev.link, name: devName(state.dev) } : null;
  if (!emuOn()) return null;
  if (!proj.emuLink || proj.emuLink.closed) {
    const m = emuMidi(), input = [...m.inputs.values()][0], output = [...m.outputs.values()][0];
    const link = proj.emuLink = new Link(output, () => {});
    input.addEventListener("midimessage", (e) => link.receive(e.data));
  }
  return { link: proj.emuLink, name: t("bkTarget") };
}
/* fm1backup.js's request ([cmd, args], {timeout, retries, match}) over a Link */
const bkRequest = (link) => async ([cmd, args], o = {}) => {
  for (let k = 0; ; k++) {
    try { return await link.request(cmd, args, o.timeout || 1200, o.match || null); }
    catch (e) { if (e.message !== "timeout" || k >= (o.retries || 0)) throw new Error(e.message === "timeout" ? "timeout" : e.message); }
  }
};
/* the library: IndexedDB (a memory list where it is not allowed) */
const bkDb = (() => {
  let db = null, mem = [], memId = 1;
  const open = () => db || (db = new Promise((res) => {
    try {
      const r = indexedDB.open("jiant-studio", 1);
      r.onupgradeneeded = () => r.result.createObjectStore("backups", { keyPath: "id", autoIncrement: true });
      r.onsuccess = () => res(r.result); r.onerror = () => res(null);
    } catch (_) { res(null); }
  }));
  const tx = async (mode, f) => { const d = await open(); if (!d) return null;
    return new Promise((res, rej) => { const st = d.transaction("backups", mode).objectStore("backups"), r = f(st); r.onsuccess = () => res(r.result); r.onerror = () => rej(r.error); }); };
  return {
    async all() { const a = await tx("readonly", (st) => st.getAll()); return (a || mem).slice().sort((x, y) => y.created.localeCompare(x.created)); },
    async put(b) { const id = await tx("readwrite", (st) => st.put(b)); if (id == null) { if (!b.id) b.id = memId++; mem = mem.filter((x) => x.id !== b.id).concat(b); return b.id; } return id; },
    async del(id) { const r = await tx("readwrite", (st) => st.delete(id)); if (r === null) mem = mem.filter((x) => x.id !== id); },
  };
})();
/* what a backup holds, in a line: the current music, the songs (6 x 4 sections from id 40), its size */
function bkSummary(file) {
  const objs = file.objects || [], size = objs.reduce((n, o) => n + (o.size || 0), 0);
  const songs = new Map();
  for (const o of objs) if (o.id >= 40 && o.size) { const n = ((o.id - 40) >> 2) + 1; songs.set(n, (songs.get(n) || 0) + 1); }
  const secs = [...songs.values()].reduce((a, b) => a + b, 0);
  const parts = [];
  if (objs.find((o) => o.id === 0 && o.size)) parts.push(t("bkMusic"));
  if (songs.size) parts.push(`${t("bkSongs").replace("%n", songs.size)} (${t("bkSections").replace("%n", secs)})`);
  parts.push(`${(size / 1024).toFixed(1)} KB`);
  return parts.join(" · ");
}
const bkJiant = (file) => { try { readBackup(file); return true; } catch (_) { return false; } };
const stamp = (d) => d.toLocaleString(lang === "es" ? "es-AR" : "en-GB", { day: "2-digit", month: "2-digit", year: "numeric", hour: "2-digit", minute: "2-digit" });
const fileName = (b) => `${(b.name || "backup").replace(/[^\w.-]+/g, "_")}.json`;
function download(blob, name) { const a = document.createElement("a"); a.href = URL.createObjectURL(blob); a.download = name; document.body.append(a); a.click(); a.remove(); setTimeout(() => URL.revokeObjectURL(a.href), 4000); }
function projSay(s) { $("projstat").textContent = s; }
function progress(done, total) { $("bkprog").hidden = !total; $("bkbar").style.width = total ? `${Math.round(done / total * 100)}%` : "0"; }
/* the other tools let go (the editor in its frame, other tabs), the screen stops, then f; then they may come back */
async function exclusive(f) {
  proj.working = true; renderProj();
  try { midiBc && midiBc.postMessage({ type: "installer", busy: true }); } catch (_) {}
  screenStop();
  await new Promise((r) => setTimeout(r, 400));
  try { return await f(); }
  finally {
    try { midiBc && midiBc.postMessage({ type: "installer", busy: false }); } catch (_) {}
    proj.working = false; progress(0, 0); renderProj();
  }
}
async function backupNow() {
  const L = projLink();
  if (!L) { projSay(t("bkNoDev")); return; }
  await exclusive(async () => {
    try {
      projSay(t("bkWorking"));
      const fw = state.target === "emu" ? `${state.fw === "byte" ? "JIANT BYTE" : "JIANT FM"} (emulator)` : L.name;
      let file;
      try { file = await captureBackup(bkRequest(L.link), fw, (d, n) => { progress(d, n); projSay(t("bkWorking") + `${Math.round(d / n * 100)} %`); }); }
      catch (e) {   // (another firmware's list of objects: kept whole, restored with its own editor)
        if (!/manifest|archive object/i.test(e.message)) throw e;
        file = await captureBackup(bkRequest(L.link), fw, (d, n) => progress(d, n), { complete: true });
      }
      const now = new Date(), b = { name: `${fw} · ${stamp(now)}`, created: now.toISOString(), fw: state.fw, firmware: fw, summary: bkSummary(file), file };
      b.id = await bkDb.put(b);
      if ($("bkdl").checked) download(new Blob([JSON.stringify(file)], { type: "application/json" }), fileName(b));
      projSay(t("bkDone").replace("%s", b.name));
    } catch (e) { projSay(t("bkFail").replace("%s", e.message)); }
    await loadList();
  });
}
async function restore(b) {
  const L = projLink();
  if (!L) { projSay(t("bkNoDev")); return; }
  if (!confirm(t("bkConfirm").replace("%s", b.name).replace("%s", L.name))) return;
  await exclusive(async () => {
    try {
      projSay(t("bkRestoring"));
      const r = await restoreBackup(bkRequest(L.link), b.file, (d, n) => { progress(d, n); projSay(t("bkRestoring") + `${Math.round(d / n * 100)} %`); });
      const lost = songsNotRestored(r.notRestored);
      projSay(lost.length ? t("bkRestoredBut").replace("%s", lost.join(", ")) : t("bkRestored").replace("%s", L.name));
    } catch (e) { projSay(t("bkFail").replace("%s", e.message)); }
  });
}
async function loadList() { proj.list = await bkDb.all(); renderProj(); }
$("bkgo").addEventListener("click", backupNow);
$("bkimport").addEventListener("click", () => $("bkfile").click());
$("bkfile").addEventListener("change", async () => {
  const f = $("bkfile").files[0]; $("bkfile").value = "";
  if (!f) return;
  try {
    const file = JSON.parse(await f.text());
    if (!file || file.format !== "felucca-backup" || !Array.isArray(file.objects)) throw new Error(t("bkBadFile"));
    const b = { name: f.name.replace(/\.json$/i, ""), created: file.created || new Date().toISOString(), fw: /BYTE/i.test(file.firmware || "") ? "byte" : "fm",
      firmware: file.firmware || "", summary: bkSummary(file), file };
    await bkDb.put(b); projSay(t("bkImported").replace("%s", b.name));
  } catch (e) { projSay(t("bkFail").replace("%s", e.message)); }
  loadList();
});
/* the slots: read on demand (a query each), Load and Save after asking */
async function readSlots() {
  const L = projLink();
  if (!L) { projSay(t("bkNoDev")); return; }
  for (let k = 0; k < 4; k++) {
    try { const a = await L.link.request(9, [2, k], 1500, (r) => r[1] === k); proj.slots[k] = !!a[2]; } catch (_) { proj.slots[k] = null; }
  }
  renderProj();
}
async function slotDo(k, op) {
  const L = projLink();
  if (!L) { projSay(t("bkNoDev")); return; }
  if (!confirm(t(op ? "slotSaveConfirm" : "slotLoadConfirm").replace("%s", SLOT[k]))) return;
  await exclusive(async () => {
    try {
      const a = await L.link.request(9, [op, k], op ? 6000 : 3000, (r) => r[0] === op && r[1] === k);
      if (op === 0 && !a[2]) throw new Error(t("slotEmpty"));
      projSay(t(op ? "slotSaved" : "slotLoaded").replace("%s", SLOT[k]));
      if (op) proj.slots[k] = true;
    } catch (e) { projSay(t("bkFail").replace("%s", e.message)); }
  });
}
$("slotsread").addEventListener("click", readSlots);
function renderProj() {
  if (!$("v-proj")) return;
  const can = !!projLink() && !state.busy && !proj.working;
  $("bkgo").disabled = !can; $("slotsread").disabled = !can; $("bkimport").disabled = proj.working;
  $("slotshelp").textContent = t("slotsHelp");
  const el = (tag, cls, text) => { const e = document.createElement(tag); if (cls) e.className = cls; if (text != null) e.textContent = text; return e; };
  const btn = (k, cls, f, dis) => { const b = el("button", "btn" + (cls ? " " + cls : ""), t(k)); b.type = "button"; b.disabled = !!dis; b.addEventListener("click", f); return b; };
  $("slots").replaceChildren(...SLOT.map((n, k) => {
    const v = proj.slots[k], c = el("div", "slot" + (v ? " used" : ""));
    const row = el("div", "row");
    row.append(btn("slotLoad", "", () => slotDo(k, 0), !can || v === false), btn("slotSave", "ghost", () => slotDo(k, 1), !can));
    c.append(el("b", null, n), el("span", "st", t(v == null ? "slotUnknown" : v ? "slotUsed" : "slotEmpty")), row);
    return c;
  }));
  const list = proj.list.filter((b) => (b.fw || "fm") === state.fw);
  $("bklist").replaceChildren(...(list.length ? list.map((b) => {
    const r = el("div", "bkrow"), name = el("input"); name.value = b.name; name.setAttribute("aria-label", "name");
    name.addEventListener("change", () => { b.name = name.value.trim() || b.name; bkDb.put(b); });
    const left = el("div"); left.append(name, el("div", "when", `${stamp(new Date(b.created))} · ${b.firmware || ""}`));
    const ok = bkJiant(b.file);
    const acts = el("div", "acts");
    acts.append(btn("bkDownload", "ghost", () => download(new Blob([JSON.stringify(b.file)], { type: "application/json" }), fileName(b))),
      btn("bkRestore", "", () => restore(b), !can || !ok), btn("bkDelete", "ghost", async () => { if (confirm(t("bkDelConfirm").replace("%s", b.name))) { await bkDb.del(b.id); loadList(); } }));
    r.append(left, el("div", "what", ok ? b.summary : `${b.summary} · ${t("bkOther")}`), acts);
    return r;
  }) : [el("p", "bkempty", t("bkEmpty"))]));
}

/* ------------------------------------------------------------------------------------------- how it works */
/* each firmware's ideas (learn-fm.json, learn-byte.json): sectors (the concept, the engines, the sequencers, the
 * effects, the modulators, the generators, the songs), each item its statement, how it works, where it is on the
 * FM-1 and its animation (learn.js) */
const lr = { data: {}, fw: null, sec: -1, item: 0, player: null, want: null };   // sec -1: the index
/* #learn/<sector>/<item>: a page of its own (kept for when the data is there) */
function lrRoute(hash) {
  const m = /^#learn\/([\w-]+)(?:\/([\w-]+))?/.exec(hash || "");
  if (!m) return false;
  lr.want = { sec: m[1], item: m[2] || null };
  return true;
}
const lrSecs = () => ((lr.data[state.fw] || {}).sectors || []).filter((x) => x.items && x.items.length);
function lrOpen(si, ii) {
  lr.sec = si; lr.item = ii;
  const S = lrSecs()[si];
  const h = si < 0 || !S ? "#learn" : `#learn/${S.id}/${S.items[ii].id}`;
  if (location.hash !== h) history.replaceState(null, "", location.pathname + location.search + h);
  renderLearn();
}
async function lrLoad(fw) {
  if (!lr.data[fw]) {
    try { const r = await fetch(`learn-${fw}.json`, { cache: "no-cache" }); lr.data[fw] = r.ok ? await r.json() : { sectors: [] }; }
    catch (_) { lr.data[fw] = { sectors: [] }; }
  }
  return lr.data[fw];
}
const L = (o) => (o && typeof o === "object" ? o[lang] || o.es || o.en || "" : o || "");
async function renderLearn() {
  const fw = state.fw, D = await lrLoad(fw);
  if (lr.fw !== fw) { lr.fw = fw; lr.sec = -1; lr.item = 0; }
  if (lr.want && lr.want.sec === "live") { lr.sec = -2; lr.want = null; }
  if (lr.want) {                                         // a link to a page: its sector and item
    const secs0 = (D.sectors || []).filter((x) => x.items && x.items.length), si = secs0.findIndex((x) => x.id === lr.want.sec);
    if (si >= 0) { lr.sec = si; lr.item = Math.max(0, secs0[si].items.findIndex((x) => x.id === lr.want.item)); }
    lr.want = null;
  }
  $("lrtitle").textContent = t(fw === "byte" ? "lrTitleByte" : "lrTitleFm");
  $("lrintro").textContent = L(D.intro) || t("lrNone");
  const secs = (D.sectors || []).filter((x) => x.items && x.items.length);
  const chip = (label, n, on, f) => { const b = document.createElement("button"); b.type = "button"; b.setAttribute("aria-pressed", String(on)); b.append(label);
    if (n != null) { const c = document.createElement("b"); c.textContent = n; b.append(c); } b.addEventListener("click", f); return b; };
  $("lrsec").replaceChildren(chip(t("lvChip"), null, lr.sec === -2, () => { lr.sec = -2; history.replaceState(null, "", location.pathname + location.search + "#learn/live"); renderLearn(); }),
    chip(t("lrIndex"), secs.reduce((a, x) => a + x.items.length, 0), lr.sec === -1, () => lrOpen(-1, 0)),
    ...secs.map((x, i) => chip(L(x.name), x.items.length, i === lr.sec, () => lrOpen(i, 0))));
  const S = secs[lr.sec];
  $("lrlive").hidden = lr.sec !== -2;
  $("lrcard").hidden = !S; $("lritems").hidden = !S; $("lrindex").hidden = !!S || !secs.length || lr.sec === -2;
  if (lr.sec === -2) { $("lrlead").textContent = t("lvLead"); await liveGloss(fw); renderLive(); return; }
  if (!S) { $("lrlead").textContent = secs.length ? t("lrIndexLead") : ""; lrIndex(secs); return; }
  $("lrlead").textContent = L(S.lead);
  $("lritems").replaceChildren(...S.items.map((it, i) => {
    const b = document.createElement("button"); b.type = "button"; b.setAttribute("role", "option"); b.setAttribute("aria-selected", String(i === lr.item));
    const n = document.createElement("b"), g = document.createElement("span"); n.textContent = it.name; g.textContent = L(it.tag); b.append(n, g);
    b.addEventListener("click", () => { lrOpen(lr.sec, i); if (matchMedia("(max-width: 820px)").matches) $("lrcard").scrollIntoView({ behavior: "smooth", block: "start" }); });
    return b;
  }));
  const it = S.items[Math.min(lr.item, S.items.length - 1)];
  $("lrname").textContent = it.name; $("lrtag").textContent = L(it.tag);
  $("lrstate").textContent = L(it.statement);
  const how = (it.how && (it.how[lang] || it.how.es || it.how.en)) || [];
  $("lrhow").replaceChildren(...how.map((h) => Object.assign(document.createElement("li"), { textContent: h })));
  const where = L(it.where); $("lrwhere").hidden = !where; $("lrwhere").textContent = where ? t("lrWhere") + where + (it.since ? ` · ${t("lrSince").replace("%s", it.since)}` : "") : "";
  $("lrpos").textContent = `${lr.item + 1} / ${S.items.length}`;
  lr.player = lr.player || new Player($("lrcanvas"));
  lr.player.show(it.anim && it.anim.kind, it.anim && it.anim.params);
  const sel = $("lritems").children[lr.item]; if (sel && sel.scrollIntoView) sel.scrollIntoView({ block: "nearest", inline: "nearest" });
}
function lrStep(d) {
  const secs = lrSecs(), S = secs[lr.sec];
  if (!S) return;
  let si = lr.sec, ii = lr.item + d;
  if (ii < 0) { si = (si - 1 + secs.length) % secs.length; ii = secs[si].items.length - 1; }
  else if (ii >= S.items.length) { si = (si + 1) % secs.length; ii = 0; }
  lrOpen(si, ii);
}
/* the index: every sector, every item a tile (its animation still, its name and line); a tap opens its page */
let lrIndexFor = "";
function lrIndex(secs) {
  const key = state.fw + lang + document.documentElement.dataset.skin;
  if (lrIndexFor === key) return;
  lrIndexFor = key;
  const el = (tag, cls, text) => { const e = document.createElement(tag); if (cls) e.className = cls; if (text != null) e.textContent = text; return e; };
  const tiles = [];
  $("lrindex").replaceChildren(...secs.map((S, si) => {
    const sec = el("section", "lrisec");
    sec.append(el("h2", null, L(S.name)), el("p", "dim", L(S.lead)));
    const grid = el("div", "lrigrid");
    grid.append(...S.items.map((it, ii) => {
      const b = el("button", "lritile"); b.type = "button";
      const cv = el("canvas"); tiles.push([cv, it.anim || {}]);
      b.append(cv, el("b", null, it.name), el("span", null, L(it.tag)));
      b.addEventListener("click", () => { lrOpen(si, ii); window.scrollTo({ top: $("v-learn").offsetTop - 10, behavior: "smooth" }); });
      return b;
    }));
    sec.append(grid);
    return sec;
  }));
  // their stills, drawn once each as they come into sight
  const io = "IntersectionObserver" in window ? new IntersectionObserver((es) => es.forEach((e) => { if (e.isIntersecting) { io.unobserve(e.target); e.target._draw(); } })) : null;
  for (const [cv, a] of tiles) {
    cv._draw = () => lrStill(cv, a.kind, a.params);
    io ? io.observe(cv) : cv._draw();
  }
}
function lrStill(cv, kind, params) {
  const r = cv.getBoundingClientRect(), d = Math.min(2, devicePixelRatio || 1), W = Math.max(1, r.width), H = Math.max(1, r.height);
  cv.width = Math.round(W * d); cv.height = Math.round(H * d);
  const c = cv.getContext("2d"), VW = 520, k = W / VW;   // (drawn at the card's size, then scaled down: the same picture)
  c.setTransform(d * k, 0, 0, d * k, 0, 0);
  const s = getComputedStyle(cv), v = (n, f) => s.getPropertyValue(n).trim() || f;
  try { anim(kind)(c, 1.3, VW, H / k, params || {}, { fg: v("--fg", "#efe6c8"), dim: v("--dim", "#a39bb6"), acc: v("--accent", "#ff7a1a"), a2: v("--accent-2", "#ff9a8a"), bg: v("--bg", "#000") }); } catch (_) {}
}
$("lrprev").addEventListener("click", () => lrStep(-1));
$("lrback").addEventListener("click", () => lrOpen(-1, 0));
$("lrnext").addEventListener("click", () => lrStep(1));
document.addEventListener("keydown", (e) => { if (state.view !== "learn" || /input|select|textarea/i.test(e.target.tagName)) return; if (e.key === "ArrowRight") lrStep(1); else if (e.key === "ArrowLeft") lrStep(-1); });

/* ---------------------------------------------------------------------------------------- the quick tour */
/* The first time Studio opens (and from the ? button): each part in turn, lit while the rest dims, a card telling what
 * it is, what it does and how to use it; Next / Back / Skip, the arrow keys, Esc. Remembered in this browser */
const TOUR = [
  { k: "Welcome" },
  { k: "Fw", sel: "#fwseg" },
  { k: "Target", sel: "#targetseg" },
  { k: "Dev", sel: "#devchip", view: "home" },
  { k: "Find", sel: "#find", view: "home" },
  ...["install", "editor", "screen", "emu", "rec", "proj", "learn", "docs", "rescue", "ports"].map((v) => ({ k: "V_" + v, sel: `nav.views [data-go="${v}"]` })),
  { k: "Lang", sel: "#lang" },
  { k: "End", sel: "#tourbtn" },
];
const tour = { i: 0, on: false };
const TOUR_KEY = "jiant.studio.tour.1";
function tourShow(i) {
  tour.i = Math.max(0, Math.min(TOUR.length - 1, i));
  const st = TOUR[tour.i];
  if (st.view && state.view !== st.view) go(st.view);
  $("tour").hidden = false; tour.on = true;
  $("tourstep").textContent = `${tour.i + 1} / ${TOUR.length}`;
  $("tourtitle").textContent = t("tour" + st.k + "T"); $("tourwhat").textContent = t("tour" + st.k);
  const how = TEXT[lang]["tour" + st.k + "H"]; $("tourhow").textContent = how || ""; $("tourhow").hidden = !how;
  $("tourprev").disabled = tour.i === 0; $("tournext").textContent = t(tour.i === TOUR.length - 1 ? "tourDone" : "tourNext");
  $("tourdots").replaceChildren(...TOUR.map((_, k) => Object.assign(document.createElement("i"), { className: k === tour.i ? "on" : "" })));
  const el = st.sel && document.querySelector(st.sel);
  const hole = $("tourhole"), card = $("tourcard");
  if (el && el.getClientRects().length) {
    el.scrollIntoView({ block: "nearest", inline: "center" });
    requestAnimationFrame(() => {
      const r = el.getBoundingClientRect(), pad = 6;
      Object.assign(hole.style, { left: `${r.left - pad}px`, top: `${r.top - pad}px`, width: `${r.width + 2 * pad}px`, height: `${r.height + 2 * pad}px`, display: "" });
      const cw = card.offsetWidth, ch = card.offsetHeight, below = r.bottom + 14 + ch < innerHeight;
      card.style.top = `${below ? r.bottom + 14 : Math.max(12, r.top - 14 - ch)}px`;
      card.style.left = `${Math.max(12, Math.min(innerWidth - cw - 12, r.left + r.width / 2 - cw / 2))}px`;
    });
  } else {
    hole.style.display = "none";
    requestAnimationFrame(() => { card.style.top = `${Math.max(12, (innerHeight - card.offsetHeight) / 2)}px`; card.style.left = `${(innerWidth - card.offsetWidth) / 2}px`; });
  }
  $("tournext").focus();
}
function tourEnd() { $("tour").hidden = true; tour.on = false; try { localStorage.setItem(TOUR_KEY, "1"); } catch (_) {} }
$("tournext").addEventListener("click", () => (tour.i === TOUR.length - 1 ? tourEnd() : tourShow(tour.i + 1)));
$("tourprev").addEventListener("click", () => tourShow(tour.i - 1));
$("tourskip").addEventListener("click", tourEnd);
$("tourbtn").addEventListener("click", () => tourShow(0));
document.addEventListener("keydown", (e) => {
  if (!tour.on) return;
  if (e.key === "Escape") tourEnd(); else if (e.key === "ArrowRight") $("tournext").click(); else if (e.key === "ArrowLeft") tourShow(tour.i - 1); else return;
  e.preventDefault(); e.stopPropagation();
}, true);
addEventListener("resize", () => { if (tour.on) tourShow(tour.i); });

/* -------------------------------------------------------------------------------- how it works: live */
/* Following the FM-1 (or the emulator): WATCH 31 and a PING a second keep its pushes coming; a knob turned (CHANGED)
 * is named by its DESC (cached per engine) and explained by the firmware's glossary (live-fm.json, live-byte.json:
 * the label's meaning, by engine where it differs), with the guide's related element (its animation); RELOAD (engine,
 * preset, track), STEP_CHANGED, TRACK_CHANGED, TRANSPORT, PUNCH and MUTATE are told too, and UI (43, page and buttons)
 * when the firmware sends it. The last ones in a list */
/* the UI push's numbers (EDITOR_PROTOCOL.md: push 43, the same in JIANT FM and JIANT BYTE) */
const UI_SECTIONS = ["HOME", "EDIT", "SEQ", "FX", "GLO", "ENV", "LFO", "SCL", "ARP", "SAVE"];
const UI_LAYERS = ["", "FX", "GLO", "SCL", "EDIT", "SEQ", "REC", "SAVE", "MACRO"];
const UI_BUTTONS = ["FX", "SCL", "ENV", "LFO", "EDIT", "GLO", "HOME", "SAVE", "ARP", "SEQ", "PLAY", "REC", "OCT−", "OCT+"];
const live = { on: false, link: null, timer: 0, desc: new Map(), engine: -1, engines: [], track: 0, gloss: {}, log: [], last: null, player: null, shown: null, status: "" };
const v14 = (a, i) => (a[i] | (a[i + 1] << 7)) - 8192;
function readStr(a, i) { let s = ""; while (i < a.length && a[i]) s += String.fromCharCode(a[i++]); return [s, i + 1]; }
function parseDesc(a) {
  const o = { scope: a[0], id: a[1], fmt: a[2], min: v14(a, 3), max: v14(a, 5), def: v14(a, 7) };
  let i = 9; [o.label, i] = readStr(a, i); [o.unit, i] = readStr(a, i);
  o.names = [];
  if (o.fmt === 8) for (let k = 0; k <= o.max - o.min && k < 32 && i < a.length; k++) { let n; [n, i] = readStr(a, i); o.names.push(n); }
  return o;
}
async function liveGloss(fw) {
  if (!live.gloss[fw]) { try { const r = await fetch(`live-${fw}.json`, { cache: "no-cache" }); live.gloss[fw] = r.ok ? await r.json() : {}; } catch (_) { live.gloss[fw] = {}; } }
  return live.gloss[fw];
}
function liveSource() {
  if (state.target === "fm1") return state.dev ? { link: state.dev.link, name: devName(state.dev), engines: state.dev.engines } : null;
  const L = projLink(); return L ? { link: L.link, name: t("bkTarget"), engines: null } : null;
}
async function liveStart() {
  const S = liveSource();
  if (!S) { live.status = t("lvNoDev"); renderLive(); return; }
  live.on = true; live.link = S.link; live.desc.clear(); live.status = t("lvWaiting").replace("%s", S.name); renderLive();
  try {
    const info = parseInfo(await S.link.request(CMD.INFO, [], 900));
    live.engines = S.engines || info.engines; live.pe0 = info.pe0;
    await S.link.request(22, [63], 900);                     // WATCH: every push this firmware has (bit 5: UI, 43)
    const d = await S.link.request(4, [], 1500);             // DUMP: the selected track's engine
    live.engine = d[0];
  } catch (_) {}
  await lrLoad(state.fw); await liveGloss(state.fw);
  clearInterval(live.timer);
  let tick = 0;                                              // (a PING a second; WATCH again every 5 s: another tool's WATCH 0 ends it)
  live.timer = setInterval(() => { if (!live.on || state.busy) return; live.link.request(++tick % 5 ? CMD.PING : 22, tick % 5 ? [] : [63], 600).catch(() => {}); }, 1000);
  renderLive();
}
function liveStop() {
  live.on = false; clearInterval(live.timer);
  try { if (live.link) live.link.request(22, [0], 600).catch(() => {}); } catch (_) {}
  live.status = ""; renderLive();
}
const engName = (e) => { const n = live.engines && live.engines[e]; return n && n !== "-" ? n : ""; };
/* a push, as it comes in (from Link.receive) */
async function livePush(f, link) {
  if (!live.on || link !== live.link) return;
  const a = f.a;
  if (f.cmd === 23 || f.cmd === 32) {                        // a parameter: this track's (CHANGED) or another's (TRACK_CHANGED)
    const other = f.cmd === 32, scope = other ? 0 : a[0], id = a[1], v = v14(a, 2);
    const key = `${scope}:${id}:${scope ? "" : live.engine}`;
    let d = live.desc.get(key);
    if (!d) {
      try { d = parseDesc(await link.request(5, [scope, id], 800)); live.desc.set(key, d); }
      catch (_) { d = { label: `#${id}`, min: 0, max: 127, names: [], unit: "" }; }
    }
    liveShow({ kind: "param", d, v, scope, track: other ? a[0] : null });
  } else if (f.cmd === 24) {                                  // RELOAD: engine, preset (, the selected track)
    live.engine = a[0]; if (a.length > 2) live.track = a[2];
    for (const k of [...live.desc.keys()]) if (k.startsWith("0:")) live.desc.delete(k);
    liveShow({ kind: "reload", engine: a[0], preset: a[1], track: a[2] });
  } else if (f.cmd === 26) liveShow({ kind: "step", index: a[0] });
  else if (f.cmd === 40) liveShow({ kind: "transport", playing: a[0], beat: a[1], bar: a[2] | (a[3] << 7) }, true);
  else if (f.cmd === 41) liveShow({ kind: "punch", bits: a[0] | a[1] << 7 | a[2] << 14 | a[3] << 21 });
  else if (f.cmd === 42) liveShow({ kind: "mutate", track: a[0] });
  else if (f.cmd === 43) {                                    // UI (when the firmware sends it): section, page, layer, button, knobs
    let i = 1, page; const knobs = [];
    [page, i] = readStr(a, i);
    const layer = a[i], button = a[i + 1]; i += 2;
    for (let k = 0; k < 4 && i < a.length; k++) { let s1; [s1, i] = readStr(a, i); knobs.push(s1); }
    // (a button's press or release: told as such; a page or layer change: the page)
    const pressed = button != null && button !== 127 ? { b: button & 63, up: !!(button & 64) } : null;
    const changed = !live.ui || live.ui.page !== page || live.ui.section !== a[0] || live.ui.layer !== layer;
    live.ui = { section: a[0], page, layer, knobs };
    if (changed) liveShow({ kind: "ui", section: a[0], page, layer, knobs });
    else if (pressed && !pressed.up) liveShow({ kind: "button", button: pressed.b, page });
  }
}
/* the guide's element whose id or name matches a word (an engine, FILTER, DELAY…) */
function lrFind(word, sector) {
  if (!word) return null;
  const w = String(word).toLowerCase(), D = lr.data[state.fw];
  if (!D) return null;
  const secs = (D.sectors || []).filter((x) => !sector || x.id === sector), all = [];
  for (const sec of secs) for (const it of sec.items) all.push([sec, it]);
  const hit = (f) => { const r = all.find(([, it]) => f(it)); return r ? `${r[0].id}/${r[1].id}` : null; };
  return hit((it) => it.id === w)                                 // an id first, in every sector (slicer: FX's, not audio in's)
    || hit((it) => it.name.toLowerCase() === w)
    || hit((it) => it.id.startsWith(w + "-"))                      // DRUM: drum-x
    || hit((it) => it.name.toLowerCase().split(/[^a-z0-9-]+/).includes(w));
}
function lrItem(path) {
  const [sid, iid] = String(path).split("/"), D = lr.data[state.fw];
  const sec = D && (D.sectors || []).find((x) => x.id === sid);
  return sec && sec.items.find((i) => i.id === iid);
}
/* what an event says: a title, a line, the value, its explanation (what, how) and the guide's element to show */
function liveExplain(e) {
  const G = live.gloss[state.fw] || {}, eng = engName(live.engine), L2 = (o) => (o ? o[lang] || o.es || o.en : null);
  if (e.kind === "param") {
    const d = e.d, engParam = e.scope === 0 && e.track == null && (state.fw !== "fm" || !live.pe0 || d.id >= live.pe0);   // FM: an engine's own knob (INFO's pe0) (a common one with the same label keeps the general text)
    const g = (engParam && G.engines && G.engines[eng] && G.engines[eng][d.label]) || (G.labels && G.labels[d.label]) || null;
    const span = d.max - d.min || 1, frac = Math.max(0, Math.min(1, (e.v - d.min) / span));
    const val = d.names && d.names.length ? d.names[e.v - d.min] || e.v : `${e.v}${d.unit ? " " + d.unit : ""}`;
    const where = e.track != null ? `${t("lvTrack")} ${e.track + 1}` : e.scope ? t("lvGlobal") : `${t("lvTrack")} ${live.track + 1}${eng ? " · " + eng : ""}`;
    const gx = g ? L2(g) : null;
    let learn = g && g.learn ? (lrFind(g.learn) || null) : null;
    if (!learn) learn = lrFind(d.label) || lrFind(eng, "engines") || lrFind(eng);
    return { title: d.label, line: where, value: String(val), frac, what: gx ? gx.what : t("lvUnknown"), how: gx ? gx.how : "", learn };
  }
  if (e.kind === "reload") { const en = engName(e.engine); return { title: en || t("lvSound"), line: `${t("lvTrack")} ${(e.track ?? live.track) + 1} · ${t("lvPreset")} ${e.preset + 1}`, what: t("lvReload"), how: "", learn: lrFind(en, "engines") || lrFind(en) }; }
  if (e.kind === "step") return { title: `${t("lvStep")} ${e.index + 1}`, line: `${t("lvTrack")} ${live.track + 1}`, what: t("lvStepWhat"), how: "", learn: lrFind("seq") || lrFind("step") };
  if (e.kind === "transport") return { title: e.playing ? "PLAY" : "STOP", line: e.playing ? `${t("lvBar")} ${e.bar + 1} · ${e.beat + 1}` : "", what: t(e.playing ? "lvPlay" : "lvStop"), how: "", learn: lrFind("song") };
  if (e.kind === "punch") return { title: "FX PUNCH-IN", line: e.bits ? t("lvPunchOn") : t("lvPunchOff"), what: t("lvPunch"), how: "", learn: lrFind("punch") };
  if (e.kind === "mutate") return { title: "MUTATE", line: `${t("lvTrack")} ${e.track + 1}`, what: t("lvMutate"), how: "", learn: lrFind("mutate") };
  if (e.kind === "ui") {
    const sec = UI_SECTIONS[e.section] || "", lay = e.layer ? UI_LAYERS[e.layer] || "" : "";
    const knobs = (e.knobs || []).map((k, i) => (k ? `K${i + 1} ${k}` : "")).filter(Boolean).join(" · ");
    return { title: e.page || sec || "—", line: [sec !== e.page ? sec : "", lay ? `${t("lvLayer")} ${lay}` : "", knobs].filter(Boolean).join(" · "),
      what: lay ? t("lvLayerWhat").replace("%s", lay) : t("lvPage"), how: "", learn: lrFind(e.page) || lrFind(lay) || lrFind(sec) };
  }
  if (e.kind === "button") {
    const n = UI_BUTTONS[e.button] || "?";
    return { title: n, line: `${t("lvPressed")} · ${e.page || ""}`, what: t("lvButton").replace("%s", n), how: "", learn: lrFind(n) };
  }
  return { title: "?", line: "", what: "", how: "" };
}
function liveShow(e, quiet) {
  const x = liveExplain(e);
  if (!(quiet && live.last && live.last.kind === e.kind)) {
    const same = live.log[0] && live.log[0].title === x.title && live.log[0].line === x.line;
    if (same) live.log[0] = x; else { live.log.unshift(x); if (live.log.length > 12) live.log.length = 12; }
  }
  live.last = { kind: e.kind, x }; live.status = "";
  renderLive();
}
function renderLive() {
  if (!$("lrlive") || state.view !== "learn" || lr.sec !== -2) return;
  $("lvgo").textContent = t(live.on ? "lvStopBtn" : "lvStartBtn");
  const here = live.on && live.ui ? ` ${t("lvNow")} ${[UI_SECTIONS[live.ui.section], live.ui.page].filter((x, i, A) => x && A.indexOf(x) === i).join(" › ")}${live.ui.layer ? ` (${t("lvLayer")} ${UI_LAYERS[live.ui.layer] || ""})` : ""}` : "";
  $("lvstat").textContent = (live.status || (live.on ? t("lvTouch") : t("lvOff"))) + here;
  const x = live.last && live.last.x;
  $("lvcard").hidden = !x;
  if (x) {
    $("lvtitle").textContent = x.title; $("lvline").textContent = x.line || "";
    $("lvval").hidden = x.value == null; $("lvvaltext").textContent = x.value ?? ""; $("lvbar").style.width = `${Math.round((x.frac ?? 0) * 100)}%`;
    $("lvwhat").textContent = x.what || ""; $("lvhow").textContent = x.how || ""; $("lvhow").hidden = !x.how;
    const it = x.learn && lrItem(x.learn);
    $("lvmore").hidden = !it; $("lvstage").hidden = !it;
    if (it) {
      $("lvmore").textContent = `${t("lvMore")} ${it.name} →`; $("lvmore").href = `#learn/${x.learn}`;
      live.player = live.player || new Player($("lvcanvas"));
      if (live.shown !== x.learn) { live.shown = x.learn; live.player.show(it.anim && it.anim.kind, it.anim && it.anim.params); }
    }
  }
  $("lvlog").replaceChildren(...live.log.map((l) => {
    const li = document.createElement("li"), b = document.createElement("b");
    b.textContent = l.title; li.append(b, ` ${l.line || ""}${l.value != null ? " · " + l.value : ""}`);
    return li;
  }));
}
$("lvgo").addEventListener("click", () => (live.on ? liveStop() : liveStart()));

/* ------------------------------------------------------------------------------------------------- rescue */
/* FM-1 Rescue (rescue/, android/rescue): an RP2040 wired to the FM-1's USB-C puts it in the JieLi ROM's boot mode
 * and writes the firmware area again. Here: the tutorial, the board's wiring drawn, and its .uf2 made in the browser
 * (rescue/builder.js: the board's base firmware + the chosen package + JieLi's loader, each checked) */
const RS_STEPS = {
  es: { flash: ["Mantené apretado <b>BOOT</b> en la placa y conectala a la computadora. Aparece una unidad <code>RPI-RP2</code>.",
                "Arrastrá <code>fm1-rescue.uf2</code> a esa unidad. Se cierra sola cuando termina.",
                "Desconectá la placa de la computadora."],
        go: ["Con el FM-1 <b>apagado</b>, conectá los tres cables a la placa (como en el paso 2) y el USB-C al FM-1.",
             "Conectá la placa a la computadora o a un cargador USB. El LED parpadea en <b>amarillo</b>: espera.",
             "<b>Encendé el FM-1.</b> El LED pasa a azul mientras trabaja: entre 1 y 3 minutos. No desconectes nada.",
             "<b>Verde fijo</b> = listo y verificado. Si parpadea en rojo: apagá el FM-1, reconectá la placa y volvé a encenderlo."] },
  en: { flash: ["Hold <b>BOOT</b> on the board and plug it into the computer. An <code>RPI-RP2</code> drive appears.",
                "Drag <code>fm1-rescue.uf2</code> onto that drive. It closes by itself when done.",
                "Unplug the board from the computer."],
        go: ["With the FM-1 <b>off</b>, connect the three wires to the board (as in step 2) and the USB-C to the FM-1.",
             "Plug the board into the computer or a USB charger. The LED blinks <b>yellow</b>: waiting.",
             "<b>Switch the FM-1 on.</b> The LED turns blue while it works: 1 to 3 minutes. Do not unplug anything.",
             "<b>Steady green</b> = done and verified. Blinking red: switch the FM-1 off, re-plug the board, switch it on again."] },
};
const RS_LEDS = {   // [colour, second colour, blink, es, en]
  list: [["#e5c100", "", "blink", ["Amarillo, parpadea", "Esperando: encendé el FM-1 (tiene que estar apagado antes)."], ["Yellow, blinking", "Waiting: switch the FM-1 on (it must be off before)."]],
         ["#e5c100", "", "", ["Amarillo, fijo", "El FM-1 respondió: entrando en modo de arranque."], ["Yellow, steady", "The FM-1 answered: entering boot mode."]],
         ["#2f6fe0", "", "blink", ["Azul, parpadea", "Leyendo y escribiendo. No desconectes nada."], ["Blue, blinking", "Reading and writing. Do not unplug anything."]],
         ["#2e8b57", "", "", ["Verde, fijo", "Terminado y verificado."], ["Green, steady", "Done and verified."]],
         ["#e0443a", "", "fast", ["Rojo, parpadea rápido", "Error: apagá el FM-1, reconectá la placa y encendelo. Si sigue: revisá los cables (probá cambiar verde y blanco) y la batería."], ["Red, fast blinking", "Error: switch the FM-1 off, re-plug the board, switch it on. If it stays: check the wires (try swapping green and white) and the battery."]],
         ["#9b4dca", "", "", ["Violeta, fijo", "El FM-1 está encendido con un firmware que no acepta el modo de arranque: apagalo, reconectá la placa y encendelo."], ["Purple, steady", "The FM-1 runs a firmware that refuses boot mode: switch it off, re-plug the board, switch it on."]],
         ["#e0443a", "#2f6fe0", "alt", ["Rojo y azul, alternando", "El .uf2 está dañado: crealo otra vez."], ["Red and blue, alternating", "The .uf2 is damaged: make it again."]]],
};
/* a board and its three wires to the FM-1's USB-C plug, drawn (SVG): the pads named as printed on the board */
const RS_BOARDS = {
  zero: { name: "RP2040-Zero", pads: ["0", "1", "GND"], text: "rsWireZero" },
  xiao: { name: "XIAO RP2040", pads: ["D6", "D7", "GND"], text: "rsWireXiao" },
  pico: { name: "Raspberry Pi Pico", pads: ["GP0 · 1", "GP1 · 2", "GND · 3"], text: "rsWirePico" },
};
function rsWiring(b) {
  const B = RS_BOARDS[b], W = [["#3fbf5f", "D+"], ["#f2f0ea", "D−"], ["#111", "GND"]];
  const pad = (i) => 104 + i * 46;
  let g = `<svg viewBox="0 0 560 260" role="img" aria-label="${B.name}: ${B.pads.join(", ")}" font-family="IBM Plex Mono, monospace">`;
  g += `<rect x="20" y="40" width="200" height="180" rx="14" fill="#16131f" stroke="currentColor" stroke-opacity=".35"/>`;
  g += `<rect x="6" y="110" width="28" height="40" rx="6" fill="#2a2633" stroke="currentColor" stroke-opacity=".35"/><text x="20" y="100" font-size="11" text-anchor="middle" fill="currentColor" opacity=".6">USB</text>`;
  g += `<text x="120" y="68" font-size="15" text-anchor="middle" fill="currentColor">${B.name}</text>`;
  g += `<rect x="60" y="86" width="40" height="40" rx="5" fill="#2a2633"/><text x="80" y="111" font-size="10" text-anchor="middle" fill="currentColor" opacity=".6">RP2040</text>`;
  B.pads.forEach((n, i) => {
    g += `<circle cx="200" cy="${pad(i)}" r="9" fill="#c9a74a" stroke="#000" stroke-opacity=".4"/>`;
    g += `<text x="186" y="${pad(i) + 4}" font-size="12" text-anchor="end" fill="currentColor">${n}</text>`;
    const d = `M200 ${pad(i)} C 300 ${pad(i)}, 330 ${100 + i * 30}, 420 ${100 + i * 30}`;
    g += `<path d="${d}" fill="none" stroke="currentColor" stroke-opacity=".45" stroke-width="9" stroke-linecap="round"/>`;   // (an edge: black shows on dark)
    g += `<path d="${d}" fill="none" stroke="${W[i][0]}" stroke-width="6" stroke-linecap="round"/>`;
    g += `<text x="300" y="${(pad(i) + 100 + i * 30) / 2 - 8}" font-size="11" fill="currentColor" opacity=".8">${W[i][1]}</text>`;
  });
  g += `<path d="M420 190 C 395 190, 380 205, 350 215" fill="none" stroke="#d13a3a" stroke-width="6" stroke-linecap="round" stroke-dasharray="2 9"/>`;
  g += `<text x="356" y="238" font-size="11" fill="currentColor" opacity=".7">5 V ✕</text>`;
  g += `<rect x="420" y="80" width="60" height="124" rx="10" fill="#2a2633" stroke="currentColor" stroke-opacity=".35"/>`;
  g += `<rect x="480" y="122" width="40" height="40" rx="10" fill="#3a3544" stroke="currentColor" stroke-opacity=".5"/>`;
  g += `<text x="450" y="70" font-size="12" text-anchor="middle" fill="currentColor">USB-C</text><text x="500" y="186" font-size="12" text-anchor="middle" fill="currentColor">FM-1</text>`;
  return g + "</svg>";
}
const rs = { board: "zero", fw: null, fwName: "", loader: null, lib: null, src: "fm", busy: false };
const rsLib = () => rs.lib || (rs.lib = import("./rescue/builder.js"));
function rsSay(id, text, cls = "") { const e = $(id); e.textContent = text; e.className = "rsst" + (cls ? " " + cls : ""); }
async function rsLoader() {
  if (rs.loader) return rs.loader;
  const L = await rsLib();
  rsSay("rsldst", t("rsLdGet"));
  try {
    const r = await fetch(L.LOADER_URL);
    if (!r.ok) throw new Error("HTTP " + r.status);
    rs.loader = await L.checkLoader(new Uint8Array(await r.arrayBuffer()));
    rsSay("rsldst", t("rsLdOk"), "ok");
  } catch (e) { rsSay("rsldst", t("rsLdBad").replace("%s", e.message), "err"); }
  return rs.loader;
}
async function rsUse(bytes, name) {
  const L = await rsLib();
  try { const v = await L.checkPackage(bytes); rs.fw = bytes; rs.fwName = `${name} · ${v.official ? "V15" : v.identity}`; rsSay("rsfwst", t("rsFwOk").replace("%s", rs.fwName), "ok"); }
  catch (e) { rs.fw = null; rsSay("rsfwst", t("rsFwBad").replace("%s", e.message), "err"); }
  rsRender();
}
/* the published package of a firmware: its site's firmware/versions.json, the first (the installer's) */
async function rsPublished(fw) {
  const name = fw === "byte" ? "JIANT BYTE" : "JIANT FM", base = `${TOOLS[fw].base}firmware/`;
  rs.fw = null; rsSay("rsfwst", t("rsFwGet").replace("%s", name)); rsRender();
  try {
    const v = await (await fetch(base + "versions.json", { cache: "no-cache" })).json();
    const e = v.find((x) => !x.status || x.status === "public") || v[0];
    const r = await fetch(base + e.file);
    if (!r.ok) throw new Error("HTTP " + r.status);
    await rsUse(new Uint8Array(await r.arrayBuffer()), `${name} ${e.version}`);
  } catch (e) { rsSay("rsfwst", t("rsFwBad").replace("%s", e.message), "err"); }
}
async function rsMake() {
  if (!rs.fw) { rsSay("rsmakest", t("rsNeedFw"), "err"); return; }
  rs.busy = true; rsRender();
  try {
    const L = await rsLib(), loader = await rsLoader();
    if (!loader) throw new Error(t("rsLdBad").replace("%s", "—"));
    const base = new Uint8Array(await (await fetch(`rescue/base/fm1-rescue-base-${rs.board}.uf2`)).arrayBuffer());
    const { uf2 } = await L.buildRescueUf2({ baseUf2: base, fwsc: rs.fw, loader });
    download(new Blob([uf2], { type: "application/octet-stream" }), "fm1-rescue.uf2");
    rs.made = uf2.length;
    rsSay("rsmakest", t("rsMade").replace("%s", `${RS_BOARDS[rs.board].name}, ${rs.fwName}`).replace("%n", Math.round(uf2.length / 1024)), "ok");
  } catch (e) { rsSay("rsmakest", t("rsFwBad").replace("%s", e.message), "err"); }
  rs.busy = false; rsRender();
}
let rsStarted = false;
function rsRender() {
  if (!$("v-rescue")) return;
  document.querySelectorAll("#rsboard [data-board]").forEach((b) => b.setAttribute("aria-pressed", String(b.dataset.board === rs.board)));
  document.querySelectorAll("#rsfw [data-fwsrc]").forEach((b) => b.setAttribute("aria-pressed", String(b.dataset.fwsrc === rs.src)));
  $("rswire").innerHTML = rsWiring(rs.board);
  $("rswiretext").textContent = t(RS_BOARDS[rs.board].text);
  const S = RS_STEPS[lang] || RS_STEPS.en;
  $("rsflash").innerHTML = S.flash.map((x) => `<li>${x}</li>`).join("");
  $("rsgo").innerHTML = S.go.map((x) => `<li>${x}</li>`).join("");
  $("rsleds").replaceChildren(...RS_LEDS.list.map(([c, c2, k, es, en]) => {
    const r = document.createElement("div"), i = document.createElement("i"), a = document.createElement("b"), d = document.createElement("span");
    r.className = "led"; i.className = k; i.style.setProperty("--c", c); if (c2) i.style.setProperty("--c2", c2);
    const [x, y] = lang === "es" ? es : en; a.textContent = x; d.textContent = y; r.append(i, a, d); return r;
  }));
  $("rsmake").disabled = rs.busy || !rs.fw;
  const done = { board: true, fw: !!rs.fw, make: !!rs.made };
  document.querySelectorAll("#rsteps .rstep").forEach((li) => li.classList.toggle("done", !!done[li.dataset.k]));
  if (!rsStarted && state.view === "rescue") { rsStarted = true; rsPublished(rs.src === "byte" ? "byte" : "fm"); rsLoader(); }
}
$("rsboard").addEventListener("click", (e) => { const b = e.target.closest("[data-board]"); if (b) { rs.board = b.dataset.board; rsRender(); } });
$("rsfw").addEventListener("click", (e) => {
  const b = e.target.closest("[data-fwsrc]"); if (!b) return;
  rs.src = b.dataset.fwsrc;
  if (rs.src === "file") $("rsfile").click(); else rsPublished(rs.src);
  rsRender();
});
$("rsfile").addEventListener("change", async () => { const f = $("rsfile").files[0]; $("rsfile").value = ""; if (f) rsUse(new Uint8Array(await f.arrayBuffer()), f.name); });
$("rsmake").addEventListener("click", rsMake);

/* ------------------------------------------------------------------------------------------------ manuals */
/* each firmware's manuals on its own site (same origin): the interactive manual (here, in a frame, or a tab), the full
 * manual in HTML (tools/gen_manual_html.py), the two cheat sheets (picture and PDF) and, for JIANT FM, what's new, the
 * map and the other devices' manuals. What a site does not have yet (a HEAD that fails) says "on its way" */
const DOCS = {
  fm: { inter: "manual/", full: { es: "docs/guide/manual.es.html", en: "docs/guide/manual.html" },
        ctl: "docs/cheatsheet-controls", snd: "docs/cheatsheet-sound",
        extra: [["docNew", "docNewText", { es: "docs/guide/whats-new-v10.8.21.es.html", en: "docs/guide/whats-new-v10.8.21.html" }, null],
                ["docMap", "docMapText", { es: "docs/navigation-map.es.png", en: "docs/navigation-map.png" }, "docs/navigation-map.png"],
                ["docPorts", "docPortsText", { es: "docs/guide/smk37.html", en: "docs/guide/smk37.html" }, "studio/img/smk37.webp"]] },
  byte: { inter: "manual/", full: { es: "docs/guide/manual.es.html" }, ctl: "docs/cheatsheet-byte-controls", snd: "docs/cheatsheet-byte-sound", extra: [] },
};
const docHas = new Map();   // url -> Promise<bool>
const has = (url) => {
  if (MOCK) return Promise.resolve(true);
  // (a GET let go once its answer starts: any server answers it, a HEAD not always)
  if (!docHas.has(url)) { const c = new AbortController();
    docHas.set(url, fetch(url, { cache: "no-cache", signal: c.signal }).then((r) => { c.abort(); return r.ok; }, () => false)); }
  return docHas.get(url);
};
function renderDocs() {
  const fw = state.fw, D = DOCS[fw], base = TOOLS[fw].base, name = fw === "byte" ? "JIANT BYTE" : "JIANT FM";
  $("docstitle").textContent = name;
  $("docslead").textContent = t(fw === "byte" ? "docsLeadByte" : "docsLeadFm");
  const el = (tag, cls, text) => { const e = document.createElement(tag); if (cls) e.className = cls; if (text != null) e.textContent = text; return e; };
  const link = (text, href, go) => { const a = el("a", "btn" + (go ? "" : " ghost"), text); a.href = href; a.target = "_blank"; a.rel = "noopener"; return a; };
  // a card: its picture (an image that opens large, or a word), title, text, its links once the site has them
  const card = (o) => {
    const c = el("article", "card doc" + (o.wide ? " wide" : ""));
    const pic = el("div", "pic" + (o.img ? "" : " glyph"));
    if (o.img) {
      const b = el("button"); b.type = "button"; const im = new Image(); im.loading = "lazy"; im.alt = o.title; b.append(im);
      b.addEventListener("click", () => openShot({ name: "", shots: [[o.big || o.img, ""]] , title: o.title }, 0));
      pic.append(b);
      has(o.img).then((ok) => { if (ok) im.src = o.img; else { pic.className = "pic glyph"; pic.replaceChildren(name); } });
    } else pic.textContent = o.glyph || name;
    const links = el("div", "links");
    c.append(pic, el("h2", null, o.title), el("p", null, o.text), links);
    has(o.check).then((ok) => links.replaceChildren(...(ok ? o.links() : [el("span", "soon", t("docSoon"))])));
    return c;
  };
  const full = D.full[lang] || D.full.es || D.full.en;
  const cards = [
    card({ wide: true, title: t("docInter"), text: t(fw === "byte" ? "docInterTextByte" : "docInterText"), glyph: `${name} · ${t("docInter")}`,
      img: fw === "fm" ? "img/manual-fm.webp" : null, check: base + D.inter, links: () => {
        const here = el("button", "btn", t("docOpenHere")); here.type = "button";
        here.addEventListener("click", () => openManual(base + D.inter, `${name} · ${t("docInter")}`));
        return [here, link(t("docNewTab"), base + D.inter)];
      } }),
    card({ title: t("docFull"), text: t("docFullText"), glyph: name, check: base + full,
      links: () => Object.entries(D.full).map(([l, h], i) => link(Object.keys(D.full).length > 1 ? (l === "es" ? "Español" : "English") : t("docNewTab"), base + h, l === lang || (i === 0 && !D.full[lang]))) }),
    ...[["docCtl", "docCtlText", D.ctl], ["docSnd", "docSndText", D.snd]].map(([k, tx, f]) =>
      card({ title: t(k), text: t(tx), img: base + f + ".png", check: base + f + ".png",
        links: () => { const v = el("button", "btn", t("docView")); v.type = "button";
          v.addEventListener("click", () => openShot({ name: "", shots: [[base + f + ".png", ""]], title: t(k) }, 0));
          return [v, link(t("docPdf"), base + f + ".pdf"), link(t("docPng"), base + f + ".png")]; } })),
    ...D.extra.map(([k, tx, hrefs, img]) => card({ title: t(k), text: t(tx), img: img && base + img, glyph: t(k), check: base + (hrefs[lang] || hrefs.en),
      links: () => [link(t("docNewTab"), base + (hrefs[lang] || hrefs.en), true)] })),
  ];
  $("doclist").replaceChildren(...cards);
}
/* the interactive manual inside Studio, in its frame under the cards */
function openManual(url, title) {
  const f = $("f-manual");
  $("mantry").hidden = false; $("mantrytitle").textContent = title; $("mannew").href = url;
  if (f.src !== new URL(url, location.href).href) f.src = url;
  $("mantry").scrollIntoView({ behavior: "smooth", block: "start" });
}
$("manclose").addEventListener("click", () => { $("mantry").hidden = true; $("f-manual").src = "about:blank"; });

/* ------------------------------------------------------------------------------------------- the whole page */
function render() {
  const d = state.dev, fwName = state.fw === "byte" ? "BYTE" : "FM";
  $("brandfw").textContent = fwName; $("herofw").textContent = fwName;
  $("herolead").textContent = t(state.fw === "byte" ? "leadByte" : "leadFm");
  $("sitelink").href = TOOLS[state.fw].base; $("sitelink").textContent = t("siteLink").replace("%s", state.fw === "byte" ? "JIANT BYTE" : "JIANT FM");
  document.querySelectorAll("[data-fw]").forEach((b) => b.setAttribute("aria-pressed", String(b.dataset.fw === state.fw)));
  document.querySelectorAll("[data-install]").forEach((b) => b.setAttribute("aria-pressed", String(b.dataset.install === state.fw)));
  document.querySelectorAll("[data-target]").forEach((b) => b.setAttribute("aria-pressed", String(b.dataset.target === state.target)));
  document.querySelectorAll("nav.views button").forEach((b) => b.setAttribute("aria-current", b.dataset.go === state.view ? "page" : "false"));
  // the device chip
  const dot = $("devdot");
  dot.className = "dot" + (state.busy ? " warn" : d ? " ok" : "");
  $("devtext").textContent = state.busy ? t("devBusy") : state.looking ? t("devLooking") :
    d ? devName(d) : t("devNone");
  $("find").disabled = state.looking || state.busy; $("forget").hidden = !d;
  $("findres").textContent = state.found;
  $("step1").classList.toggle("now", !d); $("step3").classList.toggle("now", !!d);
  // install
  $("insttitle").textContent = t(state.fw === "byte" ? "instTitleByte" : "instTitleFm");
  // editor
  $("edtitle").textContent = state.fw === "byte" ? "JIANT BYTE" : "JIANT FM";
  $("edtarget").textContent = t("edTarget") + t(state.target === "emu" ? "tEmu" : "tFm1");
  $("ednote").hidden = state.target !== "emu"; $("ednote").textContent = t("edEmuNote");
  // emulator
  $("emutitle").textContent = state.fw === "byte" ? "JIANT BYTE" : "JIANT FM";
  // screen
  $("scrgo").textContent = t(scr.on ? "scrStop" : "scrStart");
  const noMirror = state.target === "fm1" && d && d.screen === false;
  $("scrgo").disabled = state.busy || (!scr.on && state.target === "fm1" && (!d || noMirror));
  $("scrfull").disabled = noMirror;
  $("scrwait").hidden = scr.on && scr.got;
  $("scrwait").textContent = scr.on ? t("scrWaiting") : noMirror ? t("scrNoFw").replace("%s", devName(d)) :
    t(state.target === "emu" ? "scrWaitEmu" : "scrWaitFm1");
  $("scrhelp").textContent = t(state.target === "emu" ? "scrHelpEmu" : "scrHelpFm1");
  $("scr").setAttribute("aria-label", t("scrTitle"));
  if (!scr.on && state.view !== "screen") $("scrstat").textContent = "";
  $("scrfull").hidden = !(document.fullscreenEnabled || document.webkitFullscreenEnabled);
  // record
  $("recgo").textContent = t(rec.on ? "recStop" : "recStart");
  $("recgo").classList.toggle("recording", rec.on);
  $("recgo").disabled = state.busy;
  $("recmon").textContent = t(rec.mon ? "recMonitorStop" : "recMonitor");
  $("recmon").disabled = rec.on || state.busy;
  $("recplay").textContent = t(daw.playing ? "recPlayStop" : "recPlay");
  $("recplay").disabled = rec.on || !daw.planes;
  $("recprof").disabled = rec.mon || /^emu/.test($("recdev").value);
  if (state.view === "rec") profileHelp();
  if (state.view === "rec" && !rec.on) { renderAudioInputs(); renderMidiInputs(); }
  renderTakes();
  if (state.view === "ports") renderPorts();
  if (state.view === "proj") renderProj();
  if (state.view === "learn") renderLearn();
  if (state.view === "rescue") { if (!rsStarted) rs.src = state.fw; rsRender(); }
  if (state.view === "docs" && docsFor !== state.fw + lang) { docsFor = state.fw + lang; renderDocs(); }
}

/* ------------------------------------------------- the editor and the emulator joined (target: Emulator) */
/* A MIDIAccess-like pair of ports for the editor's frame (editor.html ?emu=1): what the editor sends goes to the emulator's
 * own editor handler (emu/index.html jiantEmu.sysex -> worklet.js -> editor-api.h), its replies and pushes come back */
function emuMidi() {
  const emu = () => { try { return frames.emu.el.contentWindow.jiantEmu; } catch (_) { return null; } };
  loadFrame("emu");
  const input = { id: "emu-in", name: "JIANT (emulator)", type: "input", state: "connected", manufacturer: "JIANT", onmidimessage: null,
    _l: new Set(), addEventListener(_, f) { this._l.add(f); }, removeEventListener(_, f) { this._l.delete(f); },
    open() { return Promise.resolve(this); }, close() { return Promise.resolve(this); } };
  let off = null;
  const hook = () => {
    const e = emu();
    if (!e || off) return;
    off = e.listen((bytes) => { const ev = { data: Uint8Array.from(bytes) }; input.onmidimessage && input.onmidimessage(ev); input._l.forEach((f) => f(ev)); });
  };
  const output = { id: "emu-out", name: "JIANT (emulator)", type: "output", state: "connected", manufacturer: "JIANT",
    open() { return Promise.resolve(this); }, close() { return Promise.resolve(this); },
    send(d) { hook(); const e = emu(); if (e && e.on()) e.sysex(Array.from(d)); } };
  hook();
  return { inputs: new Map([[input.id, input]]), outputs: new Map([[output.id, output]]), onstatechange: null, sysexEnabled: true };
}
/* which MIDI inputs play the emulator (emu/index.html jiantEmu.midiInput): every one, none, or one port */
function renderEmuMidi() {
  const sel = $("emumidi");
  let e = null;
  try { e = frames.emu.el.contentWindow.jiantEmu; } catch (_) {}
  if (!e || !e.on()) { sel.replaceChildren(new Option(t("emuMidiOff"), "")); sel.disabled = true; return; }
  sel.disabled = false;
  sel.replaceChildren(new Option(t("emuMidiAll"), "all"), new Option(t("emuMidiNone"), "none"), ...e.midiInputs().map((p) => new Option(p.name, p.id)));
  sel.value = e.midiInput();
}
$("emumidi").addEventListener("change", () => { try { frames.emu.el.contentWindow.jiantEmu.midiInput($("emumidi").value); } catch (_) {} });
$("emumidi").addEventListener("pointerdown", renderEmuMidi);
setInterval(() => { if (state.view === "emu" && document.activeElement !== $("emumidi")) renderEmuMidi(); }, 1500);
/* the emulator's whole panel in its frame: the device as wide as the frame lets it be with all of it in sight (its own
 * page makes it as wide as the page, taller than the frame); refitted on any resize, in or out of full screen */
function fitEmu() {
  try {
    const w = frames.emu.el.contentWindow, d = w.document, dev = d.getElementById("device");
    if (!dev) return;
    const ar = (dev.style.aspectRatio || "1000 / 624").split("/").map(Number), r = ar[0] / ar[1];
    const main = dev.parentElement, cs = w.getComputedStyle(main);
    const padX = parseFloat(cs.paddingLeft) + parseFloat(cs.paddingRight);
    const top = dev.getBoundingClientRect().top + w.scrollY;
    const h = Math.max(240, w.innerHeight - top - 12), wid = Math.min(main.clientWidth - padX, h * r);
    main.style.maxWidth = "none";
    Object.assign(dev.style, { width: `${Math.floor(wid)}px`, marginLeft: "auto", marginRight: "auto" });
  } catch (_) {}
}
frames.emu.el.addEventListener("load", () => {
  fitEmu();
  try {
    const w = frames.emu.el.contentWindow;
    w.addEventListener("resize", fitEmu);
    // (its layout, wide or tall, rebuilt: the aspect changes)
    new w.MutationObserver(fitEmu).observe(w.document.getElementById("device"), { attributes: true, attributeFilter: ["style"] });
  } catch (_) {}
});
/* full screen: the browser's own (the frame's box) where it has it; else (iPhone) the box over the whole page */
const emuFullOn = () => $("emubox").classList.contains("max") || document.fullscreenElement === $("emubox") || document.webkitFullscreenElement === $("emubox");
function emuFull(on) {
  const b = $("emubox");
  if (on) {
    loadFrame("emu");
    const req = b.requestFullscreen || b.webkitRequestFullscreen;
    const p = req ? req.call(b) : null;
    if (p && p.catch) p.catch(() => b.classList.add("max")); else if (!req) b.classList.add("max");
  } else {
    b.classList.remove("max");
    if (document.fullscreenElement || document.webkitFullscreenElement) (document.exitFullscreen || document.webkitExitFullscreen).call(document);
  }
  setTimeout(fitEmu, 300);
}
$("emufull").addEventListener("click", () => emuFull(!emuFullOn()));
$("emuexit").addEventListener("click", () => emuFull(false));
document.addEventListener("keydown", (e) => { if (e.key === "Escape" && $("emubox").classList.contains("max")) emuFull(false); });
const emuOn = () => { try { const e = frames.emu.el.contentWindow.jiantEmu; return !!(e && e.on()); } catch (_) { return false; } };

/* ?mock=1: STEMS+IN's 10 channels made up (each its own pitch and pulse), in the recorder's own context: a node */
function mockStems(ac) {
  const merge = ac.createChannelMerger(10);
  for (let k = 0; k < 10; k++) {
    const o = ac.createOscillator(), g = ac.createGain(), lfo = ac.createOscillator(), lg = ac.createGain();
    o.type = ["square", "sawtooth", "triangle", "sine"][k % 4]; o.frequency.value = 55 * (k + 2);
    lfo.frequency.value = 0.7 + k * 0.45; lg.gain.value = 0.18; g.gain.value = 0.2;
    lfo.connect(lg).connect(g.gain); o.connect(g).connect(merge, 0, k); o.start(); lfo.start();
  }
  return merge;
}

/* ------------------------------------------------------------------------------- ?mock=1: a simulated FM-1 */
function mockAccess() {
  const fb = new Uint16Array(SCR.W * SCR.H), rgb = (r, g, b) => (r >> 3) << 11 | (g >> 2) << 5 | b >> 3;
  const rect = (x, y, w, h, c) => { for (let j = y; j < y + h; j++) fb.fill(c, j * SCR.W + x, j * SCR.W + x + w); };
  const kind = QS.get("fw") === "byte" ? "byte" : "fm";
  let on = false, last = 0, step = 0, tick = 0;
  const input = { id: "mock-in", name: "JIANT", type: "input", state: "connected", onmidimessage: null,
    _l: new Set(), addEventListener(_, f) { this._l.add(f); }, removeEventListener(_, f) { this._l.delete(f); } };
  const deliver = (bytes) => setTimeout(() => { const ev = { data: Uint8Array.from(bytes) }; input.onmidimessage && input.onmidimessage(ev); input._l.forEach((f) => f(ev)); }, 2);
  const draw = () => {
    const bg = kind === "byte" ? rgb(0, 0, 0) : rgb(20, 18, 28), acc = kind === "byte" ? rgb(167, 123, 255) : rgb(236, 110, 80);
    rect(0, 0, 240, 240, bg); rect(0, 0, 240, 24, rgb(40, 36, 52)); rect(8, 8, 70, 8, rgb(230, 230, 230)); rect(196, 8, 36, 8, acc);
    rect(8, 34, 52, 44, acc); rect(64, 34, 52, 44, rgb(60, 56, 80)); rect(120, 34, 52, 44, rgb(60, 56, 80)); rect(176, 34, 56, 44, rgb(60, 56, 80));
    for (let i = 0; i < 120; i++) fb[(120 + Math.round(30 * Math.sin(i / 9))) * 240 + 60 + i] = acc;
  };
  // its projects (A and C saved) and its archive (backups: the current music, the settings, the song index, two songs)
  const slots = [true, false, true, false];
  let mockWatch = 0;
  const arch = new Map(BACKUP_IDS.map((id) => {
    const n = { 0: 520, 1: 140, 10: 64, 40: 300, 41: 300, 44: 280 }[id] || 0;
    return [id, Uint8Array.from({ length: n }, (_, i) => (i * 7 + id * 13) & 255)];
  }));
  const puts = new Map();
  const output = { id: "mock-out", name: "JIANT", type: "output", state: "connected", send(d) {
    const f = unframe(Uint8Array.from(d));
    if (!f) return;
    last = Date.now();
    if (f.cmd === 22) {                                         // WATCH: then a knob turning (CUT), now and then a sound change
      clearInterval(mockWatch);
      if (f.a[0]) { let k = 0; mockWatch = setInterval(() => {
        k++;
        if (k % 12 === 0) deliver(frame(24, [(k / 12) % 2 ? 2 : 0, k % 5, 0]));
        else if (k % 12 === 6) { const s0 = (x) => [...x].map((c) => c.charCodeAt(0)).concat(0);
          deliver(frame(43, [1, ...s0(k % 24 === 6 ? "FILTER" : "OSC"), 0, 127, ...s0("CUT"), ...s0("RES"), ...s0("ENV"), ...s0("TYPE")])); }
        else { const v = 40 + Math.round(40 * Math.sin(k / 3)) + 8192; deliver(frame(23, [0, 20, v & 127, v >> 7])); }
      }, 250); }
      deliver(frame(22, [f.a[0] ? 31 : 0])); return;
    }
    if (f.cmd === 4) { deliver(frame(4, [0, 0])); return; }    // DUMP (the engine and preset are all the guide reads)
    if (f.cmd === 5) {                                          // DESC: CUT on any id
      const s = (x) => [...x].map((c) => c.charCodeAt(0)).concat(0), e = (v) => [(v + 8192) & 127, (v + 8192) >> 7];
      deliver(frame(5, [f.a[0], f.a[1], 0, ...e(0), ...e(127), ...e(64), ...s("CUT"), ...s("")])); return;
    }
    if (f.cmd === CMD.PING) { deliver(frame(CMD.PING, [])); return; }
    if (f.cmd === 9) {                                          // PROJECT: 0 load, 1 save, 2 query
      const [op, k] = f.a;
      if (op === 1) slots[k] = true;
      setTimeout(() => deliver(frame(9, [op, k, slots[k] ? 1 : 0])), op === 1 ? 300 : 20);
      return;
    }
    if (f.cmd === 65) {                                         // BACKUP_LIST
      deliver(frame(65, [1, 0, arch.size, ...[...arch].flatMap(([id, b]) => [id, ...bkU32(b.length), ...bkU32(b.length ? bkCrc(b) : 0)])]));
      return;
    }
    if (f.cmd === 66) {                                         // BACKUP_GET: id, offset, count
      const id = f.a[0], off = bkR32(f.a, 1), n = f.a[6] | f.a[7] << 7, b = arch.get(id);
      deliver(frame(66, [id, 0, ...bkU32(off), n & 127, n >> 7, ...bkPack(b.subarray(off, off + n))]));
      return;
    }
    if (f.cmd === 67) {                                         // BACKUP_PUT: begin, data, commit
      const [op, id] = f.a;
      if (op === 0) puts.set(id, new Uint8Array(bkR32(f.a, 2)));
      else if (op === 1) { const off = bkR32(f.a, 2), body = f.a.slice(7), n = Math.min(256, puts.get(id).length - off); puts.get(id).set(bkUnpack(body, n), off); }
      else if (op === 2) arch.set(id, puts.get(id));
      deliver(frame(67, [op, id, 0]));
      return;
    }
    if (f.cmd === CMD.INFO) {
      const s = (x) => [...x].map((c) => c.charCodeAt(0)).concat(0);
      const eng = kind === "byte" ? ["ANALOG", "-", "PHASE", "MATH", "WAVE", "DRUM"] : ["ANALOG", "-", "PHASE", "MATH", "DRUM", "FM6"];   // (FM has MATH too)
      deliver(frame(CMD.INFO, [...s(kind === "byte" ? (QS.has("nomirror") ? "JIANT BYTE v0.16" : "JIANT BYTE v0.17") : "JIANT V10.8.24"), eng.length, 100, 40, 64, 10, ...eng.flatMap(s)]));
    } else if (f.cmd === 27) {                                 // TRACK: six tracks, their engines (indexes into INFO's list)
      deliver(frame(27, [0, 6, ...[4, 0, 2, 3, 5, 1].flatMap((e) => [e, 0, 0, 64, 0, 0])]));
    } else if (f.cmd === CMD.SCREEN && !QS.has("nomirror")) {      // (?nomirror: firmware from before the mirror: no reply)
      const op = f.a[0] | 0;
      if (op === 1 || (op === 3 && on)) { on = true; draw(); for (const a of scrEncode(fb, 0, 0, 240, 240)) deliver(frame(CMD.SCREEN_DATA, a)); }
      if (op === 4) on = false;
      deliver(frame(CMD.SCREEN, [op, 0, on ? 1 : 0, 240 & 127, 240 >> 7, 240 & 127, 240 >> 7]));
    }
  } };
  setInterval(() => {                                        // the playhead's row moving, and a note now and then
    if (on && Date.now() - last < 4000) {
      const y = 200, row = fb.subarray(y * 240, (y + 12) * 240);
      row.fill(rgb(20, 18, 28)); for (let j = 0; j < 12; j++) row.fill(rgb(255, 210, 63), j * 240 + 8 + (step % 16) * 14, j * 240 + 20 + (step % 16) * 14);
      step++;
      for (const a of scrEncode(fb.subarray(y * 240), 0, y, 240, 12)) deliver(frame(CMD.SCREEN_DATA, a));
    } else on = false;
    if (++tick % 4 === 0) { const n = 48 + (tick % 12); deliver([0x90, n, 100]); setTimeout(() => deliver([0x80, n, 0]), 200); }
  }, 125);
  return { inputs: new Map([[input.id, input]]), outputs: new Map([[output.id, output]]), onstatechange: null, sysexEnabled: true };
}

/* ------------------------------------------------------------------------------------------------- the start */
document.documentElement.dataset.skin = state.fw;
const fromHash = (location.hash || "").replace(/^#/, "").split("?")[0].split("/")[0];
lrRoute(location.hash);
applyLang();
go(QS.get("view") || (VIEWS.includes(fromHash) ? fromHash : state.view), false);
window.addEventListener("hashchange", () => {
  const h = location.hash.replace(/^#/, "").split("/")[0];
  if (lrRoute(location.hash) && state.view === "learn") { renderLearn(); return; }
  if (VIEWS.includes(h) && h !== state.view) go(h, false);
});
if (MOCK || QS.has("screen")) find().then(() => { if (QS.has("screen") && state.dev) { screenStart(); } });
/* (for the tests) */
loadList();
try { if (!localStorage.getItem(TOUR_KEY) && !MOCK && !QS.has("notour") && !location.hash) setTimeout(() => tourShow(0), 700); } catch (_) {}
window.jiantStudio = { tourShow, tourEnd, proj, bkDb, emuMidi, state, find, letGo, screenStart, screenStop, recStart, recStop, setFw, setTarget, go, rec, scr };
