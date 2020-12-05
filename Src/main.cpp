/*******************************************************************************

Filename    :   Main.cpp
Content     :   Base project for mobile VR samples
Created     :   February 21, 2018
Authors     :   John Carmack, J.M.P. van Waveren, Jonathan Wright
Language    :   C++

Copyright:	Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*******************************************************************************/
#include "main.h"

#include <cstdint>
#include <cstdio>
#include <stdlib.h> // for exit()

#include <sstream>
#include <OVR_LogUtils.h>

#include <FrontendGo/TextureLoader.h>
#include <FrontendGo/Global.h>
#include <FrontendGo/Audio/OpenSLWrap.h>
#include <FrontendGo/DrawHelper.h>
#include <FrontendGo/FontMaster.h>
#include <FrontendGo/Menu.h>
#include <glm/gtc/matrix_transform.hpp>

extern "C" {

ovrVirtualBoyGo *appPtr = nullptr;

long Java_com_nintendont_virtualboygo_MainActivity_nativeSetAppInterface(JNIEnv *jni, jclass clazz, jobject activity) {
    ALOG("nativeSetAppInterface %p", appPtr);
    return reinterpret_cast<jlong>(appPtr);
}

void Java_com_nintendont_virtualboygo_MainActivity_nativeReloadRoms(JNIEnv *jni, jclass clazz, jlong interfacePtr) {
    ALOG("nativeReloadPhoto interfacePtr=%p appPtr=%p", interfacePtr, appPtr);
    if (appPtr && interfacePtr) {
        appPtr->menuGo.ScanDirectory();
    } else {
        ALOG("nativeReloadPhoto %p NULL ptr", appPtr);
    }
}

} // extern "C"

using namespace OVRFW;

Global ovrVirtualBoyGo::global;

ovrVirtualBoyGo::~ovrVirtualBoyGo() {
    ALOGV("AppDelete");
};

bool ovrVirtualBoyGo::AppInit(const OVRFW::ovrAppContext *appContext) {
    ALOGV("AppInit - enter");

    java = reinterpret_cast<const ovrJava *>(appContext->ContextForVrApi());
    JNIEnv *env;
    java->Vm->AttachCurrentThread(&env, 0);
    jobject me = java->ActivityObject;
    // used to get the battery level
    clsData = env->GetObjectClass(me); // class pointer of NativeActivity

    jmethodID messageMe = env->GetMethodID(clsData, "getInternalStorageDir", "()Ljava/lang/String;");
    jobject result = (jstring) env->CallObjectMethod(me, messageMe);
    const char *storageDir = env->GetStringUTFChars((jstring) result, NULL);

    messageMe = env->GetMethodID(clsData, "getExternalFilesDir", "()Ljava/lang/String;");
    result = (jstring) env->CallObjectMethod(me, messageMe);
    const char *appDir = env->GetStringUTFChars((jstring) result, NULL);

    global.appStoragePath = storageDir;

    global.saveFilePath = appDir;
    global.saveFilePath.append("/settings.config");

    OVR_LOG("got string from java: appdir %s", appDir);
    OVR_LOG("got string from java: storageDir %s", storageDir);

    FileSys = OVRFW::ovrFileSys::Create(*java);

    OVR_LOG_WITH_TAG("OvrApp", "Init");

    /// Init Rendering
    SurfaceRender.Init();
    Scene.SetFreeMove(false);
    OVR::Vector3f seat = {3.0f, 0.0f, 3.6f};
    Scene.SetFootPos(seat);

    OVR_LOG_WITH_TAG("OvrApp", "OpenSLWrap Init");
    openSlWrap.Init();

    glm::mat4 projection = glm::ortho(0.0f, (float) emulator.MENU_WIDTH, 0.0f, (float) emulator.MENU_HEIGHT);

    OVR_LOG_WITH_TAG("OvrApp", "DrawHelper Init");
    drawHelper.Init(projection);

    OVR_LOG_WITH_TAG("OvrApp", "FontManager Init");
    fontManager.Init(projection);

    OVR_LOG_WITH_TAG("OvrApp", "load textures and fonts");
    global.Init(FileSys);
    global.romFolderPath = global.appStoragePath + emulator.romFolderPath;
    OVR_LOG_WITH_TAG("OvrApp", "romFolderPath: %s", global.romFolderPath.data());

    OVR_LOG_WITH_TAG("OvrApp", "Emulator Init");
    emulator.Init(global.appStoragePath, &layerBuilder, &drawHelper, &openSlWrap);

    OVR_LOG_WITH_TAG("OvrApp", "init menu");
    menuGo.Init(&emulator, &layerBuilder, &drawHelper, &fontManager, java, &clsData);

    OVR_LOG_WITH_TAG("OvrApp", "Load Settings");
    menuGo.LoadSettings();

    OVR_LOG_WITH_TAG("OvrApp", "Scan directory");
    menuGo.ScanDirectory();

    OVR_LOG_WITH_TAG("OvrApp", "Setup MenuGo");
    menuGo.SetUpMenu();

    menuGo.CreateScreen();

    ALOGV("AppInit - exit");
    return true;
}

void ovrVirtualBoyGo::InitRefreshRate() {
    initRefreshRate = true;

    // number of supported refreshrates
    int refreshRateCount = vrapi_GetSystemPropertyInt(java, VRAPI_SYS_PROP_NUM_SUPPORTED_DISPLAY_REFRESH_RATES);
    std::vector<float> supportedRefreshRates(refreshRateCount);
    vrapi_GetSystemPropertyFloatArray(java, VRAPI_SYS_PROP_SUPPORTED_DISPLAY_REFRESH_RATES, supportedRefreshRates.data(), refreshRateCount);

    bool setRefreshrate = false;
    float maxRefreshrate = 0;
    // set the refresh rate to the one requested by the emulator if it is available
    for (int i = 0; i < refreshRateCount; ++i) {
        OVR_LOG("Refreshrate: %f", supportedRefreshRates[i]);
        if (supportedRefreshRates[i] > maxRefreshrate) {
            maxRefreshrate = supportedRefreshRates[i];
        }
        if (supportedRefreshRates[i] == emulator.DisplayRefreshRate) {
            setRefreshrate = true;

            if (vrapi_SetDisplayRefreshRate(GetSessionObject(), emulator.DisplayRefreshRate) == ovrSuccess)
                OVR_LOG_WITH_TAG("OvrApp", "Refreshrate set to %f", emulator.DisplayRefreshRate);
            else
                OVR_LOG_WITH_TAG("OvrApp", "Failed to set refreshrate");

            break;
        }
    }

    // use the highest refreshrate if the one the emulator uses does not exists
    if (!setRefreshrate) {
        if (refreshRateCount > 0) {
            if (vrapi_SetDisplayRefreshRate(GetSessionObject(), maxRefreshrate) == ovrSuccess)
                OVR_LOG_WITH_TAG("OvrApp", "Refreshrate set to max %f", maxRefreshrate);
            else
                OVR_LOG_WITH_TAG("OvrApp", "Failed to set refreshrate");
        }
    }
}

void ovrVirtualBoyGo::AppHandleInputShutdownRequest(ovrRendererOutput &out) {
    // this gets called when B or Y is pressed on the touch controllers
}

void ovrVirtualBoyGo::AppShutdown(const OVRFW::ovrAppContext *) {
    ALOGV("AppShutdown - enter");
    OVRFW::ovrFileSys::Destroy(FileSys);
    SurfaceRender.Shutdown();

    fontManager.Free();
    drawHelper.Free();

    emulator.Free();
    menuGo.Free();

    global.Free();

    openSlWrap.Shutdown();
    ALOGV("AppShutdown - exit");
}

void ovrVirtualBoyGo::AppResumed(const OVRFW::ovrAppContext * /* context */) {
    ALOGV("ovrVirtualBoyGo::AppResumed");
}

void ovrVirtualBoyGo::AppPaused(const OVRFW::ovrAppContext * /* context */) {
    ALOGV("ovrVirtualBoyGo::AppPaused");
}

OVRFW::ovrApplFrameOut ovrVirtualBoyGo::AppFrame(const OVRFW::ovrApplFrameIn &vrFrame) {
    Scene.SetFreeMove(false);
    Scene.Frame(vrFrame);

    return OVRFW::ovrApplFrameOut();
}

void ovrVirtualBoyGo::AppRenderFrame(const OVRFW::ovrApplFrameIn &in, OVRFW::ovrRendererOutput &out) {
    if(!initRefreshRate)
        InitRefreshRate();

    Scene.UpdateCenterEye();
    Scene.GetFrameMatrices(SuggestedEyeFovDegreesX, SuggestedEyeFovDegreesY, out.FrameMatrices);
    Scene.GenerateFrameSurfaceList(out.FrameMatrices, out.Surfaces);

    ovrLayerProjection2 &worldLayer = out.Layers[out.NumLayers++].Projection;
    worldLayer = vrapi_DefaultLayerBlackProjection2();
    worldLayer.Header.Flags |= VRAPI_FRAME_LAYER_FLAG_INHIBIT_SRGB_FRAMEBUFFER;
    worldLayer.Header.SrcBlend = VRAPI_FRAME_LAYER_BLEND_SRC_ALPHA;
    worldLayer.Header.DstBlend = VRAPI_FRAME_LAYER_BLEND_ONE_MINUS_SRC_ALPHA;
    worldLayer.HeadPose = in.Tracking.HeadPose;

    for (int eye = 0; eye < VRAPI_FRAME_LAYER_EYE_MAX; ++eye) {
        ovrFramebuffer *framebuffer = GetFrameBuffer(eye);
        worldLayer.Textures[eye].ColorSwapChain = framebuffer->ColorTextureSwapChain;
        worldLayer.Textures[eye].SwapChainIndex = framebuffer->TextureSwapChainIndex;
        worldLayer.Textures[eye].TexCoordsFromTanAngles = ovrMatrix4f_TanAngleMatrixFromProjection((ovrMatrix4f *) &out.FrameMatrices.EyeProjection[eye]);
    }

    menuGo.Update(dynamic_cast<ovrAppl &>(*this), in, out);

    /// render images for each eye
    for (int eye = 0; eye < GetNumFramebuffers(); ++eye) {
        ovrFramebuffer *framebuffer = GetFrameBuffer(eye);
        ovrFramebuffer_SetCurrent(framebuffer);

        AppEyeGLStateSetup(in, framebuffer, eye);
        AppRenderEye(in, out, eye);

        ovrFramebuffer_Resolve(framebuffer);
        ovrFramebuffer_Advance(framebuffer);
    }

    ovrFramebuffer_SetNone();
}

//==============================================================
// android_main
//==============================================================
void android_main(struct android_app *app) {
    appPtr = nullptr;
    std::unique_ptr<ovrVirtualBoyGo> appl = std::unique_ptr<ovrVirtualBoyGo>(new ovrVirtualBoyGo(0, 0, 0, 0));
    appPtr = appl.get();
    appl->Run(app);
    appPtr = nullptr;
}