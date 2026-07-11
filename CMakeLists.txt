cmake_minimum_required(VERSION 3.22)
project(VirtualBoyGo LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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

# Font glyph rasterization for the menu UI (core/ui/UiRenderer). Only needs
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
# build). The generated headers land in the committed core/generated_shaders/
# directory, so an Android build just consumes whatever the PC build most
# recently produced instead of re-running the compiler itself. Re-run a PC
# configure/build after changing shaders/*.vert|frag to refresh them.
# --------------------------------------------------------------------------
set(VBGO_GENERATED_SHADER_DIR "${CMAKE_SOURCE_DIR}/core/generated_shaders")

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
        GIT_TAG main
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

    vbgo_add_shader(CUBE_VERT_HEADER ${CMAKE_SOURCE_DIR}/shaders/cube.vert)
    vbgo_add_shader(CUBE_FRAG_HEADER ${CMAKE_SOURCE_DIR}/shaders/cube.frag)
    vbgo_add_shader(TEXTURED_QUAD_VERT_HEADER ${CMAKE_SOURCE_DIR}/shaders/textured_quad.vert)
    vbgo_add_shader(TEXTURED_QUAD_FRAG_HEADER ${CMAKE_SOURCE_DIR}/shaders/textured_quad.frag)
    vbgo_add_shader(UI_VERT_HEADER ${CMAKE_SOURCE_DIR}/shaders/ui.vert)
    vbgo_add_shader(UI_SOLID_FRAG_HEADER ${CMAKE_SOURCE_DIR}/shaders/ui_solid.frag)
    vbgo_add_shader(UI_TEXT_FRAG_HEADER ${CMAKE_SOURCE_DIR}/shaders/ui_text.frag)
    vbgo_add_shader(UI_IMAGE_FRAG_HEADER ${CMAKE_SOURCE_DIR}/shaders/ui_image.frag)
    vbgo_add_shader(UI_IMAGE_ROUNDED_FRAG_HEADER ${CMAKE_SOURCE_DIR}/shaders/ui_image_rounded.frag)
    add_custom_target(vbgo_shaders DEPENDS ${CUBE_VERT_HEADER} ${CUBE_FRAG_HEADER} ${TEXTURED_QUAD_VERT_HEADER}
                                            ${TEXTURED_QUAD_FRAG_HEADER} ${UI_VERT_HEADER} ${UI_SOLID_FRAG_HEADER}
                                            ${UI_TEXT_FRAG_HEADER} ${UI_IMAGE_FRAG_HEADER} ${UI_IMAGE_ROUNDED_FRAG_HEADER})
endif()

# --------------------------------------------------------------------------
# Core app: OpenXR session/swapchain lifecycle + Vulkan rendering. Shared,
# platform-agnostic code - the actual per-platform entry points (pc/, pc2d/,
# android/) live under platform/ and just call into this.
# --------------------------------------------------------------------------
add_library(vbgo_app STATIC
    core/OpenXrApp.cpp
    core/VulkanRenderer.cpp
    core/AssetLoader.cpp
    core/XrInput.cpp
    core/Emulator.cpp
    core/ui/UiRenderer.cpp
    core/ui/UiRendererPipelines.cpp
    core/ui/UiFontManager.cpp
    core/ui/UiVulkanUtils.cpp
    core/ui/UiIconSet.cpp
    core/ui/pages/MainPage.cpp
    core/ui/pages/SettingsPage.cpp
    core/ui/pages/RomSelectPage.cpp
    core/ui/pages/MenuButtonMapPage.cpp
    core/ui/pages/EmulatorButtonMapPage.cpp
    core/ui/pages/MoveScreenPage.cpp
    core/ui/MenuWidgets.cpp
    core/ui/AppMenu.cpp
)
target_include_directories(vbgo_app PUBLIC core)
target_link_libraries(vbgo_app PUBLIC openxr_loader volk freetype)
target_compile_definitions(vbgo_app PUBLIC XR_USE_GRAPHICS_API_VULKAN)
if(NOT ANDROID)
    add_dependencies(vbgo_app vbgo_shaders)
endif()

if(ANDROID)
    target_compile_definitions(vbgo_app PUBLIC XR_USE_PLATFORM_ANDROID)
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

    # LoadAssetBytes() on PC reads relative to the working directory, which
    # is the exe's own folder for how we've been running/testing it.
    add_custom_command(
        TARGET VirtualBoyGoPC POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_SOURCE_DIR}/assets/test_image.jpg" $<TARGET_FILE_DIR:VirtualBoyGoPC>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_SOURCE_DIR}/assets/game_image.png" $<TARGET_FILE_DIR:VirtualBoyGoPC>
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_SOURCE_DIR}/assets/fonts" "$<TARGET_FILE_DIR:VirtualBoyGoPC>/fonts"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_SOURCE_DIR}/assets/icons" "$<TARGET_FILE_DIR:VirtualBoyGoPC>/icons"
    )

    # --------------------------------------------------------------------
    # Flat desktop window, no headset/OpenXR runtime needed at all - the
    # fast local-iteration debug build. Reuses the same VulkanRenderer
    # content-drawing code (RenderTexturedQuad) as the composition-layer
    # quad on the headset builds, just presented into a normal window
    # swapchain instead of an OpenXR session.
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
                "${CMAKE_SOURCE_DIR}/assets/test_image.jpg" $<TARGET_FILE_DIR:VirtualBoyGoPC2D>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_SOURCE_DIR}/assets/game_image.png" $<TARGET_FILE_DIR:VirtualBoyGoPC2D>
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_SOURCE_DIR}/assets/fonts" "$<TARGET_FILE_DIR:VirtualBoyGoPC2D>/fonts"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_SOURCE_DIR}/assets/icons" "$<TARGET_FILE_DIR:VirtualBoyGoPC2D>/icons"
    )
endif()
