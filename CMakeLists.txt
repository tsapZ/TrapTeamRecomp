# trapteamrecomp - ReXGlue Recompiled Project
#
# SDK boilerplate lives in generated/rexglue.cmake.

cmake_minimum_required(VERSION 3.25)
project(trapteamrecomp LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build the SDK from the pinned submodule when present; otherwise generated/rexglue.cmake falls back
# to an installed SDK found through find_package. Pass -DREXSDK_DIR=<path> to use another checkout.
if(NOT REXSDK_DIR AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/rexglue-sdk/CMakeLists.txt")
    set(REXSDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/rexglue-sdk" CACHE PATH
        "Path to the rexglue-sdk source tree" FORCE)
endif()

# Vulkan next to D3D12 (the SDK leaves it off on Windows): gpu_backend = "vulkan" picks it; D3D12
# stays the default (as in SwapForceRecomp, where both ran about equally fast).
set(REXGLUE_USE_VULKAN ON CACHE BOOL "Enable Vulkan graphics backend")

include(generated/rexglue.cmake)

# Portal of Power support (thirdparty/skylanders-portal, shared with other Skylanders recomps).
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/skylanders-portal/CMakeLists.txt")
    message(FATAL_ERROR "thirdparty/skylanders-portal is missing. Run: git submodule update --init --recursive")
endif()
add_subdirectory(thirdparty/skylanders-portal)

# Recompiled code is generated from the user's own default.xex and is not in the repository.
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/generated/default/sources.cmake")
    message(WARNING
        "No recompiled code yet (generated/default/sources.cmake is missing).\n"
        "  1. Put your extracted game disc in rom/ (rom/default.xex must exist).\n"
        "  2. Run:  cmake --build --preset <preset> --target trapteamrecomp_codegen\n"
        "  3. Re-run:  cmake --preset <preset>\n"
        "  4. Build:  cmake --build --preset <preset>\n"
        "See README.md.")
endif()

# Sources
set(TRAPTEAMRECOMP_SOURCES
    src/main.cpp
    src/game/alchemy/iga11_archive.cpp
    src/game/alchemy/igz8_file.cpp
    src/game/alchemy/lzma_decoder.cpp
    src/game/controller_picture.cpp
    src/game/kernel_stubs.cpp
    src/game/portal_prompt_watch.cpp
    src/game/game_fonts.cpp
    src/game/game_text.cpp
    src/game/toy_hats.cpp
    src/game/toy_portraits.cpp
    src/game/toy_upgrades.cpp
    src/game/xenos_dxt.cpp
    src/localization.cpp
    src/settings_pages.cpp
    src/dev/gpu_diagnostics.cpp
    src/dev/input_script.cpp
    src/dev/write_watch.cpp
    src/hooks/fxaa_scale_hook.cpp
    src/hooks/intro_skip_hook.cpp
    src/hooks/scene_size_hook.cpp
    src/hooks/ultrawide_hook.cpp
)

if(WIN32)
    add_executable(trapteamrecompiled WIN32 ${TRAPTEAMRECOMP_SOURCES})
else()
    add_executable(trapteamrecompiled ${TRAPTEAMRECOMP_SOURCES})
endif()

rexglue_setup_target(trapteamrecompiled GPU_PLUGINS xenos)

if(REXSDK_DIR)
    # An installed SDK ships imgui's headers next to its own; an in-tree build must point at them.
    target_include_directories(trapteamrecompiled PRIVATE "${REXSDK_DIR}/thirdparty/imgui")
endif()

# Seed a fresh build dir with the settings-file template. Never overwrites an existing one: the
# in-game F4 Settings overlay saves the user's own changes to this exact file.
if(NOT EXISTS "${CMAKE_BINARY_DIR}/trapteamrecomp.toml")
    file(COPY_FILE "${CMAKE_CURRENT_SOURCE_DIR}/trapteamrecomp.toml.example" "${CMAKE_BINARY_DIR}/trapteamrecomp.toml")
endif()

# --- Pinned game executable -------------------------------------------------
set(TRAPTEAMRECOMP_XEX_HASH_FILE "${CMAKE_CURRENT_SOURCE_DIR}/docs/game/default.xex.sha256")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${TRAPTEAMRECOMP_XEX_HASH_FILE}")
file(READ "${TRAPTEAMRECOMP_XEX_HASH_FILE}" TRAPTEAMRECOMP_XEX_HASH_TEXT)
string(REGEX MATCH "^[0-9a-fA-F]+" TRAPTEAMRECOMP_XEX_SHA256 "${TRAPTEAMRECOMP_XEX_HASH_TEXT}")
string(LENGTH "${TRAPTEAMRECOMP_XEX_SHA256}" TRAPTEAMRECOMP_XEX_SHA256_LEN)
if(NOT TRAPTEAMRECOMP_XEX_SHA256_LEN EQUAL 64)
    message(FATAL_ERROR "docs/game/default.xex.sha256 must start with a 64-character hex SHA-256 (found ${TRAPTEAMRECOMP_XEX_SHA256_LEN} hex characters)")
endif()

add_library(trapteamrecomp_verify STATIC src/xex_verify.cpp)
target_include_directories(trapteamrecomp_verify PUBLIC src)
# Uses the SDK's vendored, portable SHA-256 (thirdparty/crypto) instead of a platform crypto API
# (BCrypt on Windows has no Linux/macOS equivalent), so this one implementation covers every preset.
if(NOT REXSDK_DIR)
    message(FATAL_ERROR "REXSDK_DIR must be set (trapteamrecomp_verify needs its vendored SHA-256 implementation)")
endif()
target_include_directories(trapteamrecomp_verify PRIVATE "${REXSDK_DIR}/thirdparty")
target_sources(trapteamrecomp_verify PRIVATE "${REXSDK_DIR}/thirdparty/crypto/sha256.cpp")

target_link_libraries(trapteamrecompiled PRIVATE trapteamrecomp_verify skylanders_portal_rex)
# The exe's icon: the game's logo from the owner's disc (rom/nxeart), made at build time (#9).
skylanders_game_icon(trapteamrecompiled "${CMAKE_CURRENT_SOURCE_DIR}/rom")
target_compile_definitions(trapteamrecompiled PRIVATE TRAPTEAMRECOMP_XEX_SHA256="${TRAPTEAMRECOMP_XEX_SHA256}")

enable_testing()
add_executable(trapteamrecomp_tests tests/xex_verify_test.cpp)
target_link_libraries(trapteamrecomp_tests PRIVATE trapteamrecomp_verify)
add_test(NAME xex_verify COMMAND trapteamrecomp_tests)

add_executable(trapteamrecomp_xexcheck src/tools/xex_check_cli.cpp)
target_link_libraries(trapteamrecomp_xexcheck PRIVATE trapteamrecomp_verify)
target_compile_definitions(trapteamrecomp_xexcheck PRIVATE TRAPTEAMRECOMP_XEX_SHA256="${TRAPTEAMRECOMP_XEX_SHA256}")

# Process-boundary test: runs the actual built trapteamrecomp_xexcheck.exe against a non-ASCII path to
# catch argv narrowing (a plain main(argc, char**) loses characters outside the system code page on
# Windows). Depends on trapteamrecomp_xexcheck existing in the same output directory at test time.
add_executable(xex_check_cli_argv_test tests/xex_check_cli_argv_test.cpp)
add_test(NAME xex_check_cli_argv COMMAND xex_check_cli_argv_test)

add_executable(xex_check_exit_code_test tests/xex_check_exit_code_test.cpp)
target_include_directories(xex_check_exit_code_test PRIVATE src)
add_test(NAME xex_check_exit_code COMMAND xex_check_exit_code_test)
