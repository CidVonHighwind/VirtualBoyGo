# VirtualBoyGo (Rework branch)

This branch is a from-scratch rework of VirtualBoyGo targeting **OpenXR +
Vulkan** instead of the legacy, Quest-only Oculus VrApi/SampleFramework stack
used on `master`. The goal is a single app, sharing one Vulkan rendering
codebase, that runs on Quest, Frame, and other Android VR headsets, plus a PC
build (streamed to the headset via Virtual Desktop/SteamVR/Link) for fast
desktop debugging.

## Current status

Confirmed working on both platforms, live on a Quest 3: OpenXR instance/
session/swapchain lifecycle (`core/OpenXrApp`), a Vulkan device via
`XR_KHR_vulkan_enable2` (`core/VulkanRenderer`), and quad/cylinder
composition layers (`core/ui/UiRenderer`, `core/ui/AppMenu`) for the menu and
emulator screen - the main eye buffers stay plain black.

A third build target, `VirtualBoyGoPC2D`, renders into a plain GLFW window
instead of an OpenXR session - no headset or runtime needed, for fast local
iteration.

## Project layout

```
CMakeLists.txt              root build - FetchContent for volk, Vulkan-Headers,
                             OpenXR-SDK (loader), glslang (shader compiler)
core/                        shared, platform-agnostic app code
  OpenXrApp.cpp/.h           instance/system/session/swapchain/event loop
  VulkanRenderer.cpp/.h      Vulkan device/pipelines/rendering
  XrMath.h                   small self-contained matrix math
  AssetLoader.cpp/.h         cross-platform file loading (APK assets / PC files)
  third_party/stb_image.h    vendored image decoder (JPEG/PNG)
  generated_shaders/         SPIR-V headers, produced by the PC build's
                             shader compiler and committed (Android's
                             cross-compile can't build/run glslang itself)
platform/                    per-platform entry points only, call into core/
  pc/Main.cpp                desktop entry point (OpenXR, streamed to headset)
  pc2d/Main.cpp               flat GLFW window entry point (no headset needed)
  android/AndroidMain.cpp    NativeActivity entry point
shaders/                     GLSL sources (compiled to SPIR-V at PC build time)
assets/                      shared between PC and Android (Gradle assets dir)
tools/ShaderCompiler.cpp     glslang-based GLSL -> SPIR-V compiler, PC-only build tool
android/                     Gradle wrapper project (externalNativeBuild -> root CMakeLists.txt)
External/OpenXR-SDK-Source/  local, git-ignored reference clone (not a submodule, not
                             built by the root CMakeLists.txt) - see "Reference material" below
```

## Reference material (not version-controlled)

`External/OpenXR-SDK-Source/` is a git-ignored local clone of
[KhronosGroup/OpenXR-SDK-Source](https://github.com/KhronosGroup/OpenXR-SDK-Source)
kept from the exploration phase - not required to build, not fetched by
cloning this repo. Recreate it if you want it back:

```
git clone https://github.com/KhronosGroup/OpenXR-SDK-Source.git External/OpenXR-SDK-Source
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

### Setup

Set your SDK path in `android/local.properties` (forward slashes, even on Windows):

```
sdk.dir=C\:/Users/<you>/AppData/Local/Android/Sdk
```

Or set the `ANDROID_HOME` environment variable instead.

Gradle (`./gradlew`) also needs a JDK, separate from the Android SDK/NDK
above - if you get an error like "JAVA_HOME is not set" or "java: command not
found", set `JAVA_HOME` before invoking gradlew. Android Studio already
bundles a JDK, so if it's installed, point at that instead of installing one
separately:

```
# PowerShell, one-time for the session:
$env:JAVA_HOME = "C:\Program Files\Android\Android Studio\jbr"

# or inline per command:
JAVA_HOME="C:\Program Files\Android\Android Studio\jbr" ./gradlew assembleRelease
```

(Adjust the path if Android Studio is installed elsewhere, or use any other
JDK 17+ install's home directory.)

### Build & install (USB)

```
cd android
./gradlew assembleDebug
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

Or `assembleRelease` / `app/build/outputs/apk/release/app-release.apk`.

### Run

```
adb shell am start -n com.nintendont.virtualboygo/android.app.NativeActivity
```

### Wi-Fi ADB (wireless debugging)

One-time: on the headset, **Settings → Developer → Wireless debugging → Pair
device with pairing code**, then `adb pair <ip>:<pairing-port>`.

Each session: `adb connect <headset-ip>:5555`, then use the same
`adb install`/`adb shell am start` commands above. Reconnect (`adb connect`
again) if it drops when the headset sleeps; `adb disconnect` to go back to
USB.

If you change a shader (`shaders/*.vert|frag`), rebuild the **PC** target
first to refresh the committed headers in `core/generated_shaders/`
before building Android - the Android cross-compile doesn't build the shader
compiler itself.

## Building - PC, 2D debug (no headset)

Same CMake project - configuring the PC build (above) also produces this
target. No OpenXR runtime or headset required at all:

```
build-pc\Debug\VirtualBoyGoPC2D.exe
```
