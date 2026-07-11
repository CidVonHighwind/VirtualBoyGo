# VirtualBoyGo (Rework branch)

This branch is a from-scratch rework of VirtualBoyGo targeting **OpenXR**
instead of the legacy, Quest-only Oculus VrApi/SampleFramework stack used on
`master`. The goal is a single OpenXR-based app that runs on Quest, Frame,
and other Android VR headsets, plus a PC build for fast desktop debugging.

## Current status

Starting point only: the [Khronos `hello_xr` sample](https://github.com/KhronosGroup/OpenXR-SDK-Source/tree/main/src/tests/hello_xr),
pulled in unmodified via the `External/OpenXR-SDK-Source` submodule, proven
to build and install on Quest. No VirtualBoyGo/FrontendGo application logic
has been ported over yet - that's the next step, integrating the emulator
core and menu/UI (`FrontendGo`) into this OpenXR app shell.

## Building the OpenXR test app (Quest)

Requires Android SDK (compileSdk 34, build-tools 34.0.0) + NDK 23.2.8568313 +
CMake 3.22.1 installed (e.g. via `sdkmanager`).

```
git submodule update --init --recursive
cd External/OpenXR-SDK-Source/src/tests/hello_xr
echo "sdk.dir=/path/to/Android/Sdk" > local.properties   # forward slashes, even on Windows
./gradlew assembleOpenGLESDebug
adb install -r build/outputs/apk/OpenGLES/debug/hello_xr-OpenGLES-debug.apk
```
