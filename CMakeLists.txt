cmake_minimum_required(VERSION 3.22)
project(VirtualBoyGo LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Single source of truth for the release version (SettingsPage's version
# label) - bump this by hand at each release tag.
set(VBGO_VERSION "2.0.0")

# Deliberately independent of CMAKE_BUILD_TYPE/$<CONFIG>: an optimized
# Release-config build is still just a local dev build (e.g. for on-device
# perf testing) unless this is explicitly set, so it gets the same
# dev.<commit count> version string as a Debug build - see
# cmake/GenerateVersion.cmake. Only flip this ON for the build you're
# actually cutting as a numbered release (Android: `./gradlew assembleRelease
# -Pofficial=true`; PC: `-DVBGO_RELEASE_BUILD=ON` on the cmake configure line).
option(VBGO_RELEASE_BUILD "This is an official numbered release, not a dev build" OFF)

# MSVC's default source/execution charset isn't UTF-8 - without this, UTF-8
# string literals in source (e.g. any future non-ASCII text in this project)
# get silently mangled at compile time before the UI's UTF-8-aware text
# rendering (see UiFontManager::EnsureGlyphsForText) ever sees them.
if(MSVC)
    add_compile_options(/utf-8)
endif()

include(FetchContent)

# --------------------------------------------------------------------------
# Dependencies - all fetched, no system Vulkan/OpenXR SDK install required.
# --------------------------------------------------------------------------
FetchContent_Declare(
    Vulkan-Headers
    GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Headers.git
    GIT_TAG v1.3.290
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(Vulkan-Headers)

FetchContent_Declare(
    volk
    GIT_REPOSITORY https://github.com/zeux/volk.git
    GIT_TAG vulkan-sdk-1.3.290.0
    GIT_SHALLOW TRUE
)
set(VOLK_PULL_IN_VULKAN OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(volk)
target_link_libraries(volk PUBLIC Vulkan-Headers)

FetchContent_Declare(
    OpenXR-SDK
    GIT_REPOSITORY https://github.com/KhronosGroup/OpenXR-SDK.git
    GIT_TAG release-1.1.42
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(OpenXR-SDK)

# Font glyph rasterization for the menu UI (core/gfx/UiRenderer). Only needs
# to run at font-load time (baking a glyph atlas), not per-frame.
set(FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
FetchContent_Declare(
    freetype
    GIT_REPOSITORY https://gitlab.freedesktop.org/freetype/freetype.git
    GIT_TAG VER-2-13-2
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(freetype)

# --------------------------------------------------------------------------
# Shader compilation: GLSL -> SPIR-V via glslang's C++ API, driven by a tiny
# build-time compiler tool (no system glslc/glslangValidator needed).
#
# Only built for the host (PC) configuration - the compiler is a build tool
# that must run on the machine doing the build, not on the target (Android
# cross-compiles produce ARM64 binaries that can't run during their own
# build). The generated headers land in the committed core/gfx/generated_shaders/
# directory, so an Android build just consumes whatever the PC build most
# recently produced instead of re-running the compiler itself. Re-run a PC
# configure/build after changing shaders/*.vert|frag to refresh them.
# --------------------------------------------------------------------------
set(VBGO_GENERATED_SHADER_DIR "${CMAKE_SOURCE_DIR}/core/gfx/generated_shaders")

if(NOT ANDROID)
    set(ENABLE_GLSLANG_BINARIES OFF CACHE BOOL "" FORCE)
    set(ENABLE_SPVREMAPPER OFF CACHE BOOL "" FORCE)
    set(ENABLE_CTEST OFF CACHE BOOL "" FORCE)
    set(ENABLE_HLSL OFF CACHE BOOL "" FORCE)
    set(ENABLE_OPT OFF CACHE BOOL "" FORCE)
    set(BUILD_EXTERNAL OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        glslang
        GIT_REPOSITORY https://github.com/KhronosGroup/glslang.git
        GIT_TAG vulkan-sdk-1.3.290.0
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(glslang)

    add_executable(vbgo_shader_compiler tools/ShaderCompiler.cpp)
    target_link_libraries(vbgo_shader_compiler PRIVATE glslang SPIRV glslang-default-resource-limits)
    target_include_directories(vbgo_shader_compiler PRIVATE ${glslang_SOURCE_DIR})

    function(vbgo_add_shader OUT_VAR SHADER_SOURCE)
        get_filename_component(SHADER_NAME ${SHADER_SOURCE} NAME)
        set(HEADER_OUT "${VBGO_GENERATED_SHADER_DIR}/${SHADER_NAME}.h")
        add_custom_command(
            OUTPUT ${HEADER_OUT}
            COMMAND ${CMAKE_COMMAND} -E make_directory "${VBGO_GENERATED_SHADER_DIR}"
            COMMAND $<TARGET_FILE:vbgo_shader_compiler> ${SHADER_SOURCE} ${HEADER_OUT}
            DEPENDS ${SHADER_SOURCE} vbgo_shader_compiler
            VERBATIM
        )
        set(${OUT_VAR} ${HEADER_OUT} PARENT_SCOPE)
    endfunction()

    vbgo_add_shader(UI_VERT_HEADER ${CMAKE_SOURCE_DIR}/shaders/ui.vert)
    vbgo_add_shader(UI_SOLID_FRAG_HEADER ${CMAKE_SOURCE_DIR}/shaders/ui_solid.frag)
    vbgo_add_shader(UI_TEXT_FRAG_HEADER ${CMAKE_SOURCE_DIR}/shaders/ui_text.frag)
    vbgo_add_shader(UI_IMAGE_FRAG_HEADER ${CMAKE_SOURCE_DIR}/shaders/ui_image.frag)
    vbgo_add_shader(UI_IMAGE_ROUNDED_FRAG_HEADER ${CMAKE_SOURCE_DIR}/shaders/ui_image_rounded.frag)
    vbgo_add_shader(SCREEN_PATTERN_FRAG_HEADER ${CMAKE_SOURCE_DIR}/shaders/screen_pattern.frag)
    add_custom_target(vbgo_shaders DEPENDS ${UI_VERT_HEADER} ${UI_SOLID_FRAG_HEADER}
                                            ${UI_TEXT_FRAG_HEADER} ${UI_IMAGE_FRAG_HEADER} ${UI_IMAGE_ROUNDED_FRAG_HEADER}
                                            ${SCREEN_PATTERN_FRAG_HEADER})
endif()

# --------------------------------------------------------------------------
# Virtual Boy emulator core - libretro/beetle-vb-libretro (a fork of
# Mednafen's VB emulation), vendored as a git submodule at
# third_party/beetle-vb-libretro, GPL-2.0 licensed (see that folder's
# COPYING). Source list mirrors that submodule's own Makefile.common (the
# core has no CMake build of its own - it's normally built via GNU make -
# so this enumerates the same files Makefile.common lists for
# HAVE_GRIFFIN!=1, NEED_BLIP=1, rather than a directory glob, to stay in
# sync with exactly what upstream's own build compiles).
# --------------------------------------------------------------------------
set(VB_CORE_DIR ${CMAKE_SOURCE_DIR}/third_party/beetle-vb-libretro)
add_library(vb_core STATIC
    ${VB_CORE_DIR}/mednafen/hw_cpu/v810/v810_cpu.cpp
    ${VB_CORE_DIR}/mednafen/mempatcher.cpp
    ${VB_CORE_DIR}/libretro.cpp
    ${VB_CORE_DIR}/mednafen/vb/vsu.c
    ${VB_CORE_DIR}/mednafen/vb/input.c
    ${VB_CORE_DIR}/mednafen/vb/timer.c
    ${VB_CORE_DIR}/mednafen/vb/vip.c
    ${VB_CORE_DIR}/mednafen/hw_cpu/v810/fpu-new/softfloat.c
    ${VB_CORE_DIR}/mednafen/sound/Blip_Buffer.c
    ${VB_CORE_DIR}/mednafen/state.c
    ${VB_CORE_DIR}/mednafen/settings.c
    ${VB_CORE_DIR}/libretro-common/compat/compat_strl.c
    ${VB_CORE_DIR}/libretro-common/compat/compat_snprintf.c
)
target_include_directories(vb_core PUBLIC
    ${VB_CORE_DIR}
    ${VB_CORE_DIR}/mednafen
    ${VB_CORE_DIR}/mednafen/include
    ${VB_CORE_DIR}/libretro-common/include
)
# Matches upstream's own Makefile flags (see Makefile/Makefile.common) for a
# WANT_32BPP, non-static-linking, non-HAVE_GRIFFIN build.
target_compile_definitions(vb_core PRIVATE
    WANT_32BPP
    __LIBRETRO__
    STDC_HEADERS
    __STDC_LIMIT_MACROS
    MEDNAFEN_VERSION="0.9.31"
    MEDNAFEN_VERSION_NUMERIC=931
)
if(MSVC)
    # Mirrors the Makefile's windows_msvc* platform block - the submodule
    # already ships MSVC-specific compat code (mednafen/msvc_compat.h,
    # mednafen/hw_cpu/v810/v810_op_table_msvc.inc) for exactly this.
    target_compile_definitions(vb_core PRIVATE INLINE=_inline NOMINMAX _CRT_SECURE_NO_DEPRECATE)
else()
    target_compile_definitions(vb_core PRIVATE INLINE=inline)
endif()

# --------------------------------------------------------------------------
# Core app: OpenXR session/swapchain lifecycle + Vulkan rendering. Shared,
# platform-agnostic code - the actual per-platform entry points (pc/, pc2d/,
# android/) live under platform/ and just call into this.
# --------------------------------------------------------------------------
add_library(vbgo_app STATIC
    core/app/OpenXrApp.cpp
    core/gfx/VulkanRenderer.cpp
    core/gfx/UiRenderer.cpp
    core/gfx/UiRendererPipelines.cpp
    core/gfx/UiFontManager.cpp
    core/gfx/UiVulkanUtils.cpp
    core/gfx/UiIconSet.cpp
    core/menu/AppMenu.cpp
    core/menu/MenuWidgets.cpp
    core/menu/pages/MainPage.cpp
    core/menu/pages/SettingsPage.cpp
    core/menu/pages/RomSelectPage.cpp
    core/menu/pages/EmulatorButtonMapPage.cpp
    core/menu/pages/MoveScreenPage.cpp
    core/emu/Emulator.cpp
    core/emu/AudioOutput.cpp
    core/input/XrInput.cpp
    core/input/ButtonMapping.cpp
    core/io/Settings.cpp
)
target_include_directories(vbgo_app PUBLIC core ${CMAKE_SOURCE_DIR}/third_party ${CMAKE_SOURCE_DIR}/platform)
target_link_libraries(vbgo_app PUBLIC openxr_loader volk freetype vb_core)
target_compile_definitions(vbgo_app PUBLIC XR_USE_GRAPHICS_API_VULKAN)
if(NOT ANDROID)
    add_dependencies(vbgo_app vbgo_shaders)
endif()

# Generates generated/Version.h (kGeneratedVersionString - see
# AppMenuLayout.h) fresh on every build, not just every configure, so the
# commit count always reflects HEAD even if nothing else triggered a
# reconfigure.
set(VBGO_GENERATED_VERSION_DIR "${CMAKE_BINARY_DIR}/generated")
add_custom_target(vbgo_version ALL
    COMMAND ${CMAKE_COMMAND}
            -DSRC_DIR=${CMAKE_SOURCE_DIR}
            -DOUT_FILE=${VBGO_GENERATED_VERSION_DIR}/Version.h
            -DVBGO_VERSION=${VBGO_VERSION}
            -DVBGO_RELEASE_BUILD=${VBGO_RELEASE_BUILD}
            -P ${CMAKE_SOURCE_DIR}/cmake/GenerateVersion.cmake
    BYPRODUCTS ${VBGO_GENERATED_VERSION_DIR}/Version.h
    COMMENT "Generating version header"
)
target_include_directories(vbgo_app PUBLIC ${VBGO_GENERATED_VERSION_DIR})
add_dependencies(vbgo_app vbgo_version)

if(ANDROID)
    target_compile_definitions(vbgo_app PUBLIC XR_USE_PLATFORM_ANDROID)
    target_sources(vbgo_app PRIVATE platform/android/AndroidPlatform.cpp)
else()
    target_sources(vbgo_app PRIVATE platform/desktop/DesktopPlatform.cpp)
endif()

if(ANDROID)
    find_library(ANDROID_LIB android)
    find_library(LOG_LIB log)

    add_library(virtualboygo SHARED
        platform/android/AndroidMain.cpp
    )
    target_link_libraries(virtualboygo PRIVATE vbgo_app android_native_app_glue ${ANDROID_LIB} ${LOG_LIB})
    target_include_directories(virtualboygo PRIVATE ${ANDROID_NDK}/sources/android/native_app_glue)
    # ANativeActivity_onCreate (in android_native_app_glue.c) is only looked
    # up dynamically by the OS, never called from our own code, so the
    # linker drops it as unreferenced unless forced to keep it.
    target_link_options(virtualboygo PRIVATE "-u" "ANativeActivity_onCreate")

    add_library(android_native_app_glue STATIC
        ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c
    )
    target_include_directories(android_native_app_glue PUBLIC ${ANDROID_NDK}/sources/android/native_app_glue)
else()
    add_executable(VirtualBoyGoPC
        platform/pc/Main.cpp
    )
    target_link_libraries(VirtualBoyGoPC PRIVATE vbgo_app)

    # MSVC's incremental linker has been observed to silently keep a stale
    # .exe (identical byte-for-byte) across rebuilds when only an upstream
    # static lib's contents changed, without erroring - forcing a full
    # relink every time trades a bit of link speed for actually picking up
    # changes, which matters a lot for the F5 edit-build-run loop.
    if(MSVC)
        target_link_options(VirtualBoyGoPC PRIVATE "/INCREMENTAL:NO")
    endif()

    if(TARGET openxr_loader AND WIN32)
        get_target_property(OPENXR_LOADER_TYPE openxr_loader TYPE)
        if(OPENXR_LOADER_TYPE STREQUAL "SHARED_LIBRARY")
            add_custom_command(
                TARGET VirtualBoyGoPC POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:openxr_loader> $<TARGET_FILE_DIR:VirtualBoyGoPC>
            )
        endif()
    endif()

    # DesktopPlatform::LoadAssetBytes() reads relative to the working
    # directory, which is the exe's own folder for how we've been
    # running/testing it.
    add_custom_command(
        TARGET VirtualBoyGoPC POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_SOURCE_DIR}/assets/game_image.png" $<TARGET_FILE_DIR:VirtualBoyGoPC>
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_SOURCE_DIR}/assets/fonts" "$<TARGET_FILE_DIR:VirtualBoyGoPC>/fonts"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_SOURCE_DIR}/assets/icons" "$<TARGET_FILE_DIR:VirtualBoyGoPC>/icons"
    )

    # --------------------------------------------------------------------
    # Flat desktop window, no headset/OpenXR runtime needed at all - the
    # fast local-iteration debug build. Reuses the same UiRenderer/AppMenu
    # content-drawing code as the composition-layer quads on the headset
    # builds, just presented into a normal window swapchain instead of an
    # OpenXR session.
    # --------------------------------------------------------------------
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.4
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(glfw)

    add_executable(VirtualBoyGoPC2D
        platform/pc2d/Main.cpp
    )
    target_link_libraries(VirtualBoyGoPC2D PRIVATE vbgo_app glfw)
    if(MSVC)
        target_link_options(VirtualBoyGoPC2D PRIVATE "/INCREMENTAL:NO")
    endif()

    add_custom_command(
        TARGET VirtualBoyGoPC2D POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_SOURCE_DIR}/assets/game_image.png" $<TARGET_FILE_DIR:VirtualBoyGoPC2D>
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_SOURCE_DIR}/assets/fonts" "$<TARGET_FILE_DIR:VirtualBoyGoPC2D>/fonts"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_SOURCE_DIR}/assets/icons" "$<TARGET_FILE_DIR:VirtualBoyGoPC2D>/icons"
    )
endif()
