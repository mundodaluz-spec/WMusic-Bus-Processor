cmake_minimum_required(VERSION 3.22)

set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")

project(WMusicBusProcessor VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release)
endif()
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# JUCE: use -DJUCE_PATH=/caminho/para/JUCE ou deixe baixar sozinho (8.0.4).
if(JUCE_PATH)
    add_subdirectory(${JUCE_PATH} juce EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(juce GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.4 GIT_SHALLOW ON)
    FetchContent_MakeAvailable(juce)
endif()

set(WM_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND WM_FORMATS AU)
endif()

juce_add_plugin(WMusicBusProcessor
    PRODUCT_NAME "WMusic Bus Processor"
    COMPANY_NAME "WYATT Sounds"
    BUNDLE_ID com.wyattsounds.wmusicbusprocessor
    PLUGIN_MANUFACTURER_CODE Wytt
    PLUGIN_CODE WmBp
    FORMATS ${WM_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Dynamics
    AU_MAIN_TYPE kAudioUnitType_Effect)

juce_add_binary_data(WMusicAssets SOURCES Assets/logo.png)

target_sources(WMusicBusProcessor PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(WMusicBusProcessor PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0)

target_link_libraries(WMusicBusProcessor
    PRIVATE WMusicAssets juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)

# Testes do motor (sem JUCE): cmake --build build --target engine_tests
add_executable(engine_tests EXCLUDE_FROM_ALL Tests/engine_tests.cpp)

# Captura da tela para conferencia: cmake --build build --target ui_snapshot
add_executable(ui_snapshot EXCLUDE_FROM_ALL Tests/ui_snapshot.cpp)
target_link_libraries(ui_snapshot PRIVATE WMusicBusProcessor WMusicAssets juce::juce_audio_utils)
target_include_directories(ui_snapshot PRIVATE $<TARGET_PROPERTY:WMusicBusProcessor,INCLUDE_DIRECTORIES>)
target_compile_definitions(ui_snapshot PRIVATE $<TARGET_PROPERTY:WMusicBusProcessor,COMPILE_DEFINITIONS>)
