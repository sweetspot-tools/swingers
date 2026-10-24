cmake_minimum_required(VERSION 3.22)

project(Swingers VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(vendors/JUCE)
add_subdirectory(vendors/clap-juce-extensions EXCLUDE_FROM_ALL)
add_subdirectory(vendors/Catch2)

set(SWINGERS_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND SWINGERS_FORMATS AU)
endif()

# Pure engine: sources under core/ are added here by later tasks.
add_library(swingers_core INTERFACE)
target_sources(swingers_core INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/core/DrumMap.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/CubaseImport.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/PairOverrides.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/ConversionTable.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/NoteRouter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/LiveRemapper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/FileConverter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/MapLibrary.cpp)
target_link_libraries(swingers_core INTERFACE
    juce::juce_core
    juce::juce_audio_basics)
target_include_directories(swingers_core INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/core)

juce_add_plugin(Swingers
    COMPANY_NAME "Sweetspot Tools"
    PRODUCT_NAME "Swingers"
    BUNDLE_ID com.sweetspot-tools.swingers
    PLUGIN_MANUFACTURER_CODE Swtl
    PLUGIN_CODE Swng
    FORMATS ${SWINGERS_FORMATS}
    IS_MIDI_EFFECT TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_SYNTH FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)

# Bundled drum maps (BinaryData namespace).
juce_add_binary_data(SwingersMaps SOURCES
    resources/maps/general-midi.swmap.json)

target_sources(Swingers PRIVATE
    plugin/PluginState.cpp
    plugin/PluginProcessor.cpp)

target_include_directories(Swingers PRIVATE plugin)

target_compile_definitions(Swingers PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Swingers
    PRIVATE
        swingers_core
        SwingersMaps
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

clap_juce_extensions_plugin(TARGET Swingers
    CLAP_ID "com.sweetspot-tools.swingers"
    CLAP_FEATURES utility)

# Tests: the engine is plain C++, the smoke test also builds the processor.
juce_add_console_app(swingers_tests
    PRODUCT_NAME "swingers_tests")

target_sources(swingers_tests PRIVATE
    tests/test_smoke.cpp
    tests/test_drum_map.cpp
    tests/test_cubase_import.cpp
    tests/test_conversion_table.cpp
    tests/test_note_router.cpp
    tests/test_live_remapper.cpp
    tests/test_file_converter.cpp
    tests/test_map_library.cpp
    tests/test_plugin_state.cpp
    plugin/PluginState.cpp
    plugin/PluginProcessor.cpp)

target_include_directories(swingers_tests PRIVATE plugin)

target_compile_definitions(swingers_tests PRIVATE
    SWINGERS_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures"
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)

target_link_libraries(swingers_tests
    PRIVATE
        swingers_core
        SwingersMaps
        juce::juce_audio_processors
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
        Catch2::Catch2WithMain)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/vendors/Catch2/extras)
include(CTest)
include(Catch)
catch_discover_tests(swingers_tests)
