# VirtualBoyGo (Rework branch)

This branch is a from-scratch rework of VirtualBoyGo targeting **OpenXR +
Vulkan** instead of the legacy, Quest-only Oculus VrApi/SampleFramework stack
used on `master`. The goal is a single app, sharing one Vulkan rendering
codebase, that runs on Quest, Frame, and other Android VR headsets, plus a PC
build (streamed to the headset via Virtual Desktop/SteamVR/Link) for fast
desktop debugging.

## Current status

An independent project (own `CMakeLists.txt`, no dependency on the
`External/OpenXR-SDK-Source` submodule - that's kept around purely as
reference material from the exploration phase, see below). Confirmed working
on both platforms, live on a Quest 3:

- OpenXR instance/system/session/swapchain lifecycle (`Platform/OpenXrApp`).
- A Vulkan device created via `XR_KHR_vulkan_enable2`'s delegated-creation
  path, plus a minimal render pipeline (`Platform/VulkanRenderer`).
- A **quad composition layer** rendering a real decoded JPEG
  (`assets/test_image.jpg`) as a floating panel - this is the important
  piece, since it's the OpenXR analogue of the old VrApi `ovrLayerCylinder2`
  that the eventual emulator screen/menu rendering will build on. The main
  eye buffers are plain black; the debug cube only shows up as a fallback if
  the test image fails to load.

Not started yet: porting `FrontendGo`'s menu/UI or the actual emulator core
into this shell.

## Project layout

```
CMakeLists.txt              root build - FetchContent for volk, Vulkan-Headers,
                             OpenXR-SDK (loader), glslang (shader compiler)
Platform/
  OpenXrApp.cpp/.h           instance/system/session/swapchain/event loop
  VulkanRenderer.cpp/.h      Vulkan device/pipelines/rendering
  XrMath.h                   small self-contained matrix math
  AssetLoader.cpp/.h         cross-platform file loading (APK assets / PC files)
  ThirdParty/stb_image.h     vendored image decoder (JPEG/PNG)
  GeneratedShaders/          SPIR-V headers, produced by the PC build's
                             shader compiler and committed (Android's
                             cross-compile can't build/run glslang itself)
  PC/Main.cpp                desktop entry point
  Android/AndroidMain.cpp    NativeActivity entry point
shaders/                     GLSL sources (compiled to SPIR-V at PC build time)
assets/                      shared between PC and Android (Gradle assets dir)
Android/                     Gradle wrapper project (externalNativeBuild -> root CMakeLists.txt)
External/OpenXR-SDK-Source/  reference only, not built by the root CMakeLists.txt
```

## Building - PC

No Vulkan SDK required (Vulkan is loaded dynamically via `volk`; shaders are
compiled via glslang's C++ API, fetched and built as part of this project).

```
cmake -B build-pc -G "Visual Studio 17 2022" -A x64 -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build build-pc --config Debug
build-pc\Debug\VirtualBoyGoPC.exe
```

Requires an OpenXR runtime already registered and a headset actively
connected (Virtual Desktop, SteamVR, or Oculus Link) - it streams straight to
the headset with no APK/adb involved. `XR_ERROR_FORM_FACTOR_UNAVAILABLE`
means the headset isn't currently connected, not a code bug.

## Building - Android (Quest)

Requires Android SDK (compileSdk 34, build-tools 34.0.0) + NDK 23.2.8568313 +
CMake 3.22.1 (e.g. via `sdkmanager`).

```
cd Android
echo "sdk.dir=/path/to/Android/Sdk" > local.properties   # forward slashes, even on Windows
./gradlew assembleDebug
adb install -r app/build/outputs/apk/debug/app-debug.apk
adb shell am start -n com.nintendont.virtualboygo/android.app.NativeActivity
```

If you change a shader (`shaders/*.vert|frag`), rebuild the **PC** target
first to refresh the committed headers in `Platform/GeneratedShaders/` before
building Android - the Android cross-compile doesn't build the shader
compiler itself.
