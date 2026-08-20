cmake_minimum_required(VERSION 3.24)

# Must be set before the first project() call to take effect.
# CI overrides CMAKE_OSX_ARCHITECTURES explicitly for release/universal builds;
# local builds are left at the native architecture.
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS deployment target" FORCE)

# JUCE's build tooling (juceaide) and some CMake find-modules it triggers
# (e.g. FindThreads) expect the C toolchain to be available; declaring it
# here at the top level avoids it being enabled implicitly deeper inside
# JUCE's CMake helpers, which has been observed to break Ninja's generate
# step (missing CMAKE_C_COMPILE_OBJECT rule variable).
project(Seraph VERSION 0.4.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(cmake/CPM.cmake)

# CPMAddPackage automatically respects the CPM_SOURCE_CACHE environment
# variable if it is set, reusing a shared package cache across builds/checkouts.
if(DEFINED ENV{CPM_SOURCE_CACHE})
    message(STATUS "Seraph: using CPM_SOURCE_CACHE at $ENV{CPM_SOURCE_CACHE}")
endif()

CPMAddPackage(
    NAME JUCE
    GITHUB_REPOSITORY juce-framework/JUCE
    GIT_TAG 8.0.14
    GIT_SHALLOW TRUE
)

# ==============================================================================
# Signalsmith Stretch - the v0.3.0 "Shift" doubler mode's STFT pitch shifter
# (docs/architecture.md, brief ss3.0/ss3.2). MIT-licensed and therefore
# AGPLv3-compatible; the copyright notices live in THIRD-PARTY-NOTICES.md.
#
# Both packages are DOWNLOAD_ONLY: their own CMakeLists would pull
# signalsmith-linear via FetchContent (bypassing CPM_SOURCE_CACHE) and, on
# Apple targets, switch signalsmith-linear onto Accelerate/ACCELERATE_NEW_LAPACK
# by default - whose new-LAPACK symbols are not available on this project's
# 11.0 deployment target. Adding the two include directories by hand keeps the
# header-only portable C++ FFT path, which is what the CPU budget in the brief
# (ss3.2: <= 8% of one core for four Shift voices) was measured against.
#
# Pinned to the 1.3.2 development head rather than the 1.1.0 release tag
# deliberately: the formant API the brief mandates (setFormantSemitones/
# setFormantBase, ss3.2) does not exist in any tagged release yet. See
# THIRD-PARTY-NOTICES.md and the v0.3.0 CHANGELOG entry.
CPMAddPackage(
    NAME signalsmith-linear
    GITHUB_REPOSITORY Signalsmith-Audio/linear
    GIT_TAG 0.3.1
    GIT_SHALLOW TRUE
    DOWNLOAD_ONLY TRUE
)

CPMAddPackage(
    NAME signalsmith-stretch
    GITHUB_REPOSITORY Signalsmith-Audio/signalsmith-stretch
    GIT_TAG 57b93f4e9206a089a45387eaa39bdc9f310d3308
    DOWNLOAD_ONLY TRUE
)

add_library(SignalsmithStretch INTERFACE)
target_include_directories(SignalsmithStretch SYSTEM INTERFACE
    "${signalsmith-stretch_SOURCE_DIR}/include"
    "${signalsmith-linear_SOURCE_DIR}/include")

# ==============================================================================
# Plugin target
# ==============================================================================

juce_add_plugin(Seraph
    COMPANY_NAME "Yves Vogl"
    BUNDLE_ID com.yvesvogl.seraph
    PLUGIN_MANUFACTURER_CODE Yvsv
    PLUGIN_CODE Srph
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "Seraph"
    ICON_BIG "docs/assets/icon.png"
    COPY_PLUGIN_AFTER_BUILD TRUE
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
)

# ==============================================================================
# Embedded assets: M2 preset system factory presets (presets/factory/*.json)
# and the M2 i18n frame's German translation (resources/i18n/de.txt). See
# src/presets/PresetManager.h and src/presets/Localisation.h - both consume
# these purely via BinaryData:: symbols passed in from PluginProcessor.cpp/
# PluginEditor.cpp, never by including BinaryData.h themselves, so they stay
# portable across the suite (see sibling plugin nave's
# docs/preset-system-notes.md, the M2 pilot's replication recipe).
# ==============================================================================

juce_add_binary_data(SeraphBinaryData SOURCES
    presets/factory/default.json
    presets/factory/leadCutThrough.json
    presets/factory/leadIntimateCloseMic.json
    presets/factory/choirWideSpread.json
    presets/factory/choirTightBlend.json
    presets/factory/spokenGrowledInterlude.json
    presets/factory/glueOnly.json
    presets/factory/deEssOnlySurgical.json
    presets/factory/wideDoubleNoDynamics.json
    presets/factory/choirSacredShift.json
    presets/factory/doublerVintageMicro.json
    presets/factory/leadTightStackHumanized.json
    resources/i18n/de.txt
    # Suite typography (M3 vector editor, issue #4): EB Garamond, OFL
    # licensed (resources/fonts/OFL-EBGaramond.txt) - consumed by
    # src/gui/BasilicaLookAndFeel.cpp as the single source of every glyph
    # the fully JUCE-drawn editor renders.
    resources/fonts/EBGaramond-Regular.ttf
    resources/fonts/EBGaramond-SemiBold.ttf
)

# ==============================================================================
# SharedCode: an INTERFACE target carrying the plugin's sources, definitions,
# and library links so that both the plugin targets (AU/VST3/Standalone) and
# the Tests executable compile the exact same code (Pamplejuce pattern, see
# https://github.com/sudara/pamplejuce), avoiding ODR violations between them.
# ==============================================================================

add_library(SharedCode INTERFACE)

target_compile_features(SharedCode INTERFACE cxx_std_20)

file(GLOB_RECURSE SeraphSources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h")

target_sources(SharedCode INTERFACE ${SeraphSources})

target_include_directories(SharedCode INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/src")

target_compile_definitions(SharedCode INTERFACE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    # AGPLv3 usage permits disabling the splash screen.
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

# Add any extra juce:: modules a specific plugin needs (e.g. juce_audio_formats
# for reading impulse-response files) to this list.
target_link_libraries(SharedCode INTERFACE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_gui_basics
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
    SeraphBinaryData
    SignalsmithStretch
)

target_link_libraries(Seraph PRIVATE SharedCode)

# ==============================================================================
# Tests
# ==============================================================================

enable_testing()
set(CTEST_OUTPUT_ON_FAILURE ON)

CPMAddPackage(
    NAME Catch2
    GITHUB_REPOSITORY catchorg/Catch2
    GIT_TAG v3.15.2
)

file(GLOB_RECURSE SeraphTestSources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.h")

add_executable(Tests ${SeraphTestSources})

target_compile_features(Tests PRIVATE cxx_std_20)

target_link_libraries(Tests PRIVATE SharedCode Catch2::Catch2)

# Copy over the plugin target's compile definitions (JucePlugin_* macros etc.)
# so plugin code compiles identically when built as part of the Tests binary.
target_compile_definitions(Tests PRIVATE
    $<TARGET_PROPERTY:Seraph,COMPILE_DEFINITIONS>
    JUCE_MODAL_LOOPS_PERMITTED=1
)

set_target_properties(Tests PROPERTIES XCODE_GENERATE_SCHEME ON)

include(${Catch2_SOURCE_DIR}/extras/Catch.cmake)

if(CMAKE_GENERATOR STREQUAL "Xcode")
    set(SeraphDiscoveryMode PRE_TEST)
else()
    set(SeraphDiscoveryMode POST_BUILD)
endif()

catch_discover_tests(Tests DISCOVERY_MODE ${SeraphDiscoveryMode})
