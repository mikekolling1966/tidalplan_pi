cmake_minimum_required(VERSION 3.15)
project(tidalplan_pi VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# OpenCPN bundles its own wxWidgets 3.2 inside the .app.
# ALL working plugins link directly against those bundled frameworks
# (otool shows @executable_path/../Frameworks//libwx_*-3.2.dylib).
# We must do the same — -undefined dynamic_lookup alone does not work.
#
# Strategy:
#   1. Compile with Homebrew wx 3.2 headers (same API, compatible ABI)
#   2. Link directly against OpenCPN's bundled wx dylibs
#   3. Post-build: fix embedded paths to @executable_path/../Frameworks//
# ---------------------------------------------------------------------------
set(OCPN_APP "/Applications/OpenCPN.app" CACHE PATH "OpenCPN application bundle")
set(OCPN_FRAMEWORKS "${OCPN_APP}/Contents/Frameworks")
set(OCPN_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include"
    CACHE PATH "Directory containing ocpn_plugin.h")

# wx headers for compilation — use Homebrew 3.2 (same API as bundled 3.2.9)
set(WX_CONFIG /opt/homebrew/Cellar/wxwidgets@3.2/3.2.10/bin/wx-config-3.2)
execute_process(
    COMMAND ${WX_CONFIG} --cxxflags
    OUTPUT_VARIABLE WX_CXXFLAGS OUTPUT_STRIP_TRAILING_WHITESPACE)
# Turn the space-separated flags string into a CMake list
separate_arguments(WX_CXXFLAGS_LIST UNIX_COMMAND "${WX_CXXFLAGS}")

# wx dylibs we need from OpenCPN's bundle (only what our code actually uses)
set(OCPN_WX_DYLIBS
    "${OCPN_FRAMEWORKS}/libwx_baseu-3.2.dylib"
    "${OCPN_FRAMEWORKS}/libwx_osx_cocoau_core-3.2.dylib"
    "${OCPN_FRAMEWORKS}/libwx_baseu_net-3.2.dylib"
    # Note: no adv — OpenCPN doesn't bundle it; on macOS the adv controls
    # (wxDatePickerCtrl, wxSpinCtrlDouble) are in core
)

# ---------------------------------------------------------------------------
# Plugin sources
# ---------------------------------------------------------------------------
add_library(tidalplan_pi SHARED
    src/tidalplan_pi.cpp
    src/tidalplan_panel.cpp
)

target_include_directories(tidalplan_pi PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${OCPN_INCLUDE_DIR}
)

# All wx -I and -D flags in one go
target_compile_options(tidalplan_pi PRIVATE ${WX_CXXFLAGS_LIST})

# Link against OpenCPN's OWN bundled wx + required macOS frameworks.
# OpenCPN API symbols (opencpn_plugin_118, GetOCPNCanvasWindow, GetRoute_Plugin,
# etc.) live in the OpenCPN binary itself — resolved at runtime via dynamic_lookup.
target_link_libraries(tidalplan_pi PRIVATE
    ${OCPN_WX_DYLIBS}
    "-framework IOKit"
    "-framework Carbon"
    "-framework Cocoa"
    "-framework QuartzCore"
    "-framework AudioToolbox"
    "-framework OpenGL"
)
target_link_options(tidalplan_pi PRIVATE -undefined dynamic_lookup)

# ---------------------------------------------------------------------------
# Post-build: rewrite embedded wx dylib paths from absolute to
# @executable_path/../Frameworks//  (the form all other OpenCPN plugins use)
# ---------------------------------------------------------------------------
foreach(DYLIB ${OCPN_WX_DYLIBS})
    get_filename_component(DYLIB_NAME "${DYLIB}" NAME)
    add_custom_command(TARGET tidalplan_pi POST_BUILD
        COMMAND install_name_tool -change
            "${DYLIB}"
            "@executable_path/../Frameworks//${DYLIB_NAME}"
            "$<TARGET_FILE:tidalplan_pi>"
        COMMENT "Fixing install name: ${DYLIB_NAME}"
    )
endforeach()

# ---------------------------------------------------------------------------
# Install
# ---------------------------------------------------------------------------
set(PLUGIN_INSTALL_DIR
    "$ENV{HOME}/Library/Application Support/OpenCPN/Contents/PlugIns"
    CACHE PATH "OpenCPN plugin install directory")

install(TARGETS tidalplan_pi
    LIBRARY DESTINATION "${PLUGIN_INSTALL_DIR}")

install(DIRECTORY data/
    DESTINATION "${PLUGIN_INSTALL_DIR}/tidalplan_pi_data"
    OPTIONAL)
