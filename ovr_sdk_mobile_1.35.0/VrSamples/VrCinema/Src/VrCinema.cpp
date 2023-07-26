/************************************************************************************

Filename    :   VrCinema.cpp
Content     :   Trivial game style scene viewer VR sample
Created     :   September 8, 2013
Authors     :   John Carmack

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/

#include "VrCinema.h"
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

static const char* copyMovieVertexShaderSrc = R"glsl(
attribute vec4 Position;
attribute vec2 TexCoord;
varying  highp vec2 oTexCoord;
void main()
{
   gl_Position = Position;
   oTexCoord = vec2( TexCoord.x, 1.0 - TexCoord.y );// need to flip Y
}
)glsl";

static const char* copyMovieFragmentShaderSource = R"glsl(
uniform samplerExternalOES Texture0;
uniform sampler2D Texture1; // edge vignette
varying highp vec2 oTexCoord;
void main()
{
	gl_FragColor = texture2D( Texture0, oTexCoord ) *  texture2D( Texture1, oTexCoord );
}
)glsl";

static const char* SceneDynamicVertexShaderSrc = R"glsl(
uniform sampler2D Texture2;
uniform lowp vec4 UniformColor;
attribute vec4 Position;
attribute vec2 TexCoord;
varying highp vec2 oTexCoord;
varying lowp vec4 oColor;
void main()
{
   gl_Position = TransformVertex( Position );
   oTexCoord = TexCoord;
   oColor = textureLod(Texture2, vec2( 0.0, 0.0), 16.0 ); // bottom mip of screen texture
   oColor.xyz += vec3( 0.05, 0.05, 0.05 );
   oColor.w = UniformColor.w;
}
)glsl";

static const char* SceneStaticAndDynamicFragmentShaderSrc = R"glsl(
uniform sampler2D Texture0;
uniform sampler2D Texture1;
varying highp vec2 oTexCoord;
varying lowp vec4 oColor;
void main()
{
	gl_FragColor.xyz = oColor.w * texture2D(Texture0, oTexCoord).xyz + (1.0 - oColor.w) * oColor.xyz * texture2D(Texture1, oTexCoord).xyz;
	gl_FragColor.w = 1.0;
}
)glsl";

static const char* SceneStaticVertexShaderSrc = R"glsl(
uniform lowp vec4 UniformColor;
attribute vec4 Position;
attribute vec2 TexCoord;
varying highp vec2 oTexCoord;
varying lowp vec4 oColor;
void main()
{
   gl_Position = TransformVertex( Position );
   oTexCoord = TexCoord;
   oColor = UniformColor;
}
)glsl";

static const char* SceneAdditiveFragmentShaderSrc = R"glsl(
uniform sampler2D Texture0;
varying highp vec2 oTexCoord;
varying lowp vec4 oColor;
void main()
{
	gl_FragColor.xyz = (1.0 - oColor.w) * texture2D(Texture0, oTexCoord).xyz;
	gl_FragColor.w = 1.0;
}
)glsl";

static const char* SceneBlackFragmentShaderSrc = R"glsl(
void main()
{
	gl_FragColor = vec4( 0.0, 0.0, 0.0, 1.0 );
}
)glsl";

static const char* overlayScreenFadeMaskVertexShaderSrc = R"glsl(
attribute vec4 VertexColor;
attribute vec4 Position;
varying  lowp vec4 oColor;
void main()
{
   gl_Position = TransformVertex( Position );
   oColor = vec4( 1.0, 1.0, 1.0, 1.0 - VertexColor.x );
}
)glsl";

static const char* overlayScreenFadeMaskFragmentShaderSrc = R"glsl(
varying lowp vec4	oColor;
void main()
{
	gl_FragColor = oColor;
}
)glsl";

static OVRFW::GlGeometry BuildFadedScreenMask(const float xFraction, const float yFraction) {
    const float posx[] = {-1.001f,
                          -1.0f + xFraction * 0.25f,
                          -1.0f + xFraction,
                          1.0f - xFraction,
                          1.0f - xFraction * 0.25f,
                          1.001f};
    const float posy[] = {-1.001f,
                          -1.0f + yFraction * 0.25f,
                          -1.0f + yFraction,
                          1.0f - yFraction,
                          1.0f - yFraction * 0.25f,
                          1.001f};

    const int vertexCount = 6 * 6;

    OVRFW::VertexAttribs attribs;
    attribs.position.resize(vertexCount);
    attribs.uv0.resize(vertexCount);
    attribs.color.resize(vertexCount);

    for (int y = 0; y < 6; y++) {
        for (int x = 0; x < 6; x++) {
            const int index = y * 6 + x;
            attribs.position[index].x = posx[x];
            attribs.position[index].y = posy[y];
            attribs.position[index].z = 0.0f;
            attribs.uv0[index].x = 0.0f;
            attribs.uv0[index].y = 0.0f;
            // the outer edges will have 0 color
            const float c = (y <= 1 || y >= 4 || x <= 1 || x >= 4) ? 0.0f : 1.0f;
            for (int i = 0; i < 3; i++) {
                attribs.color[index][i] = c;
            }
            attribs.color[index][3] = 1.0f; // solid alpha
        }
    }

    std::vector<OVRFW::TriangleIndex> indices;
    indices.resize(25 * 6);

    // Should we flip the triangulation on the corners?
    int index = 0;
    for (OVRFW::TriangleIndex x = 0; x < 5; x++) {
        for (OVRFW::TriangleIndex y = 0; y < 5; y++) {
            indices[index + 0] = y * 6 + x;
            indices[index + 1] = y * 6 + x + 1;
            indices[index + 2] = (y + 1) * 6 + x;
            indices[index + 3] = (y + 1) * 6 + x;
            indices[index + 4] = y * 6 + x + 1;
            indices[index + 5] = (y + 1) * 6 + x + 1;
            index += 6;
        }
    }

    return OVRFW::GlGeometry(attribs, indices);
}

//=============================================================================
//                             VrCinema
//=============================================================================

bool VrCinema::AppInit(const OVRFW::ovrAppContext* appContext) {
    ALOGV("AppInit - enter");

    /// Init File System / APK services
    const ovrJava& ctx = *(reinterpret_cast<const ovrJava*>(appContext->ContextForVrApi()));
    JNIEnv* env;
    ctx.Vm->AttachCurrentThread(&env, 0);
    FileSys = OVRFW::ovrFileSys::Create(ctx);
    if (nullptr == FileSys) {
        ALOG("AppInit - could not create FileSys");
        return false;
    }

    /// Init Rendering
    SurfaceRender.Init();

    /// Init Shaders
    {
        /// Disable multi-view for this ...
        OVRFW::GlProgram::MultiViewScope(false);

        static OVRFW::ovrProgramParm uniformParms[] = {
            /// Vertex
            /// Fragment
            {"Texture0", OVRFW::ovrProgramParmType::TEXTURE_SAMPLED},
            {"Texture1", OVRFW::ovrProgramParmType::TEXTURE_SAMPLED},
        };
        CopyMovieProgram = OVRFW::GlProgram::Build(
            nullptr,
            copyMovieVertexShaderSrc,
            ImageExternalDirectives,
            copyMovieFragmentShaderSource,
            uniformParms,
            sizeof(uniformParms) / sizeof(OVRFW::ovrProgramParm));
    }

    {
        static OVRFW::ovrProgramParm uniformParms[] = {
            /// Vertex
            {"UniformColor", OVRFW::ovrProgramParmType::FLOAT_VECTOR4},
            /// Fragment
        };
        const int uniformCount = sizeof(uniformParms) / sizeof(OVRFW::ovrProgramParm);
        SceneProgramBlack = OVRFW::GlProgram::Build(
            SceneStaticVertexShaderSrc, SceneBlackFragmentShaderSrc, uniformParms, uniformCount);
    }

    {
        static OVRFW::ovrProgramParm uniformParms[] = {
            /// Vertex
            {"UniformColor", OVRFW::ovrProgramParmType::FLOAT_VECTOR4},
            {"Texture2", OVRFW::ovrProgramParmType::TEXTURE_SAMPLED},
            /// Fragment
            {"Texture0", OVRFW::ovrProgramParmType::TEXTURE_SAMPLED},
            {"Texture1", OVRFW::ovrProgramParmType::TEXTURE_SAMPLED},
        };
        const int uniformCount = sizeof(uniformParms) / sizeof(OVRFW::ovrProgramParm);
        SceneProgramStaticDynamic = OVRFW::GlProgram::Build(
            SceneDynamicVertexShaderSrc,
            SceneStaticAndDynamicFragmentShaderSrc,
            uniformParms,
            uniformCount);
    }

    {
        static OVRFW::ovrProgramParm uniformParms[] = {
            /// Vertex
            {"UniformColor", OVRFW::ovrProgramParmType::FLOAT_VECTOR4},
            /// Fragment
            {"Texture0", OVRFW::ovrProgramParmType::TEXTURE_SAMPLED},
        };
        const int uniformCount = sizeof(uniformParms) / sizeof(OVRFW::ovrProgramParm);
        SceneProgramAdditive = OVRFW::GlProgram::Build(
            SceneStaticVertexShaderSrc, SceneAdditiveFragmentShaderSrc, uniformParms, uniformCount);
    }

    {
        OVRFW::ovrProgramParm uniformParms[] = {
            /// Vertex
            /// Fragment
            {"Texture0", OVRFW::ovrProgramParmType::TEXTURE_SAMPLED},
        };
        const int uniformCount = sizeof(uniformParms) / sizeof(OVRFW::ovrProgramParm);
        ProgSingleTexture = OVRFW::GlProgram::Build(
            OVRFW::SingleTextureVertexShaderSrc,
            OVRFW::SingleTextureFragmentShaderSrc,
            uniformParms,
            uniformCount);
    }

    DynamicPrograms = OVRFW::ModelGlPrograms(&SceneProgramStaticDynamic);
    /// Init screen quad
    UnitSquare = OVRFW::BuildTesselatedQuad(1, 1);
    /// Init screen texture
    ScreenVignetteTexture = BuildScreenVignetteTexture(1);
    /// Build movie Texture
    MovieTexture = new OVRFW::SurfaceTexture(ctx.Env);

    /// Check to see if we can load resources from APK
    void* zipFile = OVRFW::ovr_GetApplicationPackageFile();
    if (nullptr == zipFile) {
        char curPackageCodePath[OVRFW::ovrFileSys::OVR_MAX_PATH_LEN];
        ovr_GetPackageCodePath(
            ctx.Env, ctx.ActivityObject, curPackageCodePath, sizeof(curPackageCodePath));
        OVRFW::ovr_OpenApplicationPackage(curPackageCodePath, nullptr);
        ALOG("curPackageCodePath = '%s' zipFile = %p", curPackageCodePath, zipFile);
        zipFile = nullptr;
    }

    /// Load Theater model
    std::string SceneFile = "assets/home_theater.ovrscene";
    OVRFW::MaterialParms materialParms;
    materialParms.UseSrgbTextureFormats = UseSrgb;
    materialParms.EnableDiffuseAniso = true;
    materialParms.EnableEmissiveLodClamp = false;
    SceneModel =
        LoadModelFileFromApplicationPackage(SceneFile.c_str(), DynamicPrograms, materialParms);
    if (nullptr == SceneModel) {
        ALOG("AppInit - could not create SceneModel");
        return false;
    }
    Scene.SetWorldModel(*SceneModel);
    Scene.SetFreeMove(FreeMove);
    /// Get Screen
    SceneScreenSurface = static_cast<OVRFW::ovrSurfaceDef*>(Scene.FindNamedSurface("screen"));
    if (nullptr == SceneScreenSurface) {
        ALOG("AppInit - could not find SceneScreenSurface");
        return false;
    }
    /// Find first seat
    Vector3f seat = {3.0f, 0.0f, 1.5f};
    ALOGV("AppInit - seat = (%.3f, %.3f, %.3f)", seat.x, seat.y, seat.z);
    Scene.SetFootPos(seat);

    LightsColor = OVR::Vector4f(1.0f, 1.0f, 1.0f, 1.0f);

    /// Override the material on the background scene to allow the model to fade during state
    /// transitions.
    {
        const OVRFW::ModelFile* modelFile = Scene.GetWorldModel()->Definition;
        for (int i = 0; i < static_cast<int>(modelFile->Models.size()); i++) {
            for (int j = 0; j < static_cast<int>(modelFile->Models[i].surfaces.size()); j++) {
                if (&modelFile->Models[i].surfaces[j].surfaceDef == SceneScreenSurface) {
                    continue;
                }

                // FIXME: provide better solution for material overrides
                OVRFW::ovrGraphicsCommand& graphicsCommand =
                    *const_cast<OVRFW::ovrGraphicsCommand*>(
                        &modelFile->Models[i].surfaces[j].surfaceDef.graphicsCommand);

                if (graphicsCommand.GpuState.blendSrc == GL_ONE &&
                    graphicsCommand.GpuState.blendDst == GL_ONE) {
                    // Non-modulated additive material.
                    if (graphicsCommand.Textures[1] != 0) {
                        graphicsCommand.Textures[0] = graphicsCommand.Textures[1];
                        graphicsCommand.Textures[1] = OVRFW::GlTexture();
                    }

                    graphicsCommand.Program = SceneProgramAdditive;
                    graphicsCommand.UniformData[0].Data = &LightsColor;
                    graphicsCommand.UniformData[1].Data = &graphicsCommand.Textures[0];
                } else if (graphicsCommand.Textures[1] != 0) {
                    graphicsCommand.Program = SceneProgramStaticDynamic;
                    graphicsCommand.UniformData[0].Data = &LightsColor;
                    graphicsCommand.UniformData[1].Data = &graphicsCommand.Textures[2];
                    graphicsCommand.UniformData[2].Data = &graphicsCommand.Textures[0];
                    graphicsCommand.UniformData[3].Data = &graphicsCommand.Textures[1];
                } else {
                    // Non-modulated diffuse material.
                    graphicsCommand.Program = ProgSingleTexture;
                    graphicsCommand.UniformData[0].Data = &graphicsCommand.Textures[0];
                }
            }
        }
    }

    /// Get bounds for scene
    SceneScreenBounds = SceneScreenSurface->geo.localBounds;
    /// force to a solid black material that cuts a hole in alpha
    SceneScreenSurface->graphicsCommand.Program = SceneProgramBlack;
    /// Surface for hole
    FadedScreenMaskSquareDef.graphicsCommand.Program = OVRFW::GlProgram::Build(
        overlayScreenFadeMaskVertexShaderSrc, overlayScreenFadeMaskFragmentShaderSrc, NULL, 0);
    FadedScreenMaskSquareDef.surfaceName = "FadedScreenMaskSquare";
    FadedScreenMaskSquareDef.geo = BuildFadedScreenMask(0.0f, 0.0f);
    FadedScreenMaskSquareDef.graphicsCommand.GpuState.depthEnable = false;
    FadedScreenMaskSquareDef.graphicsCommand.GpuState.depthMaskEnable = false;
    FadedScreenMaskSquareDef.graphicsCommand.GpuState.cullEnable = false;
    FadedScreenMaskSquareDef.graphicsCommand.GpuState.colorMaskEnable[0] = false;
    FadedScreenMaskSquareDef.graphicsCommand.GpuState.colorMaskEnable[1] = false;
    FadedScreenMaskSquareDef.graphicsCommand.GpuState.colorMaskEnable[2] = false;
    FadedScreenMaskSquareDef.graphicsCommand.GpuState.colorMaskEnable[3] = true;

    /// Load movie file
    std::string movieFileName = "Oculus/Movies/Trailers/HenryShort.mp4";
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

void VrCinema::AppShutdown(const OVRFW::ovrAppContext*) {
    ALOGV("AppShutdown - enter");
    RenderState = RENDER_STATE_ENDING;

    OVRFW::GlProgram::Free(CopyMovieProgram);
    OVRFW::GlProgram::Free(SceneProgramBlack);
    OVRFW::GlProgram::Free(SceneProgramStaticDynamic);
    OVRFW::GlProgram::Free(SceneProgramAdditive);
    OVRFW::GlProgram::Free(ProgSingleTexture);

    UnitSquare.Free();

    if (ScreenVignetteTexture != 0) {
        glDeleteTextures(1, &ScreenVignetteTexture);
        ScreenVignetteTexture = 0;
    }

    if (MipMappedMovieFBOs != nullptr) {
        glDeleteFramebuffers(MipMappedMovieTextureSwapChainLength, MipMappedMovieFBOs);
        delete[] MipMappedMovieFBOs;
        MipMappedMovieFBOs = nullptr;
    }

    if (MipMappedMovieTextureSwapChain != nullptr) {
        vrapi_DestroyTextureSwapChain(MipMappedMovieTextureSwapChain);
        MipMappedMovieTextureSwapChain = nullptr;
        MipMappedMovieTextureSwapChainLength = 0;
    }

    OVRFW::ovrFileSys::Destroy(FileSys);
    SurfaceRender.Shutdown();

    ALOGV("AppShutdown - exit");
}

void VrCinema::AppResumed(const OVRFW::ovrAppContext* /* context */) {
    ALOGV("VrCinema::AppResumed");
    RenderState = RENDER_STATE_RUNNING;
    ResumeVideo();
}

void VrCinema::AppPaused(const OVRFW::ovrAppContext* /* context */) {
    ALOGV("VrCinema::AppPaused");
    if (RenderState == RENDER_STATE_RUNNING) {
        PauseVideo();
    }
}

OVRFW::ovrApplFrameOut VrCinema::AppFrame(const OVRFW::ovrApplFrameIn& vrFrame) {
    // Set free move mode if the left trigger is on
    FreeMove = vrFrame.LeftRemoteTracked && (vrFrame.LeftRemote.Buttons & ovrButton_Trigger) != 0;
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

void VrCinema::AppRenderFrame(const OVRFW::ovrApplFrameIn& in, OVRFW::ovrRendererOutput& out) {
    switch (RenderState) {
        case RENDER_STATE_LOADING: {
            DefaultRenderFrame_Loading(in, out);
        } break;
        case RENDER_STATE_RUNNING: {
            // latch the latest movie frame to the texture.
            if (MovieTexture != nullptr && CurrentMovieWidth != 0) {
                glActiveTexture(GL_TEXTURE0);
                MovieTexture->Update();
                glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);
                if (MovieTexture->GetNanoTimeStamp() != MovieTextureTimestamp) {
                    MovieTextureTimestamp = MovieTexture->GetNanoTimeStamp();
                    FrameUpdateNeeded = true;
                }
            }

            CheckForbufferResize();

            // build the mip maps
            if (FrameUpdateNeeded && MipMappedMovieTextureSwapChain != NULL) {
                FrameUpdateNeeded = false;
                CurrentMipMappedMovieTexture =
                    (CurrentMipMappedMovieTexture + 1) % MipMappedMovieTextureSwapChainLength;
                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, ScreenVignetteTexture);
                glActiveTexture(GL_TEXTURE0);
                glBindFramebuffer(GL_FRAMEBUFFER, MipMappedMovieFBOs[CurrentMipMappedMovieTexture]);
                glDisable(GL_DEPTH_TEST);
                glDisable(GL_SCISSOR_TEST);
                GL_InvalidateFramebuffer(INV_FBO, true, false);
                glViewport(0, 0, MovieTextureWidth, MovieTextureHeight);
                if (UseSrgb) { // we need this copied without sRGB conversion on the top level
                    glDisable(GL_FRAMEBUFFER_SRGB_EXT);
                }
                if (CurrentMovieWidth > 0) {
                    glBindTexture(GL_TEXTURE_EXTERNAL_OES, MovieTexture->GetTextureId());
                    glUseProgram(CopyMovieProgram.Program);
                    glBindVertexArray(UnitSquare.vertexArrayObject);
                    glDrawElements(
                        UnitSquare.primitiveType,
                        UnitSquare.indexCount,
                        UnitSquare.IndexType,
                        NULL);
                    glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);
                    if (UseSrgb) { // we need this copied without sRGB conversion on the top level
                        glEnable(GL_FRAMEBUFFER_SRGB_EXT);
                    }
                } else {
                    // If the screen is going to be black because of a movie change, don't
                    // leave the last dynamic color visible.
                    glClearColor(0.2f, 0.2f, 0.2f, 0.2f);
                    glClear(GL_COLOR_BUFFER_BIT);
                }
                glBindFramebuffer(GL_FRAMEBUFFER, 0);

                // texture 2 will hold the mip mapped screen
                glActiveTexture(GL_TEXTURE2);
                glBindTexture(
                    GL_TEXTURE_2D,
                    vrapi_GetTextureSwapChainHandle(
                        MipMappedMovieTextureSwapChain, CurrentMipMappedMovieTexture));
                glGenerateMipmap(GL_TEXTURE_2D);
                glBindTexture(GL_TEXTURE_2D, 0);

                GL_Flush();
            }

            // For Dynamic and Static-Dynamic, set the mip mapped movie texture to Texture2
            // so it can be sampled from the vertex program for scene lighting.
            const unsigned int lightingTexId = (MipMappedMovieTextureSwapChain != NULL)
                ? vrapi_GetTextureSwapChainHandle(
                      MipMappedMovieTextureSwapChain, CurrentMipMappedMovieTexture)
                : 0;

            // Override the material on the background scene to allow the model to fade during state
            // transitions.
            {
                const float cinemaLights = 0.4f; /// default lighting
                const OVRFW::ModelFile* modelFile = Scene.GetWorldModel()->Definition;
                LightsColor = OVR::Vector4f(1.0f, 1.0f, 1.0f, cinemaLights);
                for (int i = 0; i < static_cast<int>(modelFile->Models.size()); i++) {
                    for (int j = 0; j < static_cast<int>(modelFile->Models[i].surfaces.size());
                         j++) {
                        if (&modelFile->Models[i].surfaces[j].surfaceDef == SceneScreenSurface) {
                            continue;
                        }

                        // FIXME: provide better solution for material overrides
                        OVRFW::ovrGraphicsCommand& graphicsCommand =
                            *const_cast<OVRFW::ovrGraphicsCommand*>(
                                &modelFile->Models[i].surfaces[j].surfaceDef.graphicsCommand);

                        if (graphicsCommand.Program.Program == SceneProgramStaticDynamic.Program) {
                            // Do not try to apply the scene lighting texture if it is not valid.
                            if (lightingTexId != 0) {
                                graphicsCommand.Textures[2] = OVRFW::GlTexture(lightingTexId, 0, 0);
                                graphicsCommand.UniformData[1].Data = &graphicsCommand.Textures[2];
                            }
                        }
                    }
                }
            }

            Scene.GetFrameMatrices(
                SuggestedEyeFovDegreesX, SuggestedEyeFovDegreesY, out.FrameMatrices);
            Scene.GenerateFrameSurfaceList(out.FrameMatrices, out.Surfaces);

            const bool drawScreen =
                (SceneScreenSurface != NULL) && MovieTexture && (CurrentMovieWidth > 0);

            // Draw the movie screen first and compose the eye buffers on top (with holes in alpha)
            if (drawScreen) {
                Matrix4f texMatrix[2];
                ovrRectf texRect[2];
                for (int eye = 0; eye < 2; eye++) {
                    /// default to un-rotated 1:1 scale
                    texMatrix[eye] = Matrix4f::Identity();
                    texRect[eye] = {0.0f, 0.0f, 1.0f, 1.0f};
                }

                float movieAspectRatio = (CurrentMovieHeight == 0)
                    ? 1.0f
                    : ((float)CurrentMovieWidth / CurrentMovieHeight);
                Matrix4f ScreenMatrix = BoundsScreenMatrix(SceneScreenBounds, movieAspectRatio);

                // Draw the movie texture layer
                {
                    ovrLayerProjection2& overlayLayer = out.Layers[out.NumLayers++].Projection;
                    overlayLayer = vrapi_DefaultLayerProjection2();

                    overlayLayer.Header.SrcBlend = VRAPI_FRAME_LAYER_BLEND_ONE;
                    overlayLayer.Header.DstBlend = VRAPI_FRAME_LAYER_BLEND_ZERO;
                    overlayLayer.Header.Flags |=
                        VRAPI_FRAME_LAYER_FLAG_CHROMATIC_ABERRATION_CORRECTION;
                    overlayLayer.HeadPose = in.Tracking.HeadPose;
                    for (int eye = 0; eye < VRAPI_FRAME_LAYER_EYE_MAX; eye++) {
                        const ovrMatrix4f modelViewMatrix =
                            Scene.GetEyeViewMatrix(eye) * ScreenMatrix;
                        overlayLayer.Textures[eye].ColorSwapChain = MipMappedMovieTextureSwapChain;
                        overlayLayer.Textures[eye].SwapChainIndex = CurrentMipMappedMovieTexture;
                        overlayLayer.Textures[eye].TexCoordsFromTanAngles = texMatrix[eye] *
                            ovrMatrix4f_TanAngleMatrixFromUnitSquare(&modelViewMatrix);
                        overlayLayer.Textures[eye].TextureRect = texRect[eye];
                    }
                }

                // explicitly clear a hole in the eye buffer alpha
                {
                    out.Surfaces.push_back(
                        OVRFW::ovrDrawSurface(ScreenMatrix, &FadedScreenMaskSquareDef));
                }
            }

            ovrLayerProjection2& worldLayer = out.Layers[out.NumLayers++].Projection;
            worldLayer = vrapi_DefaultLayerProjection2();
            worldLayer.Header.SrcBlend =
                (drawScreen) ? VRAPI_FRAME_LAYER_BLEND_SRC_ALPHA : VRAPI_FRAME_LAYER_BLEND_ONE;
            worldLayer.Header.DstBlend = (drawScreen) ? VRAPI_FRAME_LAYER_BLEND_ONE_MINUS_SRC_ALPHA
                                                      : VRAPI_FRAME_LAYER_BLEND_ZERO;
            worldLayer.Header.Flags |= VRAPI_FRAME_LAYER_FLAG_CHROMATIC_ABERRATION_CORRECTION;
            worldLayer.HeadPose = in.Tracking.HeadPose;
            for (int eye = 0; eye < VRAPI_FRAME_LAYER_EYE_MAX; eye++) {
                ovrFramebuffer* framebuffer = GetFrameBuffer(eye);
                worldLayer.Textures[eye].ColorSwapChain = framebuffer->ColorTextureSwapChain;
                worldLayer.Textures[eye].SwapChainIndex = framebuffer->TextureSwapChainIndex;
                worldLayer.Textures[eye].TexCoordsFromTanAngles =
                    ovrMatrix4f_TanAngleMatrixFromProjection(
                        (ovrMatrix4f*)&out.FrameMatrices.EyeProjection[eye]);
            }

            // render images for each eye
            for (int eye = 0; eye < GetNumFramebuffers(); ++eye) {
                ovrFramebuffer* framebuffer = GetFrameBuffer(eye);
                ovrFramebuffer_SetCurrent(framebuffer);

                AppEyeGLStateSetup(in, framebuffer, eye);
                AppRenderEye(in, out, eye);

                ovrFramebuffer_Resolve(framebuffer);
                ovrFramebuffer_Advance(framebuffer);
            }

            ovrFramebuffer_SetNone();
        } break;
        case RENDER_STATE_ENDING: {
            DefaultRenderFrame_Ending(in, out);
        } break;
    }
}

void VrCinema::AppRenderEye(
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

void VrCinema::SetVideoSize(int width, int height) {
    ALOG("VrCinema::SetVideoSize width=%i height=%i ", width, height);
    CurrentMovieWidth = width;
    CurrentMovieHeight = height;
    MovieTextureWidth = width;
    MovieTextureHeight = height;
}

void VrCinema::GetScreenSurface(jobject& surfaceTexture) {
    ALOG("VrCinema::GetScreenSurface");
    surfaceTexture = (jobject)MovieTexture->GetJavaObject();
}

void VrCinema::CheckForbufferResize() {
    if (BufferSize.x == MovieTextureWidth &&
        BufferSize.y == MovieTextureHeight) { // already the correct size
        return;
    }

    if (MovieTextureWidth <= 0 ||
        MovieTextureHeight <= 0) { // don't try to change to an invalid size
        return;
    }
    BufferSize.x = MovieTextureWidth;
    BufferSize.y = MovieTextureHeight;

    // Free previously created frame buffers and texture set.
    if (MipMappedMovieFBOs != nullptr) {
        glDeleteFramebuffers(MipMappedMovieTextureSwapChainLength, MipMappedMovieFBOs);
        delete[] MipMappedMovieFBOs;
        MipMappedMovieFBOs = nullptr;
    }
    if (MipMappedMovieTextureSwapChain != nullptr) {
        vrapi_DestroyTextureSwapChain(MipMappedMovieTextureSwapChain);
        MipMappedMovieTextureSwapChain = nullptr;
        MipMappedMovieTextureSwapChainLength = 0;
    }

    // Create the texture set that we will mip map from the external image.
    MipMappedMovieTextureSwapChain = vrapi_CreateTextureSwapChain3(
        VRAPI_TEXTURE_TYPE_2D,
        UseSrgb ? GL_SRGB8_ALPHA8 : GL_RGBA8,
        MovieTextureWidth,
        MovieTextureHeight,
        OVRFW::ComputeFullMipChainNumLevels(MovieTextureWidth, MovieTextureHeight),
        3);
    MipMappedMovieTextureSwapChainLength =
        vrapi_GetTextureSwapChainLength(MipMappedMovieTextureSwapChain);

    MipMappedMovieFBOs = new GLuint[MipMappedMovieTextureSwapChainLength];
    glGenFramebuffers(MipMappedMovieTextureSwapChainLength, MipMappedMovieFBOs);
    for (int i = 0; i < MipMappedMovieTextureSwapChainLength; i++) {
        glBindFramebuffer(GL_FRAMEBUFFER, MipMappedMovieFBOs[i]);
        glFramebufferTexture2D(
            GL_FRAMEBUFFER,
            GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D,
            vrapi_GetTextureSwapChainHandle(MipMappedMovieTextureSwapChain, i),
            0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
}

GLuint VrCinema::BuildScreenVignetteTexture(const int horizontalTile) const {
    // make it an even border at 16:9 aspect ratio, let it get a little squished at other aspects
    static const int scale = 6;
    static const int width = 16 * scale * horizontalTile;
    static const int height = 9 * scale;
    unsigned char* buffer = new unsigned char[width * height];
    memset(buffer, 255, sizeof(unsigned char) * width * height);
    for (int i = 0; i < width; i++) {
        buffer[i] = 0; // horizontal black top
        buffer[width * height - 1 - i] = 0; // horizontal black bottom
    }
    for (int i = 0; i < height; i++) {
        buffer[i * width] = 0; // vertical black left
        buffer[i * width + width - 1] = 0; // vertical black right
        if (horizontalTile == 2) // vertical black middle
        {
            buffer[i * width + width / 2 - 1] = 0;
            buffer[i * width + width / 2] = 0;
        }
    }
    GLuint texId;
    glGenTextures(1, &texId);
    glBindTexture(GL_TEXTURE_2D, texId);
    glTexImage2D(
        GL_TEXTURE_2D, 0, GL_LUMINANCE, width, height, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, buffer);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    delete[] buffer;
    GLCheckErrorsWithTitle("screenVignette");
    return texId;
}

// Aspect is width / height
Matrix4f VrCinema::BoundsScreenMatrix(const Bounds3f& bounds, const float movieAspect) const {
    const Vector3f size = bounds.b[1] - bounds.b[0];
    const Vector3f center = bounds.b[0] + size * 0.5f;
    const float screenHeight = size.y;
    const float screenWidth = std::max<float>(size.x, size.z);
    float widthScale;
    float heightScale;
    float aspect = (movieAspect == 0.0f) ? 1.0f : movieAspect;
    if (screenWidth / screenHeight > aspect) { // screen is wider than movie, clamp size to height
        heightScale = screenHeight * 0.5f;
        widthScale = heightScale * aspect;
    } else { // screen is taller than movie, clamp size to width
        widthScale = screenWidth * 0.5f;
        heightScale = widthScale / aspect;
    }

    const float rotateAngle = (size.x > size.z) ? 0.0f : MATH_FLOAT_PI * 0.5f;

    return Matrix4f::Translation(center) * Matrix4f::RotationY(rotateAngle) *
        Matrix4f::Scaling(widthScale, heightScale, 1.0f);
}

void VrCinema::VideoEnded() {
    ALOG("VrCinema::VideoEnded");
    /// re-start
    StartVideo();
}

void VrCinema::StartVideo() {
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

void VrCinema::PauseVideo() {
    const ovrJava& ctx = *(reinterpret_cast<const ovrJava*>(GetContext()->ContextForVrApi()));
    JNIEnv* env;
    ctx.Vm->AttachCurrentThread(&env, 0);
    jobject me = ctx.ActivityObject;
    jclass acl = env->GetObjectClass(me); // class pointer of NativeActivity
    jmethodID pauseMovieMethodId = env->GetMethodID(acl, "pauseMovie", "()V");
    env->CallVoidMethod(ctx.ActivityObject, pauseMovieMethodId);
    IsPaused = true;
}

void VrCinema::ResumeVideo() {
    const ovrJava& ctx = *(reinterpret_cast<const ovrJava*>(GetContext()->ContextForVrApi()));
    JNIEnv* env;
    ctx.Vm->AttachCurrentThread(&env, 0);
    jobject me = ctx.ActivityObject;
    jclass acl = env->GetObjectClass(me); // class pointer of NativeActivity
    jmethodID resumeMovieMethodId = env->GetMethodID(acl, "resumeMovie", "()V");
    env->CallVoidMethod(ctx.ActivityObject, resumeMovieMethodId);
    IsPaused = false;
}
