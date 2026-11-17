cmake_minimum_required(VERSION 3.16)
project(drum_suite VERSION 0.6.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
include(GNUInstallDirs)

# Shared header-only DSP (ADAA, biquad, envelope, oversampling, dynamics).
set(HEARTH_DSP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/hearth-dsp"
    CACHE PATH "Path to the hearth-dsp checkout")
if(NOT EXISTS "${HEARTH_DSP_DIR}/include/hdsp/hdsp.hpp")
  message(FATAL_ERROR "hearth-dsp not found at ${HEARTH_DSP_DIR}")
endif()
include_directories("${HEARTH_DSP_DIR}/include")

enable_testing()

set(CLAP_DIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/clap"
    CACHE PATH "Directory where CLAP bundles are installed")
# The LV2 default search path does not include ~/.local/lib/lv2, so the
# prefix-relative default is only correct for system installs.  For a
# per-user install pass -DLV2_DIR=~/.lv2.
set(LV2_DIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/lv2"
    CACHE PATH "Directory where the LV2 bundle is installed")

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
pkg_check_modules(LV2 REQUIRED IMPORTED_TARGET lv2)
pkg_check_modules(CAIRO REQUIRED IMPORTED_TARGET cairo)
pkg_check_modules(FREETYPE REQUIRED IMPORTED_TARGET freetype2)
pkg_check_modules(X11 REQUIRED IMPORTED_TARGET x11)

set(DRUM_UI_SOURCES src/ui/panel.cpp)

# ---------------------------------------------------------------------------
# CLAP plugin: three pages, one X11/Cairo editor.
# ---------------------------------------------------------------------------
add_library(drum_suite MODULE
  src/clap/drum_suite_clap.cpp
  ${DRUM_UI_SOURCES})
target_include_directories(drum_suite PRIVATE
  "${CMAKE_SOURCE_DIR}/src"
  "${CMAKE_SOURCE_DIR}/third_party/clap/include")
target_link_libraries(drum_suite PRIVATE
  PkgConfig::CAIRO PkgConfig::FREETYPE PkgConfig::X11
  Threads::Threads ${CMAKE_DL_LIBS})
target_compile_features(drum_suite PRIVATE cxx_std_20)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(drum_suite PRIVATE -Wall -Wextra -fvisibility=hidden)
endif()
set_target_properties(drum_suite PROPERTIES
  PREFIX ""
  OUTPUT_NAME "DrumSuite"
  SUFFIX ".clap"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/clap")

set(DRUM_RESOURCE_FILES
  "${CMAKE_SOURCE_DIR}/assets/fonts/Jost-ExtraBold.ttf"
  "${CMAKE_SOURCE_DIR}/assets/fonts/OFL.txt")

add_custom_target(drum_suite_bundle ALL
  COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/clap/DrumSuite.resources"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${DRUM_RESOURCE_FILES}
          "${CMAKE_BINARY_DIR}/clap/DrumSuite.resources/"
  DEPENDS drum_suite ${DRUM_RESOURCE_FILES})

install(TARGETS drum_suite LIBRARY DESTINATION "${CLAP_DIR}")
install(FILES ${DRUM_RESOURCE_FILES}
        DESTINATION "${CLAP_DIR}/DrumSuite.resources")

# ---------------------------------------------------------------------------
# Tests and tools
# ---------------------------------------------------------------------------
add_executable(drum_dsp_test test/test_dsp.cpp)
target_include_directories(drum_dsp_test PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_compile_features(drum_dsp_test PRIVATE cxx_std_20)
add_test(NAME drum_dsp COMMAND drum_dsp_test)

add_executable(drum_panel_test test/test_panel.cpp ${DRUM_UI_SOURCES})
target_include_directories(drum_panel_test PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(drum_panel_test PRIVATE PkgConfig::CAIRO PkgConfig::FREETYPE)
target_compile_features(drum_panel_test PRIVATE cxx_std_20)
add_test(NAME drum_panel COMMAND drum_panel_test)

add_executable(drum_geometry_test test/test_geometry.cpp)
target_include_directories(drum_geometry_test PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(drum_geometry_test PRIVATE PkgConfig::CAIRO)
target_compile_features(drum_geometry_test PRIVATE cxx_std_20)
add_test(NAME drum_geometry COMMAND drum_geometry_test)

add_executable(drum_layout_test test/test_layout.cpp ${DRUM_UI_SOURCES})
target_include_directories(drum_layout_test PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(drum_layout_test PRIVATE PkgConfig::CAIRO PkgConfig::FREETYPE)
target_compile_features(drum_layout_test PRIVATE cxx_std_20)
add_test(NAME drum_layout COMMAND drum_layout_test)

add_executable(drum_ui_preview test/ui_preview.cpp ${DRUM_UI_SOURCES})
target_include_directories(drum_ui_preview PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(drum_ui_preview PRIVATE PkgConfig::CAIRO PkgConfig::FREETYPE)
target_compile_features(drum_ui_preview PRIVATE cxx_std_20)

# ---------------------------------------------------------------------------
# Metadata generator: emits lv2/*.ttl from src/params.hpp and src/presets.hpp.
# The test fails if the committed TTL files are stale.
# ---------------------------------------------------------------------------
add_executable(drum_ttl_gen tools/gen_ttl.cpp)
target_include_directories(drum_ttl_gen PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_compile_features(drum_ttl_gen PRIVATE cxx_std_20)
add_test(NAME drum_ttl
  COMMAND drum_ttl_gen --check --root "${CMAKE_SOURCE_DIR}")

# ---------------------------------------------------------------------------
# LV2 build: same DSP and panel as the CLAP, exposed as an LV2 plugin + X11 UI.
# ---------------------------------------------------------------------------
pkg_check_modules(LILV QUIET IMPORTED_TARGET lilv-0)

set(LV2_BUNDLE_DIR "${CMAKE_BINARY_DIR}/lv2/drum-suite.lv2")

add_library(drum_suite_lv2 MODULE src/drum_suite_lv2.cpp)
target_include_directories(drum_suite_lv2 PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(drum_suite_lv2 PRIVATE PkgConfig::LV2)
target_compile_features(drum_suite_lv2 PRIVATE cxx_std_20)
set_target_properties(drum_suite_lv2 PROPERTIES
  PREFIX ""
  OUTPUT_NAME "drum_suite"
  LIBRARY_OUTPUT_DIRECTORY "${LV2_BUNDLE_DIR}")

add_library(drum_suite_lv2_ui MODULE src/ui/lv2_ui.cpp ${DRUM_UI_SOURCES})
target_include_directories(drum_suite_lv2_ui PRIVATE
  "${CMAKE_SOURCE_DIR}/src" "${CMAKE_SOURCE_DIR}/third_party/clap/include")
target_link_libraries(drum_suite_lv2_ui PRIVATE
  PkgConfig::CAIRO PkgConfig::FREETYPE PkgConfig::X11
  PkgConfig::LV2 Threads::Threads)
target_compile_features(drum_suite_lv2_ui PRIVATE cxx_std_20)
set_target_properties(drum_suite_lv2_ui PROPERTIES
  PREFIX ""
  OUTPUT_NAME "drum_suite_ui"
  LIBRARY_OUTPUT_DIRECTORY "${LV2_BUNDLE_DIR}")

add_custom_target(drum_suite_lv2_bundle ALL
  COMMAND ${CMAKE_COMMAND} -E make_directory "${LV2_BUNDLE_DIR}"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${CMAKE_SOURCE_DIR}/lv2/manifest.ttl"
          "${CMAKE_SOURCE_DIR}/lv2/drum_suite.ttl"
          "${CMAKE_SOURCE_DIR}/lv2/presets.ttl"
          "${CMAKE_SOURCE_DIR}/assets/fonts/Jost-ExtraBold.ttf"
          "${CMAKE_SOURCE_DIR}/assets/fonts/OFL.txt"
          "${LV2_BUNDLE_DIR}/"
  DEPENDS drum_suite_lv2 drum_suite_lv2_ui)

# Release tarball: both bundles, ready to drop into ~/.lv2 and ~/.clap.
add_custom_target(dist
  COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/dist"
  COMMAND ${CMAKE_COMMAND} -E tar czf
          "${CMAKE_BINARY_DIR}/dist/drum-suite-${PROJECT_VERSION}-linux-x86_64.tar.gz"
          lv2/drum-suite.lv2 clap/DrumSuite.clap clap/DrumSuite.resources
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
  DEPENDS drum_suite_lv2_bundle drum_suite_bundle
  COMMENT "Packing drum-suite-${PROJECT_VERSION}")

install(TARGETS drum_suite_lv2 drum_suite_lv2_ui
        LIBRARY DESTINATION "${LV2_DIR}/drum-suite.lv2")
install(FILES
  lv2/manifest.ttl
  lv2/drum_suite.ttl
  lv2/presets.ttl
  assets/fonts/Jost-ExtraBold.ttf
  assets/fonts/OFL.txt
  DESTINATION "${LV2_DIR}/drum-suite.lv2")

