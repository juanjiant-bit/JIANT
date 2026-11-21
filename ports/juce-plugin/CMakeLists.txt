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

target_sources(JiantFM PRIVATE Source/JiantProcessor.cpp Source/JiantEditor.cpp "${REPO}/ports/core/jiant_core.c")
target_include_directories(JiantFM PRIVATE "${REPO}/ports/core" "${REPO}/build/gen" "${REPO}/firmware/src")
# the firmware's C, as the emulator and the host tests build it: no fused multiply-adds, its own warnings off
set(CORE_FLAGS -ffp-contract=off -w)
if(MSVC)                                                 # Windows: clang-cl (-T ClangCL), MSVC does not take GNU C
    set(CORE_FLAGS /w /clang:-ffp-contract=off /clang:-std=gnu99)
endif()
set_source_files_properties("${REPO}/ports/core/jiant_core.c" PROPERTIES
    COMPILE_OPTIONS "${CORE_FLAGS}"
    COMPILE_DEFINITIONS "FELUCCA_VERSION=\"${JIANT_FIRMWARE_VERSION}\"")
if(WIN32)
    target_include_directories(JiantFM PRIVATE "${REPO}/ports/core/compat")
endif()
target_compile_definitions(JiantFM PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0)
target_link_libraries(JiantFM
    PRIVATE juce::juce_audio_utils
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
endif()
