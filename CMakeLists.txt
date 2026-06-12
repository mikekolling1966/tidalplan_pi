cmake_minimum_required(VERSION 3.15)
project(tidalplan_pi VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(OCPN_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include"
    CACHE PATH "Directory containing ocpn_plugin.h")

# ---------------------------------------------------------------------------
# Plugin sources (same on all platforms)
# ---------------------------------------------------------------------------
add_library(tidalplan_pi SHARED
    src/tidalplan_pi.cpp
    src/tidalplan_panel.cpp
)

target_include_directories(tidalplan_pi PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${OCPN_INCLUDE_DIR}
)

# ---------------------------------------------------------------------------
# macOS — link against OpenCPN's bundled wxWidgets inside the .app bundle.
# OpenCPN bundles wx 3.2 in Contents/Frameworks; we must use those exact
# dylibs or the wx ABI won't match.  Use Homebrew wx headers for compilation
# (same API/ABI as the bundled runtime).
# ---------------------------------------------------------------------------
if(APPLE)
    set(OCPN_APP "/Applications/OpenCPN.app" CACHE PATH "OpenCPN application bundle")
    set(OCPN_FRAMEWORKS "${OCPN_APP}/Contents/Frameworks")

    set(WX_CONFIG /opt/homebrew/Cellar/wxwidgets@3.2/3.2.10/bin/wx-config-3.2
        CACHE FILEPATH "wx-config for Homebrew wxWidgets 3.2")

    execute_process(
        COMMAND ${WX_CONFIG} --cxxflags
        OUTPUT_VARIABLE WX_CXXFLAGS OUTPUT_STRIP_TRAILING_WHITESPACE)
    separate_arguments(WX_CXXFLAGS_LIST UNIX_COMMAND "${WX_CXXFLAGS}")
    target_compile_options(tidalplan_pi PRIVATE ${WX_CXXFLAGS_LIST})

    # Only the three dylibs our code actually uses (adv controls are in core on macOS)
    set(OCPN_WX_DYLIBS
        "${OCPN_FRAMEWORKS}/libwx_baseu-3.2.dylib"
        "${OCPN_FRAMEWORKS}/libwx_osx_cocoau_core-3.2.dylib"
        "${OCPN_FRAMEWORKS}/libwx_baseu_net-3.2.dylib"
    )

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

    # Rewrite embedded dylib paths to the @executable_path form that
    # OpenCPN expects from plugins
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

    set(PLUGIN_INSTALL_DIR
        "$ENV{HOME}/Library/Application Support/OpenCPN/Contents/PlugIns"
        CACHE PATH "OpenCPN plugin install directory")

# ---------------------------------------------------------------------------
# Linux (including ARM / Armbian / Raspberry Pi OS)
# Uses the system wxWidgets package — install with:
#   sudo apt install libwxgtk3.2-dev   (or libwxgtk3.0-dev on older distros)
#
# OpenCPN API symbols are resolved at runtime from the running OpenCPN binary.
# ---------------------------------------------------------------------------
elseif(UNIX)
    # Prefer wx 3.2; fall back to 3.0 if not available
    find_program(WX_CONFIG wx-config
        HINTS /usr/bin /usr/local/bin)
    if(NOT WX_CONFIG)
        message(FATAL_ERROR "wx-config not found — install libwxgtk3.2-dev (or libwxgtk3.0-dev)")
    endif()

    execute_process(
        COMMAND ${WX_CONFIG} --cxxflags
        OUTPUT_VARIABLE WX_CXXFLAGS OUTPUT_STRIP_TRAILING_WHITESPACE)
    separate_arguments(WX_CXXFLAGS_LIST UNIX_COMMAND "${WX_CXXFLAGS}")
    target_compile_options(tidalplan_pi PRIVATE ${WX_CXXFLAGS_LIST})

    # On Linux the adv component contains wxDatePickerCtrl and wxSpinCtrlDouble
    execute_process(
        COMMAND ${WX_CONFIG} --libs core,base,net,adv
        OUTPUT_VARIABLE WX_LIBS OUTPUT_STRIP_TRAILING_WHITESPACE)
    separate_arguments(WX_LIBS_LIST UNIX_COMMAND "${WX_LIBS}")

    target_link_libraries(tidalplan_pi PRIVATE ${WX_LIBS_LIST})

    # OpenCPN API symbols live in the running opencpn binary — don't require
    # them at link time (equivalent to macOS -undefined dynamic_lookup)
    target_link_options(tidalplan_pi PRIVATE
        -Wl,--allow-shlib-undefined
    )

    # OpenCPN on Linux requires the lib prefix (lib*_pi.so)

    set(PLUGIN_INSTALL_DIR
        "$ENV{HOME}/.opencpn/plugins"
        CACHE PATH "OpenCPN plugin install directory")

endif()

# ---------------------------------------------------------------------------
# Install
# ---------------------------------------------------------------------------
install(TARGETS tidalplan_pi
    LIBRARY DESTINATION "${PLUGIN_INSTALL_DIR}")

install(DIRECTORY data/
    DESTINATION "${PLUGIN_INSTALL_DIR}/tidalplan_pi_data"
    OPTIONAL)
