# VirtualBoyGo
VirtualBoy emulator for the oculus go

The font used in the emulator is from https://www.planetvb.com/modules/newbb/viewtopic.php?post_id=10801#forumpost10801

## Compiling

- download the "Oculus Mobile SDK 1.23.0" https://developer.oculus.com/downloads/package/oculus-mobile-sdk/
- open "ovr_sdk_mobile_1.23/Application.mk"
- download this repo and copy it into "ovr_sdk_mobile_1.23/VrSamples/"
- download https://github.com/CidVonHighwind/FrontendGo and copy it into "ovr_sdk_mobile_1.23/VrSamples/"
- download https://github.com/CidVonHighwind/BeetleVBLibretroGo and copy it into "ovr_sdk_mobile_1.23/VrSamples/VrEmulators/"
- download "FreeType 2.10.0" https://download.savannah.gnu.org/releases/freetype/ and copy the "include" and the "src" folder into "ovr_sdk_mobile_1.23/VrEmulators/FreeType/" folder
- Also copy the "Android.mk" and the "Application.mk" files from the FrontendGo repo and copy them into "ovr_sdk_mobile_1.23/VrEmulators/FreeType/"
- download "gli 0.8.2.0" https://github.com/g-truc/gli/releases and copy the gli folder into "ovr_sdk_mobile_1.23/VrEmulators/"
- download "glm 0.9.8.0" https://github.com/g-truc/glm/releases/tag/0.9.8.0 and copy the glm folder into "ovr_sdk_mobile_1.23/VrEmulators/"
- open the VirtualBoyGo project in Android Studio
