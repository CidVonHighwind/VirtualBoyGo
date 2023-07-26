/************************************************************************************

Filename    :   Vr360VideoPlayer.h
Content     :   Trivial game style scene viewer VR sample
Created     :   September 8, 2013
Authors     :   John Carmack

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

************************************************************************************/

#pragma once

#include <vector>
#include <string>
#include <memory>

#include "Appl.h"
#include "OVR_FileSys.h"
#include "Model/SceneView.h"
#include "Render/SurfaceRender.h"
#include "Render/SurfaceTexture.h"

class Vr360VideoPlayer : public OVRFW::ovrAppl {
   public:
    Vr360VideoPlayer(
        const int32_t mainThreadTid,
        const int32_t renderThreadTid,
        const int cpuLevel,
        const int gpuLevel)
        : ovrAppl(mainThreadTid, renderThreadTid, cpuLevel, gpuLevel, true /* useMutliView */),
          FreeMove(false),
          SceneModel(nullptr),
          FileSys(nullptr),
          RenderState(RENDER_STATE_LOADING),
          MovieTexture(nullptr),
          CurrentMovieWidth(0),
          CurrentMovieHeight(480),
          IsPaused(true),
          WasPausedOnUnMount(false) {}
    virtual ~Vr360VideoPlayer() {}

    // Called when the application initializes.
    // Must return true if the application initializes successfully.
    virtual bool AppInit(const OVRFW::ovrAppContext* context) override;
    // Called when the application shuts down
    virtual void AppShutdown(const OVRFW::ovrAppContext* context) override;
    // Called when the application is resumed by the system.
    virtual void AppResumed(const OVRFW::ovrAppContext* contet) override;
    // Called when the application is paused by the system.
    virtual void AppPaused(const OVRFW::ovrAppContext* context) override;
    // Called once per frame when the VR session is active.
    virtual OVRFW::ovrApplFrameOut AppFrame(const OVRFW::ovrApplFrameIn& in) override;
    // Called once per frame to allow the application to render eye buffers.
    virtual void AppRenderFrame(const OVRFW::ovrApplFrameIn& in, OVRFW::ovrRendererOutput& out)
        override;
    // Called once per eye each frame for default renderer
    virtual void
    AppRenderEye(const OVRFW::ovrApplFrameIn& in, OVRFW::ovrRendererOutput& out, int eye) override;
    // Called when app loses focus
    virtual void AppLostFocus() override {
        PauseVideo();
    }
    // Called when app re-gains focus
    virtual void AppGainedFocus() override {
        ResumeVideo();
    }
    /// interop with JAVA mediaPlayer

    // Called from JAVA layer
    void SetVideoSize(int width, int height);
    void GetScreenSurface(jobject& surfaceTexture);
    void VideoEnded();

    // Call to JAVA layer
    void StartVideo();
    void PauseVideo();
    void ResumeVideo();

   private:
    OVR::Matrix4f TexmForVideo(const int eye) const;

   private:
    bool FreeMove;
    OVRFW::OvrSceneView Scene;
    OVRFW::ovrSurfaceRender SurfaceRender;
    std::unique_ptr<OVRFW::ModelFile> SceneModel;
    OVRFW::ovrFileSys* FileSys;
    OVRFW::ovrAppl::ovrRenderState RenderState;

    // panorama vars
    OVRFW::GlProgram PanoramaProgram;
    OVRFW::ovrSurfaceDef GlobeSurfaceDef;
    OVRFW::GlTexture GlobeProgramTexture;

    // video vars
    std::string VideoName;
    std::unique_ptr<OVRFW::SurfaceTexture> MovieTexture;
    int CurrentMovieWidth; // set to 0 when a new movie is started, don't render until non-0
    int CurrentMovieHeight;
    bool IsPaused;
    bool WasPausedOnUnMount;
};
