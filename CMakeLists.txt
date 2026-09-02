cmake_minimum_required(VERSION 3.24)

set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

project(cardputer-zero-gba
    VERSION 0.1.0
    DESCRIPTION "Pixel-perfect GBA emulator frontend for Cardputer Zero"
    LANGUAGES C CXX
)

include(GNUInstallDirs)
include(CTest)
find_package(Threads REQUIRED)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(CZ_GBA_FETCH_SDL2 "Fetch SDL2 if find_package cannot locate it" ON)
set(CZ_GBA_SDL2_SOURCE_DIR "" CACHE PATH "Optional SDL2 source checkout to add_subdirectory")
set(CZ_GBA_SDL2_ROOT "" CACHE PATH "Optional SDL2 installation prefix for a system SDL2 build")
option(CZ_GBA_BUNDLE_SDL2 "Build bundled SDL2 from extern/SDL when a system SDL2 target is unavailable" ON)
option(CZ_GBA_PREFER_BUNDLED_SDL2 "Build bundled SDL2 before searching the target SDK" OFF)
option(CZ_GBA_SDL_SHARED "Build bundled SDL2 as a shared library" ON)
option(CZ_GBA_SDL_STATIC "Build bundled SDL2 as a static library" OFF)
option(CZ_GBA_REQUIRE_K230_DRM "Fail configuration unless Linux DRM/KMS UAPI headers are available" OFF)
option(CZ_GBA_REQUIRE_K230_WAYLAND_SHM "Fail configuration unless the K230 Wayland small-source presenter ABI is available" OFF)
set(CZ_GBA_TDVP_WAYLAND_SDK_OVERLAY "" CACHE PATH "Firmware-matched TDVP Wayland client development overlay")
set(CZ_GBA_TDVP_WAYLAND_SCANNER "" CACHE FILEPATH "Firmware-matched host wayland-scanner executable")
set(CZ_GBA_TDVP_WAYLAND_PROTOCOLS_DIR "" CACHE PATH "Firmware-matched wayland-protocols source directory")
option(CZ_GBA_USE_SYSTEM_MGBA "Use system libmgba instead of bundled mGBA" OFF)
option(CZ_GBA_BUNDLE_MGBA "Build bundled mGBA from extern/mgba" ON)
set(CZ_GBA_MGBA_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/extern/mgba" CACHE PATH "mGBA source checkout")
set(CZ_GBA_MGBA_ROOT "" CACHE PATH "Optional libmgba installation prefix for a system mGBA build")
option(CZ_GBA_TDVP_COMPOSABLE_FEED "Require independently packaged SDL2 and libmGBA release prefixes for TDVP feed builds" OFF)
option(CZ_GBA_ENABLE_DEB_PACKAGE "Enable CPack Debian package generation" OFF)
set(CZ_GBA_PACKAGE_ARCHITECTURE "" CACHE STRING "Debian package architecture override, for example arm64")
set(CZ_GBA_PRIVATE_LIBDIR "lib/cardputer-zero-gba" CACHE STRING "Private runtime directory for Cardputer Zero APPLaunch packaging")

if (CZ_GBA_TDVP_COMPOSABLE_FEED)
    # TDVP's distribution feed has one independent IPK owner for every
    # non-ABI runtime.  Do not fall back to FetchContent, git submodules, or
    # the firmware sysroot in this mode: SDL2 and libmGBA must come from the
    # release-local staging prefixes supplied by their own feed recipes.
    if (NOT CZ_GBA_SDL2_ROOT OR NOT CZ_GBA_MGBA_ROOT)
        message(FATAL_ERROR
            "CZ_GBA_TDVP_COMPOSABLE_FEED requires CZ_GBA_SDL2_ROOT and CZ_GBA_MGBA_ROOT from the TDVP feed staging release")
    endif()
    set(CZ_GBA_FETCH_SDL2 OFF CACHE BOOL "Fetch SDL2 if find_package cannot locate it" FORCE)
    set(CZ_GBA_BUNDLE_SDL2 OFF CACHE BOOL "Build bundled SDL2 from extern/SDL when a system SDL2 target is unavailable" FORCE)
    set(CZ_GBA_PREFER_BUNDLED_SDL2 OFF CACHE BOOL "Build bundled SDL2 before searching the target SDK" FORCE)
    set(CZ_GBA_USE_SYSTEM_MGBA ON CACHE BOOL "Use system libmgba instead of bundled mGBA" FORCE)
    set(CZ_GBA_BUNDLE_MGBA OFF CACHE BOOL "Build bundled mGBA from extern/mgba" FORCE)
endif()

if (CZ_GBA_USE_SYSTEM_MGBA)
    set(CZ_GBA_BUNDLE_MGBA OFF CACHE BOOL "Build bundled mGBA from extern/mgba" FORCE)
endif()

if (CZ_GBA_BUNDLE_SDL2 AND NOT CZ_GBA_SDL2_SOURCE_DIR AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/extern/SDL/CMakeLists.txt")
    set(CZ_GBA_SDL2_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/extern/SDL" CACHE PATH "Optional SDL2 source checkout to add_subdirectory" FORCE)
endif()

if (NOT CZ_GBA_TDVP_COMPOSABLE_FEED AND NOT CZ_GBA_PREFER_BUNDLED_SDL2)
    find_package(SDL2 CONFIG QUIET)

    if (NOT TARGET SDL2::SDL2)
        find_package(SDL2 QUIET)
    endif()
endif()

if (NOT TARGET SDL2::SDL2 AND CZ_GBA_SDL2_ROOT)
    # A cross-build toolchain can deliberately restrict find_package() to the
    # firmware sysroot.  A distribution build may still supply SDL2 from a
    # release-local SDK prefix, so construct the standard imported target from
    # that explicit prefix instead of falling back to extern/SDL or FetchContent.
    find_path(CZ_GBA_SDL2_INCLUDE_DIR
        NAMES SDL.h
        HINTS "${CZ_GBA_SDL2_ROOT}"
        PATH_SUFFIXES include/SDL2
        NO_CMAKE_FIND_ROOT_PATH)
    find_library(CZ_GBA_SDL2_LIBRARY
        NAMES SDL2-2.0 SDL2
        HINTS "${CZ_GBA_SDL2_ROOT}"
        PATH_SUFFIXES lib
        NO_CMAKE_FIND_ROOT_PATH)
    if (CZ_GBA_SDL2_INCLUDE_DIR AND CZ_GBA_SDL2_LIBRARY)
        add_library(SDL2::SDL2 SHARED IMPORTED GLOBAL)
        set_target_properties(SDL2::SDL2 PROPERTIES
            IMPORTED_LOCATION "${CZ_GBA_SDL2_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${CZ_GBA_SDL2_INCLUDE_DIR}")
    endif()
endif()

if (NOT TARGET SDL2::SDL2 AND CZ_GBA_SDL2_SOURCE_DIR)
    if (EXISTS "${CZ_GBA_SDL2_SOURCE_DIR}/CMakeLists.txt")
        set(SDL_SHARED ${CZ_GBA_SDL_SHARED} CACHE BOOL "" FORCE)
        set(SDL_STATIC ${CZ_GBA_SDL_STATIC} CACHE BOOL "" FORCE)
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
    set(SDL_SHARED ${CZ_GBA_SDL_SHARED} CACHE BOOL "" FORCE)
    set(SDL_STATIC ${CZ_GBA_SDL_STATIC} CACHE BOOL "" FORCE)
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
    # Upstream mGBA installs its headers and library but does not guarantee a
    # CMake package-config file on every release.  Keep the standard package
    # lookup first, then create the same imported target from a normal prefix.
    # This lets distribution packages (including TDVP's shared feed runtime)
    # be consumed without bundling a second emulation core into every app.
    if (NOT CZ_GBA_TDVP_COMPOSABLE_FEED)
        find_package(mgba CONFIG QUIET)
    endif()
    if (NOT TARGET mgba)
        find_path(CZ_GBA_MGBA_INCLUDE_DIR
            NAMES mgba/core/core.h
            HINTS "${CZ_GBA_MGBA_ROOT}"
            PATH_SUFFIXES include
            NO_CMAKE_FIND_ROOT_PATH)
        find_library(CZ_GBA_MGBA_LIBRARY
            NAMES mgba
            HINTS "${CZ_GBA_MGBA_ROOT}"
            PATH_SUFFIXES lib
            NO_CMAKE_FIND_ROOT_PATH)
        if (CZ_GBA_MGBA_INCLUDE_DIR AND CZ_GBA_MGBA_LIBRARY)
            add_library(mgba SHARED IMPORTED GLOBAL)
            set_target_properties(mgba PROPERTIES
                IMPORTED_LOCATION "${CZ_GBA_MGBA_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${CZ_GBA_MGBA_INCLUDE_DIR}")
        else()
            message(FATAL_ERROR "libmgba was not found. Set CZ_GBA_MGBA_ROOT, CMAKE_PREFIX_PATH, or enable CZ_GBA_BUNDLE_MGBA.")
        endif()
    endif()
endif()

add_executable(cardputer-zero-gba
    src/main.cpp
    src/app/app.cpp
    src/app/command_bar.cpp
    src/app/toast.cpp
    src/cheat/cheat_manager.cpp
    src/cheat/cheat_parser.cpp
    src/config/app_config.cpp
    src/emulation/emulation_runtime.cpp
    src/platform/sdl_platform.cpp
    src/platform/sdl_audio.cpp
    src/platform/tdvp_k230_drm.cpp
    src/platform/tdvp_k230_wayland_shm.cpp
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

add_executable(cardputer-zero-gba-pcm-ring-smoke
    tests/pcm_ring_smoke.cpp
)

add_executable(cardputer-zero-gba-emulation-runtime-smoke
    tests/emulation_runtime_smoke.cpp
    src/emulation/emulation_runtime.cpp
    src/app/app.cpp
    src/app/command_bar.cpp
    src/app/toast.cpp
    src/cheat/cheat_manager.cpp
    src/cheat/cheat_parser.cpp
    src/storage/paths.cpp
    src/storage/rom_scanner.cpp
)

add_executable(cardputer-zero-gba-app-menu-smoke
    tests/app_menu_smoke.cpp
    src/app/app.cpp
    src/app/command_bar.cpp
    src/app/toast.cpp
    src/cheat/cheat_manager.cpp
    src/cheat/cheat_parser.cpp
    src/storage/paths.cpp
    src/storage/rom_scanner.cpp
)

add_executable(cardputer-zero-gba-presentation-profile-smoke
    tests/presentation_profile_smoke.cpp
)

add_executable(cardputer-zero-gba-renderer-layout-smoke
    tests/renderer_layout_smoke.cpp
    src/render/canvas.cpp
    src/render/font_5x7.cpp
    src/render/renderer.cpp
    src/render/utf8_text.cpp
)

add_executable(cardputer-zero-gba-cli-profile-smoke
    tests/cli_profile_smoke.cpp
    src/util/cli.cpp
)

target_compile_features(cardputer-zero-gba-packaging-smoke PRIVATE cxx_std_20)
target_compile_features(cardputer-zero-gba-audio-core-smoke PRIVATE cxx_std_20)
target_compile_features(cardputer-zero-gba-pcm-ring-smoke PRIVATE cxx_std_20)
target_compile_features(cardputer-zero-gba-emulation-runtime-smoke PRIVATE cxx_std_20)
target_compile_features(cardputer-zero-gba-app-menu-smoke PRIVATE cxx_std_20)
target_compile_features(cardputer-zero-gba-presentation-profile-smoke PRIVATE cxx_std_20)
target_compile_features(cardputer-zero-gba-renderer-layout-smoke PRIVATE cxx_std_20)
target_compile_features(cardputer-zero-gba-cli-profile-smoke PRIVATE cxx_std_20)

if (BUILD_TESTING)
    add_test(NAME cardputer-zero-gba-packaging-smoke
             COMMAND cardputer-zero-gba-packaging-smoke)
    add_test(NAME cardputer-zero-gba-audio-core-smoke
             COMMAND cardputer-zero-gba-audio-core-smoke)
    add_test(NAME cardputer-zero-gba-pcm-ring-smoke
             COMMAND cardputer-zero-gba-pcm-ring-smoke)
    add_test(NAME cardputer-zero-gba-emulation-runtime-smoke
             COMMAND cardputer-zero-gba-emulation-runtime-smoke)
    add_test(NAME cardputer-zero-gba-app-menu-smoke
             COMMAND cardputer-zero-gba-app-menu-smoke)
    add_test(NAME cardputer-zero-gba-presentation-profile-smoke
             COMMAND cardputer-zero-gba-presentation-profile-smoke)
    add_test(NAME cardputer-zero-gba-renderer-layout-smoke
             COMMAND cardputer-zero-gba-renderer-layout-smoke)
    add_test(NAME cardputer-zero-gba-cli-profile-smoke
             COMMAND cardputer-zero-gba-cli-profile-smoke)
endif()

target_include_directories(cardputer-zero-gba
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

set(CZ_GBA_HAS_DRM_KMS OFF)
if (CMAKE_SYSTEM_NAME MATCHES "^(Linux|Buildroot)$")
    find_path(CZ_GBA_DRM_UAPI_INCLUDE_DIR NAMES drm/drm.h)
    find_file(CZ_GBA_DRM_FOURCC_HEADER NAMES drm/drm_fourcc.h
        HINTS "${CZ_GBA_DRM_UAPI_INCLUDE_DIR}")
    if (CZ_GBA_DRM_UAPI_INCLUDE_DIR AND CZ_GBA_DRM_FOURCC_HEADER)
        set(CZ_GBA_HAS_DRM_KMS ON)
        target_include_directories(cardputer-zero-gba SYSTEM PRIVATE "${CZ_GBA_DRM_UAPI_INCLUDE_DIR}")
        target_compile_definitions(cardputer-zero-gba PRIVATE CZ_GBA_HAS_DRM_KMS=1)
    endif()
endif()

if (CZ_GBA_REQUIRE_K230_DRM AND NOT CZ_GBA_HAS_DRM_KMS)
    message(FATAL_ERROR "TDVP K230 packaging requires Linux DRM/KMS UAPI headers in the selected SDK sysroot")
endif()

set(CZ_GBA_HAS_TDVP_WAYLAND_SHM OFF)
set(CZ_GBA_HAS_TDVP_WAYLAND_PRESENTER OFF)
if (CMAKE_SYSTEM_NAME MATCHES "^(Linux|Buildroot)$")
    set(CZ_GBA_WAYLAND_HINTS)
    if (CZ_GBA_TDVP_WAYLAND_SDK_OVERLAY)
        list(APPEND CZ_GBA_WAYLAND_HINTS "${CZ_GBA_TDVP_WAYLAND_SDK_OVERLAY}")
    endif()
    find_path(CZ_GBA_WAYLAND_CLIENT_INCLUDE_DIR
        NAMES wayland-client.h
        HINTS ${CZ_GBA_WAYLAND_HINTS}
        PATH_SUFFIXES include)
    find_library(CZ_GBA_WAYLAND_CLIENT_LIBRARY
        NAMES wayland-client
        HINTS ${CZ_GBA_WAYLAND_HINTS}
        PATH_SUFFIXES lib)
    find_library(CZ_GBA_FFI_LIBRARY
        NAMES ffi
        HINTS ${CZ_GBA_WAYLAND_HINTS}
        PATH_SUFFIXES lib)
    if (CZ_GBA_WAYLAND_CLIENT_INCLUDE_DIR AND CZ_GBA_WAYLAND_CLIENT_LIBRARY AND CZ_GBA_FFI_LIBRARY)
        set(CZ_GBA_HAS_TDVP_WAYLAND_SHM ON)
        target_include_directories(cardputer-zero-gba SYSTEM PRIVATE "${CZ_GBA_WAYLAND_CLIENT_INCLUDE_DIR}")
        target_link_libraries(cardputer-zero-gba PRIVATE "${CZ_GBA_WAYLAND_CLIENT_LIBRARY}" "${CZ_GBA_FFI_LIBRARY}")
        target_compile_definitions(cardputer-zero-gba PRIVATE CZ_GBA_HAS_TDVP_WAYLAND_SHM=1)

        set(CZ_GBA_WAYLAND_SCANNER_EXECUTABLE "${CZ_GBA_TDVP_WAYLAND_SCANNER}")
        if (NOT CZ_GBA_WAYLAND_SCANNER_EXECUTABLE)
            find_program(CZ_GBA_WAYLAND_SCANNER_EXECUTABLE NAMES wayland-scanner NO_CMAKE_FIND_ROOT_PATH)
        endif()
        find_file(CZ_GBA_VIEWPORTER_PROTOCOL_XML
            NAMES viewporter.xml
            HINTS "${CMAKE_CURRENT_SOURCE_DIR}/extern/SDL/wayland-protocols"
            NO_CMAKE_FIND_ROOT_PATH)
        find_file(CZ_GBA_LINUX_DMABUF_PROTOCOL_XML
            NAMES linux-dmabuf-unstable-v1.xml
            HINTS "${CZ_GBA_TDVP_WAYLAND_PROTOCOLS_DIR}"
            PATH_SUFFIXES unstable/linux-dmabuf
            NO_CMAKE_FIND_ROOT_PATH)
        if (CZ_GBA_WAYLAND_SCANNER_EXECUTABLE AND CZ_GBA_VIEWPORTER_PROTOCOL_XML AND CZ_GBA_LINUX_DMABUF_PROTOCOL_XML)
            set(CZ_GBA_TDVP_PROTOCOL_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/tdvp-wayland-protocols")
            set(CZ_GBA_TDVP_PROTOCOL_SOURCES
                "${CZ_GBA_TDVP_PROTOCOL_GENERATED_DIR}/viewporter-client-protocol.c"
                "${CZ_GBA_TDVP_PROTOCOL_GENERATED_DIR}/linux-dmabuf-unstable-v1-client-protocol.c")
            set(CZ_GBA_TDVP_PROTOCOL_HEADERS
                "${CZ_GBA_TDVP_PROTOCOL_GENERATED_DIR}/viewporter-client-protocol.h"
                "${CZ_GBA_TDVP_PROTOCOL_GENERATED_DIR}/linux-dmabuf-unstable-v1-client-protocol.h")
            add_custom_command(
                OUTPUT ${CZ_GBA_TDVP_PROTOCOL_SOURCES} ${CZ_GBA_TDVP_PROTOCOL_HEADERS}
                COMMAND "${CMAKE_COMMAND}" -E make_directory "${CZ_GBA_TDVP_PROTOCOL_GENERATED_DIR}"
                COMMAND "${CZ_GBA_WAYLAND_SCANNER_EXECUTABLE}" client-header
                    "${CZ_GBA_VIEWPORTER_PROTOCOL_XML}"
                    "${CZ_GBA_TDVP_PROTOCOL_GENERATED_DIR}/viewporter-client-protocol.h"
                COMMAND "${CZ_GBA_WAYLAND_SCANNER_EXECUTABLE}" private-code
                    "${CZ_GBA_VIEWPORTER_PROTOCOL_XML}"
                    "${CZ_GBA_TDVP_PROTOCOL_GENERATED_DIR}/viewporter-client-protocol.c"
                COMMAND "${CZ_GBA_WAYLAND_SCANNER_EXECUTABLE}" client-header
                    "${CZ_GBA_LINUX_DMABUF_PROTOCOL_XML}"
                    "${CZ_GBA_TDVP_PROTOCOL_GENERATED_DIR}/linux-dmabuf-unstable-v1-client-protocol.h"
                COMMAND "${CZ_GBA_WAYLAND_SCANNER_EXECUTABLE}" private-code
                    "${CZ_GBA_LINUX_DMABUF_PROTOCOL_XML}"
                    "${CZ_GBA_TDVP_PROTOCOL_GENERATED_DIR}/linux-dmabuf-unstable-v1-client-protocol.c"
                DEPENDS "${CZ_GBA_VIEWPORTER_PROTOCOL_XML}" "${CZ_GBA_LINUX_DMABUF_PROTOCOL_XML}"
                VERBATIM)
            target_sources(cardputer-zero-gba PRIVATE ${CZ_GBA_TDVP_PROTOCOL_SOURCES})
            target_include_directories(cardputer-zero-gba PRIVATE "${CZ_GBA_TDVP_PROTOCOL_GENERATED_DIR}")
            target_compile_definitions(cardputer-zero-gba PRIVATE CZ_GBA_HAS_TDVP_WAYLAND_PRESENTER=1)
            set(CZ_GBA_HAS_TDVP_WAYLAND_PRESENTER ON)
        endif()
    endif()
endif()

if (CZ_GBA_REQUIRE_K230_WAYLAND_SHM AND NOT CZ_GBA_HAS_TDVP_WAYLAND_PRESENTER)
    message(FATAL_ERROR
        "TDVP K230 packaging requires the firmware-matched Wayland client ABI, wayland-scanner, viewporter.xml, and linux-dmabuf-unstable-v1.xml")
endif()

target_include_directories(cardputer-zero-gba-audio-core-smoke
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_include_directories(cardputer-zero-gba-pcm-ring-smoke
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_include_directories(cardputer-zero-gba-emulation-runtime-smoke
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_include_directories(cardputer-zero-gba-app-menu-smoke
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_include_directories(cardputer-zero-gba-presentation-profile-smoke
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_include_directories(cardputer-zero-gba-renderer-layout-smoke
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_include_directories(cardputer-zero-gba-cli-profile-smoke
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

target_compile_definitions(cardputer-zero-gba-emulation-runtime-smoke
    PRIVATE
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
)

target_compile_definitions(cardputer-zero-gba
    PRIVATE
        CARDPUTER_ZERO_GBA_VERSION="${PROJECT_VERSION}"
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
)

target_compile_definitions(cardputer-zero-gba-app-menu-smoke
    PRIVATE
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
)

target_compile_definitions(cardputer-zero-gba-renderer-layout-smoke
    PRIVATE
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
)

target_compile_definitions(cardputer-zero-gba-cli-profile-smoke
    PRIVATE
        CARDPUTER_ZERO_GBA_VERSION="${PROJECT_VERSION}"
)

target_link_libraries(cardputer-zero-gba
    PRIVATE
        SDL2::SDL2
        mgba
        Threads::Threads
)

target_link_libraries(cardputer-zero-gba-pcm-ring-smoke
    PRIVATE
        Threads::Threads
)

target_link_libraries(cardputer-zero-gba-emulation-runtime-smoke
    PRIVATE
        Threads::Threads
)

target_link_libraries(cardputer-zero-gba-audio-core-smoke
    PRIVATE
        mgba
)

target_link_libraries(cardputer-zero-gba-renderer-layout-smoke
    PRIVATE
        SDL2::SDL2
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
    target_compile_options(cardputer-zero-gba-app-menu-smoke PRIVATE /W4 /permissive-)
    target_compile_options(cardputer-zero-gba-presentation-profile-smoke PRIVATE /W4 /permissive-)
    target_compile_options(cardputer-zero-gba-renderer-layout-smoke PRIVATE /W4 /permissive-)
    target_compile_options(cardputer-zero-gba-cli-profile-smoke PRIVATE /W4 /permissive-)
else()
    target_compile_options(cardputer-zero-gba PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(cardputer-zero-gba-audio-core-smoke PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(cardputer-zero-gba-app-menu-smoke PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(cardputer-zero-gba-presentation-profile-smoke PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(cardputer-zero-gba-renderer-layout-smoke PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(cardputer-zero-gba-cli-profile-smoke PRIVATE -Wall -Wextra -Wpedantic)
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
