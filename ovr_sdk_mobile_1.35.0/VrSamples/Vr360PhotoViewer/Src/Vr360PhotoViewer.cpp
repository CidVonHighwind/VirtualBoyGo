/************************************************************************************

Filename    :   Vr360PhotoViewer.cpp
Content     :   Trivial game style scene viewer VR sample
Created     :   September 8, 2013
Authors     :   John Carmack

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/

#include "Vr360PhotoViewer.h"
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
#include "OVR_BinaryFile2.h"

#include "stb_image.h"

using OVR::Bounds3f;
using OVR::Matrix4f;
using OVR::Quatf;
using OVR::Vector2f;
using OVR::Vector3d;
using OVR::Vector3f;
using OVR::Vector4f;

//=============================================================================
//                             Texture helpers
//=============================================================================

inline int AbsInt(const int x) {
    const int mask = x >> (sizeof(int) * 8 - 1);
    return (x + mask) ^ mask;
}

inline int ClampInt(const int x, const int min, const int max) {
    return min + ((AbsInt(x - min) - AbsInt(x - min - max) + max) >> 1);
}

// "A Standard Default Color Space for the Internet - sRGB, Version 1.10"
// Michael Stokes, Matthew Anderson, Srinivasan Chandrasekar, Ricardo Motta
// November 5, 1996
static float SRGBToLinear(float c) {
    const float a = 0.055f;
    if (c <= 0.04045f) {
        return c * (1.0f / 12.92f);
    } else {
        return powf(((c + a) * (1.0f / (1.0f + a))), 2.4f);
    }
}

static float LinearToSRGB(float c) {
    const float a = 0.055f;
    if (c <= 0.0031308f) {
        return c * 12.92f;
    } else {
        return (1.0f + a) * powf(c, (1.0f / 2.4f)) - a;
    }
}

unsigned char*
QuarterImageSize(const unsigned char* src, const int width, const int height, const bool srgb) {
    float table[256];
    if (srgb) {
        for (int i = 0; i < 256; i++) {
            table[i] = SRGBToLinear(i * (1.0f / 255.0f));
        }
    }

    const int newWidth = std::max<int>(1, width >> 1);
    const int newHeight = std::max<int>(1, height >> 1);
    unsigned char* out = (unsigned char*)malloc(newWidth * newHeight * 4);
    unsigned char* out_p = out;
    for (int y = 0; y < newHeight; y++) {
        const unsigned char* in_p = src + y * 2 * width * 4;
        for (int x = 0; x < newWidth; x++) {
            for (int i = 0; i < 4; i++) {
                if (srgb) {
                    const float linear =
                        (table[in_p[i]] + table[in_p[4 + i]] + table[in_p[width * 4 + i]] +
                         table[in_p[width * 4 + 4 + i]]) *
                        0.25f;
                    const float gamma = LinearToSRGB(linear);
                    out_p[i] = (unsigned char)ClampInt((int)(gamma * 255.0f + 0.5f), 0, 255);
                } else {
                    out_p[i] =
                        (in_p[i] + in_p[4 + i] + in_p[width * 4 + i] + in_p[width * 4 + 4 + i]) >>
                        2;
                }
            }
            out_p += 4;
            in_p += 8;
        }
    }
    return out;
}

Matrix4f CubeMatrixForViewMatrix(const Matrix4f& viewMatrix) {
    Matrix4f m = viewMatrix;
    // clear translation
    for (int i = 0; i < 3; i++) {
        m.M[i][3] = 0.0f;
    }
    return m.Inverted();
}

//=============================================================================
//                             Shaders
//=============================================================================

static const char* TextureMvpVertexShaderSrc = R"glsl(
attribute vec4 Position;
attribute vec2 TexCoord;
uniform mediump vec4 UniformColor;
varying  lowp vec4 oColor;
varying highp vec2 oTexCoord;
void main()
{
   gl_Position = TransformVertex( Position );
   oTexCoord = TexCoord;
   oColor = UniformColor;
}
)glsl";

static const char* TexturedMvpFragmentShaderSrc = R"glsl(
uniform sampler2D Texture0;
varying highp vec2 oTexCoord;
varying lowp vec4 oColor;
void main()
{
   gl_FragColor = oColor * texture2D( Texture0, oTexCoord );
}
)glsl";

//=============================================================================
//                             DoubleBufferedTextureData
//=============================================================================

Vr360PhotoViewer::DoubleBufferedTextureData::DoubleBufferedTextureData() : CurrentIndex(0) {
    TextureSwapChain[0] = nullptr;
    TextureSwapChain[1] = nullptr;
    Width[0] = 0;
    Width[1] = 0;
    Height[0] = 0;
    Height[1] = 0;
}

Vr360PhotoViewer::DoubleBufferedTextureData::~DoubleBufferedTextureData() {
    Clear();
}

ovrTextureSwapChain* Vr360PhotoViewer::DoubleBufferedTextureData::GetRenderTextureSwapChain()
    const {
    return TextureSwapChain[CurrentIndex ^ 1];
}

ovrTextureSwapChain* Vr360PhotoViewer::DoubleBufferedTextureData::GetLoadTextureSwapChain() const {
    return TextureSwapChain[CurrentIndex];
}

void Vr360PhotoViewer::DoubleBufferedTextureData::SetLoadTextureSwapChain(
    ovrTextureSwapChain* chain) {
    if (nullptr != TextureSwapChain[CurrentIndex]) {
        vrapi_DestroyTextureSwapChain(TextureSwapChain[CurrentIndex]);
    }
    TextureSwapChain[CurrentIndex] = chain;
}

void Vr360PhotoViewer::DoubleBufferedTextureData::Swap() {
    CurrentIndex ^= 1;
}

void Vr360PhotoViewer::DoubleBufferedTextureData::Clear() {
    if (nullptr != TextureSwapChain[0]) {
        vrapi_DestroyTextureSwapChain(TextureSwapChain[0]);
        TextureSwapChain[0] = nullptr;
    }
    if (nullptr != TextureSwapChain[1]) {
        vrapi_DestroyTextureSwapChain(TextureSwapChain[1]);
        TextureSwapChain[1] = nullptr;
    }
}

void Vr360PhotoViewer::DoubleBufferedTextureData::SetSize(const int width, const int height) {
    Width[CurrentIndex] = width;
    Height[CurrentIndex] = height;
}

bool Vr360PhotoViewer::DoubleBufferedTextureData::SameSize(const int width, const int height)
    const {
    return (Width[CurrentIndex] == width && Height[CurrentIndex] == height);
}

//=============================================================================
//                             Vr360PhotoViewer
//=============================================================================

bool Vr360PhotoViewer::AppInit(const OVRFW::ovrAppContext* appContext) {
    ALOGV("AppInit - enter");

    /// Init File System / APK services
    const ovrJava& ctx = *(reinterpret_cast<const ovrJava*>(appContext->ContextForVrApi()));
    FileSys = OVRFW::ovrFileSys::Create(ctx);
    if (nullptr == FileSys) {
        ALOG("AppInit - could not create FileSys");
        return false;
    }

    UseSrgb = true;

    /// Init Rendering
    SurfaceRender.Init();
    /// Start with an empty model Scene
    SceneModel = std::unique_ptr<OVRFW::ModelFile>(new OVRFW::ModelFile("Void"));
    Scene.SetWorldModel(*(SceneModel.get()));
    Scene.SetFreeMove(FreeMove);
    Vector3f seat = {3.0f, 0.0f, 3.6f};
    Scene.SetFootPos(seat);
    /// Build Shader and required uniforms
    static OVRFW::ovrProgramParm uniformParms[] = {
        {"UniformColor", OVRFW::ovrProgramParmType::FLOAT_VECTOR4},
        {"Texture0", OVRFW::ovrProgramParmType::TEXTURE_SAMPLED},
    };
    TexturedMvpProgram = OVRFW::GlProgram::Build(
        TextureMvpVertexShaderSrc,
        TexturedMvpFragmentShaderSrc,
        uniformParms,
        sizeof(uniformParms) / sizeof(OVRFW::ovrProgramParm));

    /// Use a globe mesh for the video surface
    GlobeSurfaceDef.surfaceName = "Globe";
    GlobeSurfaceDef.geo = OVRFW::BuildGlobe();
    GlobeSurfaceDef.graphicsCommand.Program = TexturedMvpProgram;
    GlobeSurfaceDef.graphicsCommand.GpuState.depthEnable = false;
    GlobeSurfaceDef.graphicsCommand.GpuState.cullEnable = false;

    /// Load photo file - single pano example
    /// std::string photoFileName = "Oculus/360Photos/placeholderPhoto.jpg";
    /// Load photo file - cube map example
    std::string photoFileName = "Oculus/360Photos/CubePano_nz.jpg";

    /// Check launch intents for file override
    std::string intentFromPackage, intentJSON, intentURI;
    GetIntentStrings(intentFromPackage, intentJSON, intentURI);
    ALOGV("AppInit - intent = `%s`", intentURI.c_str());
    /// Find photo file
    if (intentURI.length() > 0) {
        photoFileName = intentURI;
    }
    ALOGV("AppInit - loading photo = `%s`", photoFileName.c_str());
    /// Save this
    PhotoFileName = photoFileName;

    /// All done
    ALOGV("AppInit - exit");
    return true;
}

void Vr360PhotoViewer::AppShutdown(const OVRFW::ovrAppContext*) {
    ALOGV("AppShutdown - enter");
    RenderState = RENDER_STATE_ENDING;

    BackgroundPanoTexData.Clear();
    BackgroundCubeTexData.Clear();

    GlobeSurfaceDef.geo.Free();
    OVRFW::GlProgram::Free(TexturedMvpProgram);

    OVRFW::ovrFileSys::Destroy(FileSys);
    SurfaceRender.Shutdown();

    ALOGV("AppShutdown - exit");
}

void Vr360PhotoViewer::AppResumed(const OVRFW::ovrAppContext* /* context */) {
    ALOGV("Vr360PhotoViewer::AppResumed");
    RenderState = RENDER_STATE_RUNNING;

    // Load the file when we enter VR mode so that we ensure to have a EGL context
    ReloadPhoto();
}

void Vr360PhotoViewer::AppPaused(const OVRFW::ovrAppContext* /* context */) {
    ALOGV("Vr360PhotoViewer::AppPaused");
}

OVRFW::ovrApplFrameOut Vr360PhotoViewer::AppFrame(const OVRFW::ovrApplFrameIn& vrFrame) {
    /// Set free move mode if the left trigger is on
    FreeMove = (vrFrame.AllButtons & ovrButton_Trigger) != 0;
    Scene.SetFreeMove(FreeMove);

    // Player movement
    Scene.Frame(vrFrame);

    return OVRFW::ovrApplFrameOut();
}

void Vr360PhotoViewer::AppRenderFrame(
    const OVRFW::ovrApplFrameIn& in,
    OVRFW::ovrRendererOutput& out) {
    switch (RenderState) {
        case RENDER_STATE_LOADING: {
            DefaultRenderFrame_Loading(in, out);
        } break;
        case RENDER_STATE_RUNNING: {
            Scene.GetFrameMatrices(
                SuggestedEyeFovDegreesX, SuggestedEyeFovDegreesY, out.FrameMatrices);
            Scene.GenerateFrameSurfaceList(out.FrameMatrices, out.Surfaces);

            float fadeColor = 1.0f;

            if (CurrentPanoIsCubeMap) {
                /// composite the cubemap directly into a layer
                ovrLayerProjection2& overlayLayer = out.Layers[out.NumLayers++].Projection;
                overlayLayer = vrapi_DefaultLayerProjection2();
                overlayLayer.Header.Flags |= VRAPI_FRAME_LAYER_FLAG_CHROMATIC_ABERRATION_CORRECTION;
                overlayLayer.Header.Flags |=
                    (UseSrgb ? 0 : VRAPI_FRAME_LAYER_FLAG_INHIBIT_SRGB_FRAMEBUFFER);
                overlayLayer.Header.SrcBlend = VRAPI_FRAME_LAYER_BLEND_ONE;
                overlayLayer.Header.DstBlend = VRAPI_FRAME_LAYER_BLEND_ZERO;
                overlayLayer.Header.ColorScale = {fadeColor, fadeColor, fadeColor, fadeColor};
                overlayLayer.HeadPose = in.Tracking.HeadPose;
                for (int eye = 0; eye < VRAPI_FRAME_LAYER_EYE_MAX; eye++) {
                    overlayLayer.Textures[eye].ColorSwapChain =
                        BackgroundCubeTexData.GetRenderTextureSwapChain();
                    overlayLayer.Textures[eye].SwapChainIndex = 0;
                    overlayLayer.Textures[eye].TexCoordsFromTanAngles =
                        CubeMatrixForViewMatrix(out.FrameMatrices.CenterView);
                }

                /// additional content on top of cubemap
                ovrLayerProjection2& worldLayer = out.Layers[out.NumLayers++].Projection;
                worldLayer = vrapi_DefaultLayerProjection2();
                worldLayer.Header.Flags |=
                    (UseSrgb ? 0 : VRAPI_FRAME_LAYER_FLAG_INHIBIT_SRGB_FRAMEBUFFER);
                worldLayer.Header.SrcBlend = VRAPI_FRAME_LAYER_BLEND_SRC_ALPHA;
                worldLayer.Header.DstBlend = VRAPI_FRAME_LAYER_BLEND_ONE_MINUS_SRC_ALPHA;
                worldLayer.HeadPose = in.Tracking.HeadPose;
                for (int eye = 0; eye < VRAPI_FRAME_LAYER_EYE_MAX; ++eye) {
                    ovrFramebuffer* framebuffer = GetFrameBuffer(eye);
                    worldLayer.Textures[eye].ColorSwapChain = framebuffer->ColorTextureSwapChain;
                    worldLayer.Textures[eye].SwapChainIndex = framebuffer->TextureSwapChainIndex;
                    worldLayer.Textures[eye].TexCoordsFromTanAngles =
                        ovrMatrix4f_TanAngleMatrixFromProjection(
                            (ovrMatrix4f*)&out.FrameMatrices.EyeProjection[eye]);
                }

                /// render images for each eye
                for (int eye = 0; eye < GetNumFramebuffers(); ++eye) {
                    ovrFramebuffer* framebuffer = GetFrameBuffer(eye);
                    ovrFramebuffer_SetCurrent(framebuffer);

                    AppEyeGLStateSetup(in, framebuffer, eye);
                    AppRenderEye(in, out, eye);

                    ovrFramebuffer_Resolve(framebuffer);
                    ovrFramebuffer_Advance(framebuffer);
                }
                ovrFramebuffer_SetNone();
            } else {
                /// regular layer with a 360 texture map
                DoubleBufferedTextureData& db = BackgroundPanoTexData;
                ovrTextureSwapChain* chain = db.GetRenderTextureSwapChain();
                GlobeProgramTexture = OVRFW::GlTexture(
                    vrapi_GetTextureSwapChainHandle(chain, 0), GL_TEXTURE_2D, 0, 0);
                Vector4f GlobeProgramColor = Vector4f(1.0f);
                GlobeSurfaceDef.graphicsCommand.Program = TexturedMvpProgram;
                GlobeSurfaceDef.graphicsCommand.UniformData[0].Data = &GlobeProgramColor;
                GlobeSurfaceDef.graphicsCommand.UniformData[1].Data = &GlobeProgramTexture;
                out.Surfaces.push_back(OVRFW::ovrDrawSurface(&GlobeSurfaceDef));

                DefaultRenderFrame_Running(in, out);
            }
        } break;
        case RENDER_STATE_ENDING: {
            DefaultRenderFrame_Ending(in, out);
        } break;
    }
}

void Vr360PhotoViewer::AppEyeGLStateSetup(
    const OVRFW::ovrApplFrameIn& /* in */,
    const ovrFramebuffer* fb,
    int /* eye */) {
    GL(glEnable(GL_SCISSOR_TEST));
    GL(glDepthMask(GL_TRUE));
    GL(glEnable(GL_DEPTH_TEST));
    GL(glDepthFunc(GL_LEQUAL));
    GL(glEnable(GL_CULL_FACE));
    GL(glViewport(0, 0, fb->Width, fb->Height));
    GL(glScissor(0, 0, fb->Width, fb->Height));
    // Clear the eye buffers to 0 alpha so the overlay plane shows through.
    Vector4f clearColor = CurrentPanoIsCubeMap ? Vector4f(0.0f, 0.0f, 0.0f, 0.0f)
                                               : Vector4f(0.125f, 0.0f, 0.125f, 1.0f);
    GL(glClearColor(clearColor.x, clearColor.y, clearColor.z, clearColor.w));
    GL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
}

void Vr360PhotoViewer::AppRenderEye(
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

void Vr360PhotoViewer::LoadFile(const std::string& fileName) {
    /// Parse file based on filename
    char const* suffix = strstr(fileName.c_str(), "_nz.jpg");
    if (suffix != nullptr) {
        /// cube map using one file per side
        std::vector<std::vector<uint8_t>> mbfs(6);
        const char* const cubeSuffix[6] = {
            "_px.jpg", "_nx.jpg", "_py.jpg", "_ny.jpg", "_pz.jpg", "_nz.jpg"};

        char filenameWithoutSuffix[1024];
        int suffixStart = int(suffix - fileName.c_str());
        strcpy(filenameWithoutSuffix, fileName.c_str());
        filenameWithoutSuffix[suffixStart] = '\0';

        /// read in
        int side = 0;
        for (; side < 6; side++) {
            char sideFilename[1024];
            strcpy(sideFilename, filenameWithoutSuffix);
            strcat(sideFilename, cubeSuffix[side]);

            /// Try reading from file system first
            mbfs[side] = OVRFW::MemBufferFile(sideFilename);
            if (mbfs[side].size() <= 0 || mbfs[side].data() == nullptr) {
                /// Try the app package if that failed
                if (!OVRFW::ovr_ReadFileFromApplicationPackage(sideFilename, mbfs[side])) {
                    ALOG(
                        "Vr360PhotoViewer::LoadFile FAILED to LOAD '%s' - skipping ", sideFilename);
                    return;
                }
            }
            ALOG("Vr360PhotoViewer::LoadFile loaded '%s'", sideFilename);
        }
        std::vector<std::unique_ptr<uint8_t>> data(6);
        int resolutionX = 0;
        int resolutionY = 0;
        int buffCount = 0;
        for (; buffCount < side; buffCount++) {
            int x = 0;
            int y = 0;
            int comp = 0;
            const stbi_uc* buffer = static_cast<stbi_uc*>(mbfs[buffCount].data());
            int bufferSize = static_cast<int>(mbfs[buffCount].size());
            data[buffCount] = std::unique_ptr<uint8_t>(
                stbi_load_from_memory(buffer, bufferSize, &x, &y, &comp, 4));
            if (data[buffCount] == nullptr) {
                ALOG("Vr360PhotoViewer::LoadFile failed to load from buffer.");
                return;
            }
            if (buffCount == 0) {
                /// save the cube size
                resolutionX = x;
                resolutionY = y;
            } else {
                /// ensure the sizes are consistent
                assert(resolutionX == x);
                assert(resolutionY == y);
            }
        }
        /// this is a cube
        assert(resolutionX == resolutionY);
        /// create image resource
        LoadRgbaCubeMap(resolutionX, data, UseSrgb);
        /// Wait for the upload to complete.
        glFinish();
        CurrentPanoIsCubeMap = true;
        ALOG("Vr360PhotoViewer::LoadFile loaded '%s' successfully as a CUBEMAP.", fileName.c_str());
        return;
    } else {
        /// non-cube map single file - read file
        std::vector<uint8_t> mbf = OVRFW::MemBufferFile(fileName.c_str());
        if (mbf.size() <= 0 || mbf.data() == nullptr) {
            if (!OVRFW::ovr_ReadFileFromApplicationPackage(fileName.c_str(), mbf)) {
                ALOG("Vr360PhotoViewer::LoadFile failed to load from buffer.");
                return;
            }
        }

        /// parse file
        int x = 0;
        int y = 0;
        int comp = 0;
        std::unique_ptr<uint8_t> data = std::unique_ptr<uint8_t>(stbi_load_from_memory(
            static_cast<stbi_uc*>(mbf.data()), static_cast<int>(mbf.size()), &x, &y, &comp, 4));
        /// create image resource
        LoadRgbaTexture(data, x, y, UseSrgb);
        // Wait for the upload to complete.
        glFinish();
        data = nullptr;
        CurrentPanoIsCubeMap = false;
        ALOG("Vr360PhotoViewer::LoadFile loaded '%s' successfully as a PANO.", fileName.c_str());
        return;
    }

    ALOG("Vr360PhotoViewer::LoadFile '%s' FAILED", fileName.c_str());
}

void Vr360PhotoViewer::LoadRgbaCubeMap(
    const int resolution,
    const std::vector<std::unique_ptr<uint8_t>>& rgba,
    const bool useSrgbFormat) {
    GLCheckErrorsWithTitle("enter LoadRgbaCubeMap");

    assert(rgba.size() == 6);

    // Create texture storage once
    ovrTextureSwapChain* chain = BackgroundCubeTexData.GetLoadTextureSwapChain();
    if (chain == nullptr || !BackgroundCubeTexData.SameSize(resolution, resolution)) {
        vrapi_DestroyTextureSwapChain(chain);
        chain = vrapi_CreateTextureSwapChain3(
            VRAPI_TEXTURE_TYPE_CUBE,
            useSrgbFormat ? GL_SRGB8_ALPHA8 : GL_RGBA8,
            resolution,
            resolution,
            OVRFW::ComputeFullMipChainNumLevels(resolution, resolution),
            1);
        BackgroundCubeTexData.SetLoadTextureSwapChain(chain);
        BackgroundCubeTexData.SetSize(resolution, resolution);
        /// let the new chain be the render target
        BackgroundCubeTexData.Swap();
    }
    glBindTexture(GL_TEXTURE_CUBE_MAP, vrapi_GetTextureSwapChainHandle(chain, 0));
    for (int side = 0; side < 6; side++) {
        glTexSubImage2D(
            GL_TEXTURE_CUBE_MAP_POSITIVE_X + side,
            0,
            0,
            0,
            resolution,
            resolution,
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            rgba[side].get());
    }
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

    GLCheckErrorsWithTitle("leave LoadRgbaCubeMap");
}

void Vr360PhotoViewer::LoadRgbaTexture(
    const std::unique_ptr<uint8_t>& data,
    int width,
    int height,
    const bool useSrgbFormat) {
    GLCheckErrorsWithTitle("enter LoadRgbaTexture");

    // Create texture storage once
    ovrTextureSwapChain* chain = BackgroundPanoTexData.GetLoadTextureSwapChain();
    if (chain == NULL || !BackgroundPanoTexData.SameSize(width, height)) {
        vrapi_DestroyTextureSwapChain(chain);
        chain = vrapi_CreateTextureSwapChain3(
            VRAPI_TEXTURE_TYPE_2D,
            useSrgbFormat ? GL_SRGB8_ALPHA8 : GL_RGBA8,
            width,
            height,
            OVRFW::ComputeFullMipChainNumLevels(width, height),
            1);
        BackgroundPanoTexData.SetLoadTextureSwapChain(chain);
        BackgroundPanoTexData.SetSize(width, height);
        /// let the new chain be the render target
        BackgroundPanoTexData.Swap();
    }

    glBindTexture(GL_TEXTURE_2D, vrapi_GetTextureSwapChainHandle(chain, 0));
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, data.get());
    glGenerateMipmap(GL_TEXTURE_2D);
    // Because equirect panos pinch at the poles so much,
    // they would pull in mip maps so deep you would see colors
    // from the opposite half of the pano.  Clamping the level avoids this.
    // A well filtered pano shouldn't have any high frequency texels
    // that alias near the poles.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 2);
    glBindTexture(GL_TEXTURE_2D, 0);

    GLCheckErrorsWithTitle("leave LoadRgbaTexture");
}

void Vr360PhotoViewer::ReloadPhoto() {
    std::string photoFileName = PhotoFileName;

    if (false == HasActiveVRSession()) {
        ALOGV("ReloadPhoto - no active VR session file = `%s`", photoFileName.c_str());
        /// if we get called too early before we initialized our contenxt, eraly out
        return;
    }

    std::vector<std::string> SearchPaths;
    const ovrJava& ctx = *(reinterpret_cast<const ovrJava*>(GetContext()->ContextForVrApi()));
    OVRFW::ovrFileSys::PushBackSearchPathIfValid(
        ctx, OVRFW::EST_SECONDARY_EXTERNAL_STORAGE, OVRFW::EFT_ROOT, "", SearchPaths);
    OVRFW::ovrFileSys::PushBackSearchPathIfValid(
        ctx, OVRFW::EST_PRIMARY_EXTERNAL_STORAGE, OVRFW::EFT_ROOT, "", SearchPaths);
    SearchPaths.push_back("");
    for (const auto& searchPath : SearchPaths) {
        std::string photoPath = std::string("file://") + searchPath + photoFileName;
        ALOGV("AppInit - testing for photo at '%s'", photoPath.c_str());
        if (FileSys && FileSys->FileExists(photoPath.c_str())) {
            ALOGV("AppInit - FOUND photo at '%s'", photoPath.c_str());
            photoFileName = searchPath + photoFileName;
            break;
        }
    }
    ALOGV("ReloadPhoto - loading photo = `%s`", photoFileName.c_str());
    /// Save this
    PhotoFileName = photoFileName;
    LoadFile(PhotoFileName);
}
