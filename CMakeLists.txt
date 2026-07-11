cmake_minimum_required(VERSION 3.22)
project(VirtualBoyGo LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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

# --------------------------------------------------------------------------
# Shader compilation: GLSL -> SPIR-V via glslang's C++ API, driven by a tiny
# build-time compiler tool (no system glslc/glslangValidator needed).
#
# Only built for the host (PC) configuration - the compiler is a build tool
# that must run on the machine doing the build, not on the target (Android
# cross-compiles produce ARM64 binaries that can't run during their own
# build). The generated headers land in the committed Platform/GeneratedShaders/
# directory, so an Android build just consumes whatever the PC build most
# recently produced instead of re-running the compiler itself. Re-run a PC
# configure/build after changing shaders/*.vert|frag to refresh them.
# --------------------------------------------------------------------------
set(VBGO_GENERATED_SHADER_DIR "${CMAKE_SOURCE_DIR}/Platform/GeneratedShaders")

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
    add_custom_target(vbgo_shaders DEPENDS ${CUBE_VERT_HEADER} ${CUBE_FRAG_HEADER} ${TEXTURED_QUAD_VERT_HEADER}
                                            ${TEXTURED_QUAD_FRAG_HEADER})
endif()

# --------------------------------------------------------------------------
# Core app: OpenXR session/swapchain lifecycle + Vulkan rendering.
# Shared between the Android (.so) and PC (.exe) entry points.
# --------------------------------------------------------------------------
add_library(vbgo_app STATIC
    Platform/OpenXrApp.cpp
    Platform/VulkanRenderer.cpp
    Platform/AssetLoader.cpp
)
target_include_directories(vbgo_app PUBLIC Platform ${VBGO_GENERATED_SHADER_DIR}/..)
target_link_libraries(vbgo_app PUBLIC openxr_loader volk)
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
        Platform/Android/AndroidMain.cpp
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
        Platform/PC/Main.cpp
    )
    target_link_libraries(VirtualBoyGoPC PRIVATE vbgo_app)

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
        Platform/PC2D/Main.cpp
    )
    target_link_libraries(VirtualBoyGoPC2D PRIVATE vbgo_app glfw)

    add_custom_command(
        TARGET VirtualBoyGoPC2D POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_SOURCE_DIR}/assets/test_image.jpg" $<TARGET_FILE_DIR:VirtualBoyGoPC2D>
    )
endif()
