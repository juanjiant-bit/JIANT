# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
# JIANT FM as a plugin and an app (JUCE): VST3 everywhere, AU on macOS, AUv3 on macOS and iOS (AUM, GarageBand,
# Logic, Cubasis...), LV2 on Linux, and the standalone app. ports/juce-plugin/build.sh drives it.
#   JUCE_DIR      a JUCE checkout (default: JUCE 9.0.5 fetched)
#   JIANT_FORMATS the formats (default: per platform, below)
# The generated headers (build/gen) must exist: python3 -c "import sys; sys.path.insert(0, 'tools'); import build; build.generate()"
cmake_minimum_required(VERSION 3.22)
project(JIANT_FM VERSION 1.0.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
get_filename_component(REPO "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
if(NOT EXISTS "${REPO}/build/gen/felucca_tables.h")
    message(FATAL_ERROR "build/gen is missing: generate the headers first (see the top of this file)")
endif()

if(JUCE_DIR)
    add_subdirectory("${JUCE_DIR}" juce)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 9.0.5 GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

if(NOT JIANT_FORMATS)
    if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
        set(JIANT_FORMATS AUv3 Standalone)
    elseif(APPLE)
        set(JIANT_FORMATS VST3 AU AUv3 Standalone)
    elseif(WIN32)
        set(JIANT_FORMATS VST3 Standalone)
    else()
        set(JIANT_FORMATS VST3 LV2 Standalone)
    endif()
endif()

file(STRINGS "${REPO}/firmware/src/felucca.c" VLINE REGEX "^#define FELUCCA_VERSION \"")
string(REGEX REPLACE ".*\"([^\"]*)\".*" "\\1" JIANT_FIRMWARE_VERSION "${VLINE}")

juce_add_plugin(JiantFM
    PRODUCT_NAME "JIANT FM"
    COMPANY_NAME "Juan Jiant"
    COMPANY_WEBSITE "https://github.com/juanjiant-bit/JIANT"
    BUNDLE_ID com.juanjiant.jiantfm
    PLUGIN_MANUFACTURER_CODE Jnnt
    PLUGIN_CODE JnFM
    DESCRIPTION "JIANT FM: the FM-1 firmware as an instrument"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    LV2URI "https://github.com/juanjiant-bit/JIANT#jiant-fm"
    MICROPHONE_PERMISSION_ENABLED FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    FORMATS ${JIANT_FORMATS})

# the core, JIANT_COPIES times (ports/core/jiant_copies.h): each plugin instance its own JIANT, up to that many in a
# process; every copy one translation unit with its symbols renamed (ports/core/jiant_copy.c), a one-line file each
set(JIANT_COPIES 8 CACHE STRING "JIANTs a process can play at once (1..16)")
set(JIANT_CORE_SOURCES "${REPO}/ports/core/jiant_copies.c")
math(EXPR JIANT_LAST "${JIANT_COPIES} - 1")
foreach(k RANGE ${JIANT_LAST})
    set(f "${CMAKE_CURRENT_BINARY_DIR}/jiant_copy_${k}.c")
    set(body "/* JIANT core, copy ${k} (ports/core/jiant_copies.h) */\n#define JIANT_COPY ${k}\n#include \"jiant_copy.c\"\n")
    if(EXISTS "${f}")
        file(READ "${f}" was)
    else()
        set(was "")
    endif()
    if(NOT was STREQUAL body)
        file(WRITE "${f}" "${body}")
    endif()
    list(APPEND JIANT_CORE_SOURCES "${f}")
endforeach()
target_sources(JiantFM PRIVATE Source/JiantProcessor.cpp Source/JiantEditor.cpp Source/SamplePanel.cpp Source/JiantLook.cpp
    ${JIANT_CORE_SOURCES})
# the editor's type: DotGothic16 (ASCII, from web/emu/fonts) and IBM Plex Mono, both OFL (fonts/)
juce_add_binary_data(JiantFMFonts SOURCES fonts/DotGothic16-ascii.ttf fonts/IBMPlexMono-SemiBold.ttf)
set_target_properties(JiantFMFonts PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(JiantFM PRIVATE "${REPO}/ports/core" "${REPO}/build/gen" "${REPO}/firmware/src")
target_compile_definitions(JiantFM PRIVATE JIANT_COPIES=${JIANT_COPIES})
# the firmware's C, as the emulator and the host tests build it: no fused multiply-adds, its own warnings off; out of
# the link-time optimisation (each copy is one translation unit already: LTO of eight would only slow the link)
set(CORE_FLAGS -ffp-contract=off -w -fvisibility=hidden -fno-lto)
if(MSVC)                                                 # Windows: clang-cl (-T ClangCL), MSVC does not take GNU C
    set(CORE_FLAGS /w /clang:-ffp-contract=off /clang:-std=gnu99)
endif()
set_source_files_properties(${JIANT_CORE_SOURCES} PROPERTIES
    COMPILE_OPTIONS "${CORE_FLAGS}"
    COMPILE_DEFINITIONS "FELUCCA_VERSION=\"${JIANT_FIRMWARE_VERSION}\"")
if(WIN32)
    target_include_directories(JiantFM PRIVATE "${REPO}/ports/core/compat")
endif()
target_compile_definitions(JiantFM PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0 JUCE_USE_FLAC=1 JUCE_USE_MP3AUDIOFORMAT=1
    JUCE_REPORT_APP_USAGE=0)
target_link_libraries(JiantFM
    PRIVATE juce::juce_audio_utils JiantFMFonts
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)

# the check: the built plugin loaded as a host loads it (tests/plugin_check.cpp); -DJIANT_CHECK=ON
option(JIANT_CHECK "build tests/plugin_check" OFF)
if(JIANT_CHECK)
    juce_add_console_app(plugin_check PRODUCT_NAME "plugin_check")
    target_sources(plugin_check PRIVATE tests/plugin_check.cpp)
    target_compile_definitions(plugin_check PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_PLUGINHOST_VST3=1 JUCE_PLUGINHOST_AU=1 JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0 JUCE_PLUGINHOST_LV2=0)
    target_link_libraries(plugin_check PRIVATE juce::juce_audio_processors juce::juce_gui_basics
        PUBLIC juce::juce_recommended_config_flags)
    # the editor's picture from the plugin's own code (tests/editor_shot.cpp); not on Windows (its linker meets JUCE
    # twice, in the plugin's shared code and in the tool)
endif()
if(JIANT_CHECK AND NOT WIN32)
    juce_add_console_app(editor_shot PRODUCT_NAME "editor_shot")
    target_sources(editor_shot PRIVATE tests/editor_shot.cpp)
    target_compile_definitions(editor_shot PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(editor_shot PRIVATE JiantFM juce::juce_audio_utils PUBLIC juce::juce_recommended_config_flags)
    # the web editor's SysEx through the standalone app's processor (tests/sysex_check.cpp)
    juce_add_console_app(sysex_check PRODUCT_NAME "sysex_check")
    target_sources(sysex_check PRIVATE tests/sysex_check.cpp)
    target_compile_definitions(sysex_check PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(sysex_check PRIVATE JiantFM juce::juce_audio_utils PUBLIC juce::juce_recommended_config_flags)
    # samples: a file dropped on the editor, SAMPLE playing it, REC, the slots through the state (tests/sample_check.cpp)
    juce_add_console_app(sample_check PRODUCT_NAME "sample_check")
    target_sources(sample_check PRIVATE tests/sample_check.cpp)
    target_compile_definitions(sample_check PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(sample_check PRIVATE JiantFM juce::juce_audio_utils PUBLIC juce::juce_recommended_config_flags)
endif()
