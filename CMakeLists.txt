cmake_minimum_required(VERSION 3.22)

# Both of these must be set BEFORE project(): CMake resolves the macOS toolchain
# while running it, and a value set afterwards is silently ignored.
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal binary")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS")
endif()

project(MessyHatTricks VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(MSVC)
    add_compile_options(/MP)
endif()

include(FetchContent)

FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

FetchContent_Declare(clap-juce-extensions
    GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
    GIT_TAG main
    GIT_SUBMODULES_RECURSE TRUE)
FetchContent_MakeAvailable(clap-juce-extensions)

juce_add_plugin(MessyHatTricks
    COMPANY_NAME            "Calabi Yau"
    COMPANY_WEBSITE         "https://calabiyau.live"
    BUNDLE_ID               com.calabiyau.messyhattricks
    PLUGIN_MANUFACTURER_CODE CyAu
    PLUGIN_CODE             CbHt
    IS_SYNTH                TRUE
    NEEDS_MIDI_INPUT        TRUE
    NEEDS_MIDI_OUTPUT       TRUE
    IS_MIDI_EFFECT          FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD TRUE
    FORMATS                 AU VST3 Standalone
    VST3_CATEGORIES         "Instrument" "Drum"
    AU_MAIN_TYPE            kAudioUnitType_MusicDevice
    PRODUCT_NAME            "Messy Hat Tricks")

clap_juce_extensions_plugin(TARGET MessyHatTricks
    CLAP_ID "com.calabiyau.messyhattricks"
    CLAP_FEATURES instrument drum synthesizer
    CLAP_MANUAL_URL "https://calabiyau.live"
    CLAP_SUPPORT_URL "https://calabiyau.live")

juce_generate_juce_header(MessyHatTricks)

juce_add_binary_data(MessyHatTricksData
    HEADER_NAME MessyHatTricksData.h
    NAMESPACE MessyData
    SOURCES
        Resources/messy-logotype.png
        Resources/messy-character.png
        Resources/three-stars.png
        Resources/malvinas.png
        Resources/calabi_hats_logo.png
        Resources/calabi-yau-logotype.png)

set_target_properties(MessyHatTricksData PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

target_sources(MessyHatTricks PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(MessyHatTricks PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0)

target_link_libraries(MessyHatTricks PRIVATE
    MessyHatTricksData
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)
