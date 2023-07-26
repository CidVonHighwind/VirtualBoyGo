/************************************************************************************

Filename    :   Vr360VideoPlayer.cpp
Content     :   Trivial game style scene viewer VR sample
Created     :   September 8, 2013
Authors     :   John Carmack

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/

#include "Vr360VideoPlayer.h"
#include "PackageFiles.h" // for // LOGCPUTIME

#include <sys/stat.h>
#include <errno.h>

#include <algorithm>

#include <dirent.h>
#include "unistd.h"

#include "Misc/Log.h"
#include "Render/Egl.h"
#include "Render/GlGeometry.h"

#include "JniUtils.h"
#include "OVR_Std.h"
#include "OVR_Math.h"

using OVR::Bounds3f;
using OVR::Matrix4f;
using OVR::Quatf;
using OVR::Vector2f;
using OVR::Vector3d;
using OVR::Vector3f;
using OVR::Vector4f;

//=============================================================================
//                             Shaders
//=============================================================================

static const char* ImageExternalDirectives = R"glsl(
#extension GL_OES_EGL_image_external : enable
#extension GL_OES_EGL_image_external_essl3 : enable
)glsl";

static const char* PanoramaVertexShaderSrc = R"glsl(
uniform highp mat4 Texm[NUM_VIEWS];
attribute vec4 Position;
attribute vec2 TexCoord;
varying  highp vec2 oTexCoord;
void main()
{
   gl_Position = TransformVertex( Position );
   oTexCoord = vec2( Texm[VIEW_ID] * vec4( TexCoord, 0, 1 ) );
}
)glsl";

static const char* PanoramaFragmentShaderSrc = R"glsl(
uniform samplerExternalOES Texture0;
uniform lowp vec4 UniformColor;
varying highp vec2 oTexCoord;
void main()
{
	gl_FragColor = UniformColor * texture2D( Texture0, oTexCoord );
}
)glsl";

//=============================================================================
//                             Vr360VideoPlayer
//=============================================================================

bool Vr360VideoPlayer::AppInit(const OVRFW::ovrAppContext* appContext) {
    ALOGV("AppInit - enter");

    /// Init File System / APK services
    const ovrJava& ctx = *(reinterpret_cast<const ovrJava*>(appContext->ContextForVrApi()));
    FileSys = OVRFW::ovrFileSys::Create(ctx);
    if (nullptr == FileSys) {
        ALOG("AppInit - could not create FileSys");
        return false;
    }

    /// Init Rendering
    SurfaceRender.Init();
    /// Build movie Texture
    MovieTexture = std::unique_ptr<OVRFW::SurfaceTexture>(new OVRFW::SurfaceTexture(ctx.Env));
    /// Start with an empty model Scene
    SceneModel = std::unique_ptr<OVRFW::ModelFile>(new OVRFW::ModelFile("Void"));
    Scene.SetWorldModel(*(SceneModel.get()));
    Scene.SetFreeMove(FreeMove);
    Vector3f seat = {3.0f, 0.0f, 3.6f};
    Scene.SetFootPos(seat);
    /// Build Shader and required uniforms
    static OVRFW::ovrProgramParm PanoramaUniformParms[] = {
        {"Texm", OVRFW::ovrProgramParmType::FLOAT_MATRIX4},
        {"UniformColor", OVRFW::ovrProgramParmType::FLOAT_VECTOR4},
        {"Texture0", OVRFW::ovrProgramParmType::TEXTURE_SAMPLED},
    };
    PanoramaProgram = OVRFW::GlProgram::Build(
        NULL,
        PanoramaVertexShaderSrc,
        ImageExternalDirectives,
        PanoramaFragmentShaderSrc,
        PanoramaUniformParms,
        sizeof(PanoramaUniformParms) / sizeof(OVRFW::ovrProgramParm));
    /// Use a globe mesh for the video surface
    GlobeSurfaceDef.surfaceName = "Globe";
    GlobeSurfaceDef.geo = OVRFW::BuildGlobe();
    GlobeSurfaceDef.graphicsCommand.Program = PanoramaProgram;
    GlobeSurfaceDef.graphicsCommand.GpuState.depthEnable = false;
    GlobeSurfaceDef.graphicsCommand.GpuState.cullEnable = false;

    /// Load movie file
    std::string movieFileName = "Oculus/360Videos/[Samsung] 360 video demo.mp4";
    /// Check launch intents for file override
    std::string intentFromPackage, intentJSON, intentURI;
    GetIntentStrings(intentFromPackage, intentJSON, intentURI);
    ALOGV("AppInit - intent = `%s`", intentURI.c_str());
    /// Find movie file
    if (intentURI.length() > 0) {
        movieFileName = intentURI;
    }
    std::vector<std::string> SearchPaths;
    OVRFW::ovrFileSys::PushBackSearchPathIfValid(
        ctx, OVRFW::EST_SECONDARY_EXTERNAL_STORAGE, OVRFW::EFT_ROOT, "", SearchPaths);
    OVRFW::ovrFileSys::PushBackSearchPathIfValid(
        ctx, OVRFW::EST_PRIMARY_EXTERNAL_STORAGE, OVRFW::EFT_ROOT, "", SearchPaths);
    for (const auto& searchPath : SearchPaths) {
        std::string moviePath = std::string("file://") + searchPath + movieFileName;
        ALOGV("AppInit - testing for movie at '%s'", moviePath.c_str());
        if (FileSys && FileSys->FileExists(moviePath.c_str())) {
            ALOGV("AppInit - FOUND movie at '%s'", moviePath.c_str());
            movieFileName = searchPath + movieFileName;
            break;
        }
    }
    ALOGV("AppInit - loading movie = `%s`", movieFileName.c_str());
    /// Save this
    VideoName = movieFileName;

    /// For movie players, best to set the display to 60fps if available
    {
        // Query supported frame rates
        int numSupportedRates =
            vrapi_GetSystemPropertyInt(&ctx, VRAPI_SYS_PROP_NUM_SUPPORTED_DISPLAY_REFRESH_RATES);
        std::vector<float> refreshRates(numSupportedRates);
        int numValues = vrapi_GetSystemPropertyFloatArray(
            &ctx,
            VRAPI_SYS_PROP_SUPPORTED_DISPLAY_REFRESH_RATES,
            (float*)refreshRates.data(),
            numSupportedRates);
        if (numValues > 0) {
            // See if we have one close to 60fps
            for (const float& rate : refreshRates) {
                ALOGV("AppInit - available refresh rate of %.2f Hz", rate);
                if (fabs(rate - 60.0f) < 0.001f) {
                    ALOGV("AppInit - setting refresh rate to %.2f Hz", rate);
                    vrapi_SetDisplayRefreshRate(GetSessionObject(), rate);
                    break;
                }
            }
        }
    }

    /// Start movie on Java side
    StartVideo();

    /// All done
    ALOGV("AppInit - exit");
    return true;
}

void Vr360VideoPlayer::AppShutdown(const OVRFW::ovrAppContext*) {
    ALOGV("AppShutdown - enter");
    RenderState = RENDER_STATE_ENDING;

    GlobeSurfaceDef.geo.Free();
    OVRFW::GlProgram::Free(PanoramaProgram);

    OVRFW::ovrFileSys::Destroy(FileSys);
    SurfaceRender.Shutdown();

    ALOGV("AppShutdown - exit");
}

void Vr360VideoPlayer::AppResumed(const OVRFW::ovrAppContext* /* context */) {
    ALOGV("Vr360VideoPlayer::AppResumed");
    RenderState = RENDER_STATE_RUNNING;
    ResumeVideo();
}

void Vr360VideoPlayer::AppPaused(const OVRFW::ovrAppContext* /* context */) {
    ALOGV("Vr360VideoPlayer::AppPaused");
    if (RenderState == RENDER_STATE_RUNNING) {
        PauseVideo();
    }
}

OVRFW::ovrApplFrameOut Vr360VideoPlayer::AppFrame(const OVRFW::ovrApplFrameIn& vrFrame) {
    /// Set free move mode if the left trigger is on
    FreeMove = (vrFrame.AllButtons & ovrButton_Trigger) != 0;
    Scene.SetFreeMove(FreeMove);

    // Player movement
    Scene.Frame(vrFrame);

    /// Simple Play/Pause toggle
    if (vrFrame.Clicked(ovrButton_A) || vrFrame.Clicked(ovrButton_Trigger)) {
        if (IsPaused) {
            ResumeVideo();
        } else {
            PauseVideo();
        }
    }

    // Check for mount/unmount
    if (vrFrame.HeadsetUnMounted()) {
        WasPausedOnUnMount = IsPaused;
        PauseVideo();
    }
    if (vrFrame.HeadsetMounted() && false == WasPausedOnUnMount) {
        ResumeVideo();
    }

    return OVRFW::ovrApplFrameOut();
}

void Vr360VideoPlayer::AppRenderFrame(
    const OVRFW::ovrApplFrameIn& in,
    OVRFW::ovrRendererOutput& out) {
    switch (RenderState) {
        case RENDER_STATE_LOADING: {
            DefaultRenderFrame_Loading(in, out);
        } break;
        case RENDER_STATE_RUNNING: {
            const bool drawScreen = (MovieTexture != nullptr) && (CurrentMovieWidth > 0);

            Scene.GetFrameMatrices(
                SuggestedEyeFovDegreesX, SuggestedEyeFovDegreesY, out.FrameMatrices);
            Scene.GenerateFrameSurfaceList(out.FrameMatrices, out.Surfaces);

            if (drawScreen) {
                // Latch the latest movie frame to the texture.
                MovieTexture->Update();

                GlobeProgramTexture =
                    OVRFW::GlTexture(MovieTexture->GetTextureId(), GL_TEXTURE_EXTERNAL_OES, 0, 0);
                Vector4f GlobeProgramColor = Vector4f(1.0f);
                OVR::Matrix4f GlobeProgramMatrices[2];
                GlobeProgramMatrices[0] = TexmForVideo(0);
                GlobeProgramMatrices[1] = TexmForVideo(1);

                GlobeSurfaceDef.graphicsCommand.Program = PanoramaProgram;
                GlobeSurfaceDef.graphicsCommand.UniformData[0].Data = &GlobeProgramMatrices[0];
                GlobeSurfaceDef.graphicsCommand.UniformData[0].Count = 2;
                GlobeSurfaceDef.graphicsCommand.UniformData[1].Data = &GlobeProgramColor;
                GlobeSurfaceDef.graphicsCommand.UniformData[2].Data = &GlobeProgramTexture;

                out.Surfaces.push_back(OVRFW::ovrDrawSurface(&GlobeSurfaceDef));
            }

            DefaultRenderFrame_Running(in, out);
        } break;
        case RENDER_STATE_ENDING: {
            DefaultRenderFrame_Ending(in, out);
        } break;
    }
}

void Vr360VideoPlayer::AppRenderEye(
    const OVRFW::ovrApplFrameIn& in,
    OVRFW::ovrRendererOutput& out,
    int eye) {
    // Render the surfaces returned by Frame.
    SurfaceRender.RenderSurfaceList(
        out.Surfaces,
        out.FrameMatrices.EyeView[0], // always use 0 as it assumes an array
        out.FrameMatrices.EyeProjection[0], // always use 0 as it assumes an array
        eye);
}

void Vr360VideoPlayer::SetVideoSize(int width, int height) {
    ALOG("Vr360VideoPlayer::SetVideoSize width=%i height=%i ", width, height);
    CurrentMovieWidth = width;
    CurrentMovieHeight = height;
}

void Vr360VideoPlayer::GetScreenSurface(jobject& surfaceTexture) {
    ALOG("Vr360VideoPlayer::GetScreenSurface");
    surfaceTexture = (jobject)MovieTexture->GetJavaObject();
}

void Vr360VideoPlayer::VideoEnded() {
    ALOG("Vr360VideoPlayer::VideoEnded");
    /// re-start
    StartVideo();
}

Matrix4f Vr360VideoPlayer::TexmForVideo(const int eye) const {
    if (strstr(VideoName.c_str(), "_TB.mp4")) { // top / bottom stereo panorama
        return eye ? Matrix4f(
                         1.0f,
                         0.0f,
                         0.0f,
                         0.0f,
                         0.0f,
                         0.5f,
                         0.0f,
                         0.5f,
                         0.0f,
                         0.0f,
                         1.0f,
                         0.0f,
                         0.0f,
                         0.0f,
                         0.0f,
                         1.0f)
                   : Matrix4f(
                         1.0f,
                         0.0f,
                         0.0f,
                         0.0f,
                         0.0f,
                         0.5f,
                         0.0f,
                         0.0f,
                         0.0f,
                         0.0f,
                         1.0f,
                         0.0f,
                         0.0f,
                         0.0f,
                         0.0f,
                         1.0f);
    }
    if (strstr(VideoName.c_str(), "_BT.mp4")) { // top / bottom stereo panorama
        return (!eye) ? Matrix4f(
                            1.0f,
                            0.0f,
                            0.0f,
                            0.0f,
                            0.0f,
                            0.5f,
                            0.0f,
                            0.5f,
                            0.0f,
                            0.0f,
                            1.0f,
                            0.0f,
                            0.0f,
                            0.0f,
                            0.0f,
                            1.0f)
                      : Matrix4f(
                            1.0f,
                            0.0f,
                            0.0f,
                            0.0f,
                            0.0f,
                            0.5f,
                            0.0f,
                            0.0f,
                            0.0f,
                            0.0f,
                            1.0f,
                            0.0f,
                            0.0f,
                            0.0f,
                            0.0f,
                            1.0f);
    }
    if (strstr(VideoName.c_str(), "_LR.mp4")) { // left / right stereo panorama
        return eye ? Matrix4f(
                         0.5f,
                         0.0f,
                         0.0f,
                         0.0f,
                         0.0f,
                         1.0f,
                         0.0f,
                         0.0f,
                         0.0f,
                         0.0f,
                         1.0f,
                         0.0f,
                         0.0f,
                         0.0f,
                         0.0f,
                         1.0f)
                   : Matrix4f(
                         0.5f,
                         0.0f,
                         0.0f,
                         0.5f,
                         0.0f,
                         1.0f,
                         0.0f,
                         0.0f,
                         0.0f,
                         0.0f,
                         1.0f,
                         0.0f,
                         0.0f,
                         0.0f,
                         0.0f,
                         1.0f);
    }
    if (strstr(VideoName.c_str(), "_RL.mp4")) { // left / right stereo panorama
        return (!eye) ? Matrix4f(
                            0.5f,
                            0.0f,
                            0.0f,
                            0.0f,
                            0.0f,
                            1.0f,
                            0.0f,
                            0.0f,
                            0.0f,
                            0.0f,
                            1.0f,
                            0.0f,
                            0.0f,
                            0.0f,
                            0.0f,
                            1.0f)
                      : Matrix4f(
                            0.5f,
                            0.0f,
                            0.0f,
                            0.5f,
                            0.0f,
                            1.0f,
                            0.0f,
                            0.0f,
                            0.0f,
                            0.0f,
                            1.0f,
                            0.0f,
                            0.0f,
                            0.0f,
                            0.0f,
                            1.0f);
    }

    // default to top / bottom stereo
    if (CurrentMovieWidth == CurrentMovieHeight) { // top / bottom stereo panorama
        return eye ? Matrix4f(
                         1.0f,
                         0.0f,
                         0.0f,
                         0.0f,
                         0.0f,
                         0.5f,
                         0.0f,
                         0.5f,
                         0.0f,
                         0.0f,
                         1.0f,
                         0.0f,
                         0.0f,
                         0.0f,
                         0.0f,
                         1.0f)
                   : Matrix4f(
                         1.0f,
                         0.0f,
                         0.0f,
                         0.0f,
                         0.0f,
                         0.5f,
                         0.0f,
                         0.0f,
                         0.0f,
                         0.0f,
                         1.0f,
                         0.0f,
                         0.0f,
                         0.0f,
                         0.0f,
                         1.0f);

        // We may want to support swapping top/bottom
    }
    return Matrix4f::Identity();
}

void Vr360VideoPlayer::StartVideo() {
    const ovrJava& ctx = *(reinterpret_cast<const ovrJava*>(GetContext()->ContextForVrApi()));
    JNIEnv* env;
    ctx.Vm->AttachCurrentThread(&env, 0);
    jobject me = ctx.ActivityObject;
    jclass acl = env->GetObjectClass(me); // class pointer of NativeActivity
    jmethodID startMovieMethodId =
        env->GetMethodID(acl, "startMovieFromNative", "(Ljava/lang/String;)V");
    jstring jstrMovieName = env->NewStringUTF(VideoName.c_str());
    env->CallVoidMethod(ctx.ActivityObject, startMovieMethodId, jstrMovieName);
    env->DeleteLocalRef(jstrMovieName);
    IsPaused = false;
}

void Vr360VideoPlayer::PauseVideo() {
    const ovrJava& ctx = *(reinterpret_cast<const ovrJava*>(GetContext()->ContextForVrApi()));
    JNIEnv* env;
    ctx.Vm->AttachCurrentThread(&env, 0);
    jobject me = ctx.ActivityObject;
    jclass acl = env->GetObjectClass(me); // class pointer of NativeActivity
    jmethodID pauseMovieMethodId = env->GetMethodID(acl, "pauseMovie", "()V");
    env->CallVoidMethod(ctx.ActivityObject, pauseMovieMethodId);
    IsPaused = true;
}

void Vr360VideoPlayer::ResumeVideo() {
    const ovrJava& ctx = *(reinterpret_cast<const ovrJava*>(GetContext()->ContextForVrApi()));
    JNIEnv* env;
    ctx.Vm->AttachCurrentThread(&env, 0);
    jobject me = ctx.ActivityObject;
    jclass acl = env->GetObjectClass(me); // class pointer of NativeActivity
    jmethodID resumeMovieMethodId = env->GetMethodID(acl, "resumeMovie", "()V");
    env->CallVoidMethod(ctx.ActivityObject, resumeMovieMethodId);
    IsPaused = false;
}
