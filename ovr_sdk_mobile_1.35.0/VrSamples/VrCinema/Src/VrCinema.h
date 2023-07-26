/************************************************************************************

Filename    :   VrCinema.h
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

class VrCinema : public OVRFW::ovrAppl {
   public:
    VrCinema(
        const int32_t mainThreadTid,
        const int32_t renderThreadTid,
        const int cpuLevel,
        const int gpuLevel)
        : ovrAppl(mainThreadTid, renderThreadTid, cpuLevel, gpuLevel, true /* useMutliView */),
          UseSrgb(false),
          FreeMove(false),
          SceneModel(nullptr),
          SceneScreenBounds(),
          SceneScreenSurface(nullptr),
          FileSys(nullptr),
          RenderState(RENDER_STATE_LOADING),
          MovieTexture(nullptr),
          FrameUpdateNeeded(false),
          CurrentMovieWidth(0),
          CurrentMovieHeight(480),
          MovieTextureWidth(0),
          MovieTextureHeight(0),
          ScreenVignetteTexture(0),
          CurrentMipMappedMovieTexture(0),
          MipMappedMovieTextureSwapChain(nullptr),
          MipMappedMovieTextureSwapChainLength(0),
          MipMappedMovieFBOs(nullptr),
          BufferSize(),
          IsPaused(true),
          WasPausedOnUnMount(false) {}
    virtual ~VrCinema() {}

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
    void CheckForbufferResize();
    GLuint BuildScreenVignetteTexture(const int horizontalTile) const;
    OVR::Matrix4f BoundsScreenMatrix(const OVR::Bounds3f& bounds, const float movieAspect) const;

   private:
    bool UseSrgb;
    bool FreeMove;
    OVRFW::OvrSceneView Scene;
    OVRFW::ovrSurfaceRender SurfaceRender;
    OVRFW::ModelFile* SceneModel;
    OVR::Bounds3f SceneScreenBounds;
    OVRFW::ovrSurfaceDef* SceneScreenSurface; // override this to the movie texture
    OVRFW::ovrFileSys* FileSys;
    OVRFW::ovrAppl::ovrRenderState RenderState;

    // Render the external image texture to a conventional texture to allow
    // mipmap generation.
    OVRFW::GlProgram CopyMovieProgram;
    OVRFW::GlProgram SceneProgramBlack;
    OVRFW::GlProgram SceneProgramStaticDynamic;
    OVRFW::GlProgram SceneProgramAdditive;
    OVRFW::GlProgram ProgSingleTexture;
    OVRFW::ModelGlPrograms DynamicPrograms;

    OVRFW::SurfaceTexture* MovieTexture;
    long long MovieTextureTimestamp;
    bool FrameUpdateNeeded;

    // Set when MediaPlayer knows what the stream size is.
    // current is the aspect size, texture may be twice as wide or high for 3D content.
    int CurrentMovieWidth; // set to 0 when a new movie is started, don't render until non-0
    int CurrentMovieHeight;
    int MovieTextureWidth;
    int MovieTextureHeight;

    // Used to explicitly clear a hole in alpha.
    OVRFW::ovrSurfaceDef FadedScreenMaskSquareDef;

    // We can't directly create a mip map on the OES_external_texture, so
    // it needs to be copied to a conventional texture.
    // It must be triple buffered for use as a TimeWarp overlay plane.
    OVRFW::GlGeometry UnitSquare; // -1 to 1
    GLuint ScreenVignetteTexture;
    int CurrentMipMappedMovieTexture; // 0 - 2
    ovrTextureSwapChain* MipMappedMovieTextureSwapChain;
    int MipMappedMovieTextureSwapChainLength;
    GLuint* MipMappedMovieFBOs;
    OVR::Vector2i BufferSize; // rebuild if != MovieTextureWidth / Height
    bool IsPaused;
    bool WasPausedOnUnMount;
    std::string VideoName;

    OVR::Vector4f LightsColor;
};
