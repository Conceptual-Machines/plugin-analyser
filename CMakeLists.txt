cmake_minimum_required(VERSION 3.22)
project(plugin-analyser VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE setup
# By default the pinned JUCE release below is downloaded (and checksum-verified) via FetchContent.
# To build against a local JUCE checkout instead: -DPLUGIN_ANALYSER_JUCE_SOURCE_DIR=/path/to/JUCE

set(PLUGIN_ANALYSER_JUCE_SOURCE_DIR "" CACHE PATH "Local JUCE checkout to use instead of the pinned download")
option(PLUGIN_ANALYSER_USE_FETCH_JUCE "Use FetchContent to get JUCE if no local checkout is given" ON)

if(PLUGIN_ANALYSER_JUCE_SOURCE_DIR)
    if(NOT EXISTS "${PLUGIN_ANALYSER_JUCE_SOURCE_DIR}/CMakeLists.txt")
        message(FATAL_ERROR "PLUGIN_ANALYSER_JUCE_SOURCE_DIR does not contain JUCE: ${PLUGIN_ANALYSER_JUCE_SOURCE_DIR}")
    endif()
    message(STATUS "Using local JUCE checkout at ${PLUGIN_ANALYSER_JUCE_SOURCE_DIR}")
    add_subdirectory(${PLUGIN_ANALYSER_JUCE_SOURCE_DIR} ${CMAKE_BINARY_DIR}/juce)
elseif(PLUGIN_ANALYSER_USE_FETCH_JUCE)
    message(STATUS "Downloading JUCE 8.0.10 via FetchContent...")
    include(FetchContent)
    # 8.0.10 = commit 3af3ce009f6a02f6fa651008fffb5b41743a9fab. A release tarball (with checksum) instead of a
    # git clone: reproducible, and avoids fetching JUCE's full history. 8.0.0 no longer builds against
    # the macOS 15 SDK (CGWindowListCreateImage is obsoleted).
    FetchContent_Declare(
        juce
        URL https://github.com/juce-framework/JUCE/archive/refs/tags/8.0.10.tar.gz
        URL_HASH SHA256=a84149d99dbb7ac04c0fc9a87ef0e61155416d6e13e09ae34bc7e829e1f650fd
    )
    FetchContent_MakeAvailable(juce)
else()
    find_package(JUCE CONFIG REQUIRED)
endif()

# Source files
set(SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow.h
    src/MainComponent.cpp
    src/MainComponent.h
    src/ParameterConfigComponent.cpp
    src/ParameterConfigComponent.h
    src/MeasurementConfigComponent.cpp
    src/MeasurementConfigComponent.h
    src/Config.cpp
    src/Config.h
    src/PluginLoader.cpp
    src/PluginLoader.h
    src/SignalGenerator.cpp
    src/SignalGenerator.h
    src/BucketSpec.cpp
    src/BucketSpec.h
    src/RunConfig.h
    src/BlockContext.h
    src/Analyzer.h
    src/RawCsvAnalyzer.cpp
    src/RawCsvAnalyzer.h
    src/RmsPeakAnalyzer.cpp
    src/RmsPeakAnalyzer.h
    src/TransferCurveAnalyzer.cpp
    src/TransferCurveAnalyzer.h
    src/LinearResponseAnalyzer.cpp
    src/LinearResponseAnalyzer.h
    src/ThdAnalyzer.cpp
    src/ThdAnalyzer.h
    src/MeasurementEngine.cpp
    src/MeasurementEngine.h
)

# Create GUI application
# WIN32: START_JUCE_APPLICATION defines WinMain on Windows, so the GUI app needs the Windows subsystem
add_executable(PluginAnalyser WIN32 MACOSX_BUNDLE ${SOURCES})

target_compile_definitions(PluginAnalyser
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_USE_DARK_SPLASH_SCREEN=0
        JUCE_PLUGINHOST_VST3=1
)

# Add DEBUG definition for debug builds (for dev-only defaults)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(PluginAnalyser PRIVATE DEBUG=1)
endif()

target_link_libraries(PluginAnalyser
    PRIVATE
        juce::juce_audio_processors
        juce::juce_audio_basics
        juce::juce_dsp
        juce::juce_core
        juce::juce_data_structures
        juce::juce_gui_basics
        juce::juce_gui_extra
)

# Set macOS bundle properties for proper app bundle with dock icon
set_target_properties(PluginAnalyser PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_INFO_STRING "Plugin Analyser - VST3 Plugin Analysis Tool"
    MACOSX_BUNDLE_ICON_FILE ""
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.pluginanalyser.app"
    MACOSX_BUNDLE_BUNDLE_NAME "Plugin Analyser"
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
    MACOSX_BUNDLE_LONG_VERSION_STRING ${PROJECT_VERSION}
    MACOSX_BUNDLE_COPYRIGHT "Copyright © 2024"
    OUTPUT_NAME "Plugin Analyser"
    XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.pluginanalyser.app"
    XCODE_ATTRIBUTE_LSMinimumSystemVersion "12.0"
)

# Also create console version for CLI usage
add_executable(plugin_measure_grid_cli src/main_cli.cpp
    src/Config.cpp src/Config.h
    src/PluginLoader.cpp src/PluginLoader.h
    src/SignalGenerator.cpp src/SignalGenerator.h
    src/BucketSpec.cpp src/BucketSpec.h
    src/RunConfig.h src/BlockContext.h src/Analyzer.h
    src/RawCsvAnalyzer.cpp src/RawCsvAnalyzer.h
    src/RmsPeakAnalyzer.cpp src/RmsPeakAnalyzer.h
    src/TransferCurveAnalyzer.cpp src/TransferCurveAnalyzer.h
    src/LinearResponseAnalyzer.cpp src/LinearResponseAnalyzer.h
    src/ThdAnalyzer.cpp src/ThdAnalyzer.h
    src/MeasurementEngine.cpp src/MeasurementEngine.h
)

target_compile_definitions(plugin_measure_grid_cli
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_PLUGINHOST_VST3=1
)

target_link_libraries(plugin_measure_grid_cli
    PRIVATE
        juce::juce_audio_processors
        juce::juce_audio_basics
        juce::juce_dsp
        juce::juce_core
        juce::juce_data_structures
)

# For JSON parsing, we'll use JUCE's JSON class or add nlohmann/json
# JUCE has built-in JSON support via juce::var and juce::JSON
