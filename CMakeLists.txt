cmake_minimum_required(VERSION 3.22)
project(CalibrationCurveGenerator VERSION 1.0.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Windows: static C/C++ runtime, so the .exe doesn't need the Visual C++ Redistributable
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

if(NOT DEFINED JUCE_PATH)
  if(WIN32)
    set(JUCE_PATH "C:/Users/pinte/Desktop/JUCE" CACHE PATH "Path to JUCE")
  else()
    set(JUCE_PATH "$ENV{HOME}/JUCE" CACHE PATH "Path to JUCE")
  endif()
endif()

if(NOT EXISTS "${JUCE_PATH}/CMakeLists.txt")
  message(FATAL_ERROR "JUCE not found at ${JUCE_PATH}. Pass -DJUCE_PATH=/path/to/JUCE")
endif()

add_subdirectory("${JUCE_PATH}" "${CMAKE_BINARY_DIR}/juce_build")

option(CALIB_NATIVE_ARCH "Optimise for this CPU only (-march=native)" OFF)
option(CALIB_BUILD_TESTS "Build the offline verification program" ON)
option(CALIB_PORTABLE_LINUX "Link libstdc++/libgcc statically so the binary runs on other Linux distros" OFF)
if(CALIB_PORTABLE_LINUX AND (CALIB_NATIVE_ARCH OR NOT UNIX OR APPLE))
  message(FATAL_ERROR "CALIB_PORTABLE_LINUX is Linux-only and incompatible with CALIB_NATIVE_ARCH")
endif()

set(CORE_SOURCES
  Source/CalibrationCore.h Source/CalibrationCore.cpp)

function(calib_common target)
  if(CALIB_NATIVE_ARCH AND NOT MSVC)
    target_compile_options(${target} PRIVATE -march=native)
  endif()
  if(CALIB_PORTABLE_LINUX)
    target_link_options(${target} PRIVATE -static-libstdc++ -static-libgcc)
  endif()
endfunction()

juce_add_gui_app(CalibrationCurveGenerator PRODUCT_NAME "Calibration_Curve_Generator" COMPANY_NAME "AlexMihai" VERSION 1.0.0)
target_sources(CalibrationCurveGenerator PRIVATE
  Source/Main.cpp Source/MainComponent.h Source/MainComponent.cpp
  Source/CurvePlot.h Source/CurvePlot.cpp
  Source/AppLookAndFeel.h
  ${CORE_SOURCES})
target_compile_definitions(CalibrationCurveGenerator PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
  JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:CalibrationCurveGenerator,JUCE_PRODUCT_NAME>"
  JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:CalibrationCurveGenerator,JUCE_VERSION>")
target_link_libraries(CalibrationCurveGenerator PRIVATE
  juce::juce_gui_basics juce::juce_graphics juce::juce_events juce::juce_core
  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
calib_common(CalibrationCurveGenerator)
juce_generate_juce_header(CalibrationCurveGenerator)

if(CALIB_BUILD_TESTS)
  juce_add_console_app(CalibrationCurveGeneratorTests PRODUCT_NAME "CalibrationCurveGeneratorTests")
  target_sources(CalibrationCurveGeneratorTests PRIVATE Tests/CoreTests.cpp ${CORE_SOURCES})
  target_compile_definitions(CalibrationCurveGeneratorTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
  target_link_libraries(CalibrationCurveGeneratorTests PRIVATE juce::juce_core
    juce::juce_recommended_config_flags)
  calib_common(CalibrationCurveGeneratorTests)
endif()
