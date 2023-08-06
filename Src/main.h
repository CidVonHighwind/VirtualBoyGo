//
// Created by Patrick on 24.10.2020.
//
#pragma once

#include <FrontendGo/Menu.h>
#include <FrontendGo/Audio/OpenSLWrap.h>
#include "Emulator.h"
#include "Menu.h"

#include "Appl.h"
#include "OVR_FileSys.h"
#include "Model/SceneView.h"
#include "Render/SurfaceRender.h"

//==============================================================================
// Our custom application class
class ovrVirtualBoyGo : public ApplInterface, public OVRFW::ovrAppl {
public:
    static Global global;

    MenuGo menuGo;

    ovrVirtualBoyGo(
            const int32_t mainThreadTid,
            const int32_t renderThreadTid,
            const int cpuLevel,
            const int gpuLevel)
            : ovrAppl(mainThreadTid, renderThreadTid, cpuLevel, gpuLevel, true), FileSys(nullptr) {}

    virtual ~ovrVirtualBoyGo();

    // Called when the application initializes.
    // Must return true if the application initializes successfully.
    virtual bool AppInit(const OVRFW::ovrAppContext *context) override;

    // Called when the application shuts down
    virtual void AppShutdown(const OVRFW::ovrAppContext *context) override;

    // Called when the application is resumed by the system.
    virtual void AppResumed(const OVRFW::ovrAppContext *contet) override;

    // Called when the application is paused by the system.
    virtual void AppPaused(const OVRFW::ovrAppContext *context) override;

    // Called once per frame when the VR session is active.
    virtual OVRFW::ovrApplFrameOut AppFrame(const OVRFW::ovrApplFrameIn &in) override;

    // Called once per frame to allow the application to render eye buffers.
    virtual void AppRenderFrame(const OVRFW::ovrApplFrameIn &in, OVRFW::ovrRendererOutput &out) override;

    virtual void AddLayerCylinder2(ovrLayerCylinder2 &layer) override;

private:
    OVRFW::ovrFileSys *FileSys;
    OVRFW::OvrSceneView Scene;
    OVRFW::ovrSurfaceRender SurfaceRender;

    Emulator emulator;
    LayerBuilder layerBuilder;

    DrawHelper drawHelper;
    FontManager fontManager;

    OpenSLWrapper openSlWrap;

    const ovrJava *java;
    jclass clsData;

    bool initRefreshRate;

    void InitRefreshRate();
};