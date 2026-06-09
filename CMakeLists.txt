cmake_minimum_required(VERSION 3.24)

set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

project(cardputer-zero-gba
    VERSION 0.1.0
    DESCRIPTION "Pixel-perfect GBA emulator frontend for Cardputer Zero"
    LANGUAGES C CXX
)

include(GNUInstallDirs)
include(CTest)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(CZ_GBA_FETCH_SDL2 "Fetch SDL2 if find_package cannot locate it" ON)
set(CZ_GBA_SDL2_SOURCE_DIR "" CACHE PATH "Optional SDL2 source checkout to add_subdirectory")
option(CZ_GBA_BUNDLE_SDL2 "Build bundled SDL2 from extern/SDL when a system SDL2 target is unavailable" ON)
option(CZ_GBA_USE_SYSTEM_MGBA "Use system libmgba instead of bundled mGBA" OFF)
option(CZ_GBA_BUNDLE_MGBA "Build bundled mGBA from extern/mgba" ON)
set(CZ_GBA_MGBA_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/extern/mgba" CACHE PATH "mGBA source checkout")
option(CZ_GBA_ENABLE_DEB_PACKAGE "Enable CPack Debian package generation" OFF)
set(CZ_GBA_PACKAGE_ARCHITECTURE "" CACHE STRING "Debian package architecture override, for example arm64")
set(CZ_GBA_PRIVATE_LIBDIR "lib/cardputer-zero-gba" CACHE STRING "Private runtime directory for Cardputer Zero APPLaunch packaging")

if (CZ_GBA_USE_SYSTEM_MGBA)
    set(CZ_GBA_BUNDLE_MGBA OFF CACHE BOOL "Build bundled mGBA from extern/mgba" FORCE)
endif()

if (CZ_GBA_BUNDLE_SDL2 AND NOT CZ_GBA_SDL2_SOURCE_DIR AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/extern/SDL/CMakeLists.txt")
    set(CZ_GBA_SDL2_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/extern/SDL" CACHE PATH "Optional SDL2 source checkout to add_subdirectory" FORCE)
endif()

find_package(SDL2 CONFIG QUIET)

if (NOT TARGET SDL2::SDL2)
    find_package(SDL2 QUIET)
endif()

if (NOT TARGET SDL2::SDL2 AND CZ_GBA_SDL2_SOURCE_DIR)
    if (EXISTS "${CZ_GBA_SDL2_SOURCE_DIR}/CMakeLists.txt")
        set(SDL_SHARED ON CACHE BOOL "" FORCE)
        set(SDL_STATIC OFF CACHE BOOL "" FORCE)
        set(SDL_TEST OFF CACHE BOOL "" FORCE)
        add_subdirectory("${CZ_GBA_SDL2_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/_deps/sdl2-build" EXCLUDE_FROM_ALL)
    else()
        message(FATAL_ERROR "CZ_GBA_SDL2_SOURCE_DIR does not contain an SDL2 CMakeLists.txt")
    endif()
endif()

if (NOT TARGET SDL2::SDL2 AND CZ_GBA_FETCH_SDL2)
    include(FetchContent)
    FetchContent_Declare(
        SDL2
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-2.30.11
        GIT_SHALLOW TRUE
    )
    set(SDL_SHARED ON CACHE BOOL "" FORCE)
    set(SDL_STATIC OFF CACHE BOOL "" FORCE)
    set(SDL_TEST OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(SDL2)
endif()

if (NOT TARGET SDL2::SDL2)
    message(FATAL_ERROR "SDL2 was not found. Install SDL2, set CZ_GBA_SDL2_SOURCE_DIR, or enable CZ_GBA_FETCH_SDL2.")
endif()

if (CZ_GBA_BUNDLE_MGBA)
    if (NOT EXISTS "${CZ_GBA_MGBA_SOURCE_DIR}/CMakeLists.txt")
        message(FATAL_ERROR "Bundled mGBA requested but CZ_GBA_MGBA_SOURCE_DIR is invalid")
    endif()

    set(M_CORE_GBA ON CACHE BOOL "" FORCE)
    set(M_CORE_GB OFF CACHE BOOL "" FORCE)
    set(BUILD_STATIC ON CACHE BOOL "" FORCE)
    set(BUILD_SHARED OFF CACHE BOOL "" FORCE)
    set(BUILD_QT OFF CACHE BOOL "" FORCE)
    set(BUILD_SDL OFF CACHE BOOL "" FORCE)
    set(BUILD_GL OFF CACHE BOOL "" FORCE)
    set(BUILD_GLES2 OFF CACHE BOOL "" FORCE)
    set(BUILD_GLES3 OFF CACHE BOOL "" FORCE)
    set(BUILD_TEST OFF CACHE BOOL "" FORCE)
    set(BUILD_SUITE OFF CACHE BOOL "" FORCE)
    set(BUILD_CINEMA OFF CACHE BOOL "" FORCE)
    set(BUILD_ROM_TEST OFF CACHE BOOL "" FORCE)
    set(BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
    set(BUILD_PYTHON OFF CACHE BOOL "" FORCE)
    set(USE_DEBUGGERS OFF CACHE BOOL "" FORCE)
    set(USE_FFMPEG OFF CACHE BOOL "" FORCE)
    set(USE_PNG OFF CACHE BOOL "" FORCE)
    set(USE_ZLIB OFF CACHE BOOL "" FORCE)
    set(USE_MINIZIP OFF CACHE BOOL "" FORCE)
    set(USE_LIBZIP OFF CACHE BOOL "" FORCE)
    set(USE_SQLITE3 OFF CACHE BOOL "" FORCE)
    set(USE_ELF OFF CACHE BOOL "" FORCE)
    set(USE_LUA OFF CACHE BOOL "" FORCE)
    set(USE_LZMA OFF CACHE BOOL "" FORCE)
    set(USE_DISCORD_RPC OFF CACHE BOOL "" FORCE)
    set(ENABLE_SCRIPTING OFF CACHE BOOL "" FORCE)
    add_subdirectory("${CZ_GBA_MGBA_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/_deps/mgba-build" EXCLUDE_FROM_ALL)
elseif (NOT TARGET mgba)
    find_package(mgba CONFIG REQUIRED)
endif()

add_executable(cardputer-zero-gba
    src/main.cpp
    src/app/app.cpp
    src/app/command_bar.cpp
    src/app/toast.cpp
    src/cheat/cheat_manager.cpp
    src/cheat/cheat_parser.cpp
    src/config/app_config.cpp
    src/platform/sdl_platform.cpp
    src/platform/sdl_audio.cpp
    src/input/input_mapper.cpp
    src/core/mgba_core.cpp
    src/render/canvas.cpp
    src/render/font_5x7.cpp
    src/render/renderer.cpp
    src/render/utf8_text.cpp
    src/storage/paths.cpp
    src/storage/rom_scanner.cpp
    src/ui/cheat_menu.cpp
    src/ui/pause_menu.cpp
    src/ui/rom_browser.cpp
    src/ui/settings_menu.cpp
    src/util/cli.cpp
)

add_executable(cardputer-zero-gba-packaging-smoke
    tests/packaging_contract_smoke.cpp
)

add_executable(cardputer-zero-gba-audio-core-smoke
    tests/audio_core_smoke.cpp
    src/core/mgba_core.cpp
    src/storage/rom_scanner.cpp
)

target_compile_features(cardputer-zero-gba-packaging-smoke PRIVATE cxx_std_20)
target_compile_features(cardputer-zero-gba-audio-core-smoke PRIVATE cxx_std_20)

if (BUILD_TESTING)
    add_test(NAME cardputer-zero-gba-packaging-smoke
             COMMAND cardputer-zero-gba-packaging-smoke)
    add_test(NAME cardputer-zero-gba-audio-core-smoke
             COMMAND cardputer-zero-gba-audio-core-smoke)
endif()

target_include_directories(cardputer-zero-gba
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_include_directories(cardputer-zero-gba-audio-core-smoke
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_compile_definitions(cardputer-zero-gba-packaging-smoke
    PRIVATE
        CZ_GBA_SOURCE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}"
)

target_compile_definitions(cardputer-zero-gba-audio-core-smoke
    PRIVATE
        CZ_GBA_SOURCE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}"
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
)

target_compile_definitions(cardputer-zero-gba
    PRIVATE
        CARDPUTER_ZERO_GBA_VERSION="${PROJECT_VERSION}"
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
)

target_link_libraries(cardputer-zero-gba
    PRIVATE
        SDL2::SDL2
        mgba
)

target_link_libraries(cardputer-zero-gba-audio-core-smoke
    PRIVATE
        mgba
)

if (TARGET SDL2::SDL2main)
    target_link_libraries(cardputer-zero-gba PRIVATE SDL2::SDL2main)
endif()

if (WIN32)
    add_custom_command(TARGET cardputer-zero-gba POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL2::SDL2>
            $<TARGET_FILE_DIR:cardputer-zero-gba>
    )
endif()

if (MSVC)
    target_compile_options(cardputer-zero-gba PRIVATE /W4 /permissive-)
    target_compile_options(cardputer-zero-gba-audio-core-smoke PRIVATE /W4 /permissive-)
else()
    target_compile_options(cardputer-zero-gba PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(cardputer-zero-gba-audio-core-smoke PRIVATE -Wall -Wextra -Wpedantic)
endif()

install(TARGETS cardputer-zero-gba
    RUNTIME DESTINATION "${CZ_GBA_PRIVATE_LIBDIR}"
    PERMISSIONS
        OWNER_READ OWNER_WRITE OWNER_EXECUTE
        GROUP_READ GROUP_EXECUTE
        WORLD_READ WORLD_EXECUTE
)

install(PROGRAMS packaging/cardputer-zero-gba
    DESTINATION "${CMAKE_INSTALL_BINDIR}"
    PERMISSIONS
        OWNER_READ OWNER_WRITE OWNER_EXECUTE
        GROUP_READ GROUP_EXECUTE
        WORLD_READ WORLD_EXECUTE
)

install(PROGRAMS packaging/cardputer-zero-gba-applaunch
    DESTINATION "${CZ_GBA_PRIVATE_LIBDIR}"
    PERMISSIONS
        OWNER_READ OWNER_WRITE OWNER_EXECUTE
        GROUP_READ GROUP_EXECUTE
        WORLD_READ WORLD_EXECUTE
)

install(FILES desktop/cardputer-zero-gba.desktop
    DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/APPLaunch/applications"
    PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
)

install(FILES packaging/cardputer-zero-gba.png
    DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/APPLaunch/share/images"
    PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
)

install(FILES assets/icons/cardputer-zero-gba-64.png
    DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/64x64/apps"
    RENAME cardputer-zero-gba.png
    PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
)

install(FILES assets/icons/cardputer-zero-gba-128.png
    DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/128x128/apps"
    RENAME cardputer-zero-gba.png
    PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
)

install(DIRECTORY assets/themes/
    DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/cardputer-zero-gba/themes"
    FILE_PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
    DIRECTORY_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

install(DIRECTORY assets/fonts/
    DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/cardputer-zero-gba/fonts"
    FILE_PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
    DIRECTORY_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

install(FILES README.md LICENSE
    DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/doc/cardputer-zero-gba"
    PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
)

if (CZ_GBA_ENABLE_DEB_PACKAGE)
    file(TO_CMAKE_PATH "${CMAKE_BINARY_DIR}" CZ_GBA_CPACK_BUILD_DIR)
    file(TO_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}" CZ_GBA_CPACK_SOURCE_DIR)

    if (CZ_GBA_PACKAGE_ARCHITECTURE)
        set(CZ_GBA_DEBIAN_ARCHITECTURE "${CZ_GBA_PACKAGE_ARCHITECTURE}")
    elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64)$")
        set(CZ_GBA_DEBIAN_ARCHITECTURE "amd64")
    elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
        set(CZ_GBA_DEBIAN_ARCHITECTURE "arm64")
    elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "^(armv7l|armhf)$")
        set(CZ_GBA_DEBIAN_ARCHITECTURE "armhf")
    else()
        set(CZ_GBA_DEBIAN_ARCHITECTURE "${CMAKE_SYSTEM_PROCESSOR}")
    endif()

    set(CZ_GBA_CPACK_ARCHITECTURE_LINE
        "set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE \"${CZ_GBA_DEBIAN_ARCHITECTURE}\")")
    set(CZ_GBA_CPACK_PACKAGE_FILE_NAME
        "${PROJECT_NAME}_${PROJECT_VERSION}_${CZ_GBA_DEBIAN_ARCHITECTURE}")

    find_program(CZ_GBA_CPACK_COMMAND NAMES cpack REQUIRED)
    set(CZ_GBA_CPACK_CONFIG "${CMAKE_BINARY_DIR}/CardputerZeroGbaCPackConfig.cmake")
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CardputerZeroGbaDeb.cmake.in"
        "${CZ_GBA_CPACK_CONFIG}"
        @ONLY
    )
    add_custom_target(cardputer-zero-gba-deb
        COMMAND "${CZ_GBA_CPACK_COMMAND}" --config "${CZ_GBA_CPACK_CONFIG}"
        DEPENDS cardputer-zero-gba
        COMMENT "Building cardputer-zero-gba Debian package"
    )
endif()
