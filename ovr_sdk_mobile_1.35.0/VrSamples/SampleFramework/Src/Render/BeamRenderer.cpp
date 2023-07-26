/************************************************************************************

Filename    :   BeamRenderer.cpp
Content     :   Class that manages and renders view-oriented beams.
Created     :   October 23; 2015
Authors     :   Jonathan E. Wright

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/

#include "BeamRenderer.h"
#include "TextureAtlas.h"

#include "Misc/Log.h"

using OVR::Matrix4f;
using OVR::Posef;
using OVR::Quatf;
using OVR::Vector2f;
using OVR::Vector3f;
using OVR::Vector4f;

inline Vector3f GetViewMatrixPosition(Matrix4f const& m) {
    return m.Inverted().GetTranslation();
}

namespace OVRFW {

static const char* DebugLineVertexSrc = R"glsl(
attribute vec4 Position;
attribute vec4 VertexColor;
attribute vec2 TexCoord;

varying lowp vec4 outColor;
varying highp vec2 oTexCoord;

void main()
{
	gl_Position = TransformVertex( Position );
	oTexCoord = TexCoord;
   	outColor = VertexColor;
}
)glsl";

static const char* DebugLineFragmentSrc = R"glsl(
uniform sampler2D Texture0;

varying lowp vec4 outColor;
varying highp vec2 oTexCoord;

void main()
{
	gl_FragColor = outColor * texture2D( Texture0, oTexCoord );
}
)glsl";

float ovrBeamRenderer::LIFETIME_INFINITE = FLT_MAX;

//==============================
// ovrBeamRenderer::ovrBeamRenderer
ovrBeamRenderer::ovrBeamRenderer() : MaxBeams(0) {}

//==============================
// ovrBeamRenderer::ovrBeamRenderer
ovrBeamRenderer::~ovrBeamRenderer() {
    Shutdown();
}

//==============================
// ovrBeamRenderer::Init
void ovrBeamRenderer::Init(const int maxBeams, const bool depthTest) {
    Shutdown();

    MaxBeams = maxBeams;

    if (LineProgram.VertexShader == 0 || LineProgram.FragmentShader == 0) {
        OVRFW::ovrProgramParm uniformParms[] = {
            /// Vertex
            /// Fragment
            {"Texture0", OVRFW::ovrProgramParmType::TEXTURE_SAMPLED},
        };
        const int uniformCount = sizeof(uniformParms) / sizeof(OVRFW::ovrProgramParm);
        LineProgram = OVRFW::GlProgram::Build(
            DebugLineVertexSrc, DebugLineFragmentSrc, uniformParms, uniformCount);
    }

    const int numVerts = maxBeams * 4;

    VertexAttribs attr;
    attr.position.resize(numVerts);
    attr.uv0.resize(numVerts);
    attr.color.resize(numVerts);

    // the indices will never change once we've set them up; we just won't necessarily
    // use all of the index buffer to render.
    std::vector<TriangleIndex> indices;
    indices.resize(MaxBeams * 6);

    for (int i = 0; i < MaxBeams; i++) {
        indices[i * 6 + 0] = static_cast<TriangleIndex>(i * 4 + 0);
        indices[i * 6 + 1] = static_cast<TriangleIndex>(i * 4 + 1);
        indices[i * 6 + 2] = static_cast<TriangleIndex>(i * 4 + 3);
        indices[i * 6 + 3] = static_cast<TriangleIndex>(i * 4 + 0);
        indices[i * 6 + 4] = static_cast<TriangleIndex>(i * 4 + 3);
        indices[i * 6 + 5] = static_cast<TriangleIndex>(i * 4 + 2);
    }

    Surf.surfaceName = "beams";
    Surf.geo.Create(attr, indices);
    Surf.geo.primitiveType = GL_TRIANGLES;
    Surf.geo.indexCount = 0;

    ovrGraphicsCommand& gc = Surf.graphicsCommand;

    gc.GpuState.depthEnable = gc.GpuState.depthMaskEnable = depthTest;
    gc.GpuState.blendEnable = ovrGpuState::BLEND_ENABLE;
    gc.GpuState.blendSrc = GL_SRC_ALPHA;
    gc.GpuState.blendDst = GL_ONE;
    gc.Program = LineProgram;

#if defined(OVR_OS_ANDROID)
    gc.GpuState.lineWidth = 2.0f;
#else
    gc.GpuState.lineWidth = 1.0f; // deprecated in OpenGL 4.2
#endif
}

//==============================
// ovrBeamRenderer::Shutdown
void ovrBeamRenderer::Shutdown() {
    Surf.geo.Free();
    OVRFW::GlProgram::Free(LineProgram);

    MaxBeams = 0;
    FreeBeams.resize(0);
    ActiveBeams.resize(0);
    BeamInfos.resize(0);
}

//==============================
// ovrBeamRenderer::AddBeam
ovrBeamRenderer::handle_t ovrBeamRenderer::AddBeam(
    const OVRFW::ovrApplFrameIn& frame,
    const ovrTextureAtlas& atlas,
    const int atlasIndex,
    const float width,
    const Vector3f& startPos,
    const Vector3f& endPos,
    const Vector4f& initialColor,
    const float lifeTime) {
    handle_t handle;

    // ALOG( "ovrBeamRenderer::AddDebugLine" );
    if (FreeBeams.size() > 0) {
        handle = FreeBeams[static_cast<int>(FreeBeams.size()) - 1];
        FreeBeams.pop_back();
    } else {
        handle = handle_t(static_cast<uint16_t>(BeamInfos.size()));
        if (handle.Get() >= MaxBeams || handle.Get() >= MAX_BEAMS) {
            return handle_t();
        }
        BeamInfos.push_back(ovrBeamInfo());
    }

    assert(handle.IsValid());
    assert(handle.Get() < static_cast<int>(BeamInfos.size()));
    assert(handle.Get() < MAX_BEAMS);

    ActiveBeams.push_back(handle);

    UpdateBeamInternal(
        frame, handle, atlas, atlasIndex, width, startPos, endPos, initialColor, lifeTime);

    return (lifeTime == LIFETIME_INFINITE) ? handle : handle_t();
}

//==============================
// ovrBeamRenderer::UpdateBeam
void ovrBeamRenderer::UpdateBeam(
    const OVRFW::ovrApplFrameIn& frame,
    const handle_t handle,
    const ovrTextureAtlas& atlas,
    const int atlasIndex,
    const float width,
    const Vector3f& startPos,
    const Vector3f& endPos,
    const Vector4f& initialColor) {
    assert(BeamInfos[handle.Get()].Handle.IsValid());
    UpdateBeamInternal(
        frame, handle, atlas, atlasIndex, width, startPos, endPos, initialColor, LIFETIME_INFINITE);
}

void ovrBeamRenderer::RemoveBeam(const handle_t handle) {
    if (!handle.IsValid() || handle.Get() >= BeamInfos.size()) {
        return;
    }
    BeamInfos[handle.Get()].StartTime = -1.0;
    BeamInfos[handle.Get()].LifeTime = -1.0f;
}

//==============================
// ovrBeamRenderer::UpdateBeamInternal
void ovrBeamRenderer::UpdateBeamInternal(
    const OVRFW::ovrApplFrameIn& frame,
    const handle_t handle,
    const ovrTextureAtlas& atlas,
    const int atlasIndex,
    const float width,
    const Vector3f& startPos,
    const Vector3f& endPos,
    const Vector4f& initialColor,
    float const lifeTime) {
    if (!handle.IsValid()) {
        assert(handle.IsValid());
        return;
    }

    ovrBeamInfo& beam = BeamInfos[handle.Get()];

    beam.Handle = handle;
    beam.StartTime = frame.PredictedDisplayTime;
    beam.LifeTime = lifeTime;
    beam.Width = width;
    beam.AtlasIndex = static_cast<uint16_t>(atlasIndex);
    beam.StartPos = startPos;
    beam.EndPos = endPos;
    beam.InitialColor = initialColor;
    const ovrTextureAtlas::ovrSpriteDef& sd = atlas.GetSpriteDef(atlasIndex);
    beam.TexCoords[0] = sd.uvMins; // min tex coords
    beam.TexCoords[1] = sd.uvMaxs; // max tex coords
}

//==============================
// ovrBeamRenderer::Frame
void ovrBeamRenderer::Frame(
    const OVRFW::ovrApplFrameIn& frame,
    const Matrix4f& centerViewMatrix,
    const ovrTextureAtlas& atlas) {
    Surf.geo.indexCount = 0;

    Surf.graphicsCommand.Textures[0] = atlas.GetTexture();
    Surf.graphicsCommand.BindUniformTextures();

    VertexAttribs attr;
    attr.position.resize(ActiveBeams.size() * 4);
    attr.color.resize(ActiveBeams.size() * 4);
    attr.uv0.resize(ActiveBeams.size() * 4);

    const Vector3f viewPos = GetViewMatrixPosition(centerViewMatrix);

    int quadIndex = 0;
    for (int i = 0; i < static_cast<int>(ActiveBeams.size()); ++i) {
        const handle_t beamHandle = ActiveBeams[i];
        if (!beamHandle.IsValid()) {
            continue;
        }

        const ovrBeamInfo& cur = BeamInfos[beamHandle.Get()];
        double const timeAlive = frame.PredictedDisplayTime - cur.StartTime;
        if (timeAlive > cur.LifeTime) {
            ALOG("Free beamIndex %i", (int)beamHandle.Get());
            BeamInfos[beamHandle.Get()].Handle = handle_t();
            FreeBeams.push_back(beamHandle);
            ActiveBeams[i] = ActiveBeams.back();
            ActiveBeams.pop_back();
            i--;
            continue;
        }

        const Vector3f beamCenter = (cur.EndPos - cur.StartPos) * 0.5f;
        const Vector3f beamDir = beamCenter.Normalized();
        const Vector3f viewToCenter = (beamCenter - viewPos).Normalized();
        const Vector3f cross = beamDir.Cross(viewToCenter).Normalized() * cur.Width * 0.5f;

        const float t = static_cast<float>(frame.PredictedDisplayTime - cur.StartTime);

        const Vector4f color = EaseFunctions[cur.EaseFunc](cur.InitialColor, t / cur.LifeTime);
        const Vector2f uvOfs(0.0f);

        attr.position[quadIndex * 4 + 0] = cur.StartPos + cross;
        attr.position[quadIndex * 4 + 1] = cur.StartPos - cross;
        attr.position[quadIndex * 4 + 2] = cur.EndPos + cross;
        attr.position[quadIndex * 4 + 3] = cur.EndPos - cross;
        attr.color[quadIndex * 4 + 0] = color;
        attr.color[quadIndex * 4 + 1] = color;
        attr.color[quadIndex * 4 + 2] = color;
        attr.color[quadIndex * 4 + 3] = color;
        attr.uv0[quadIndex * 4 + 0] = Vector2f(cur.TexCoords[0].x, cur.TexCoords[0].y) + uvOfs;
        attr.uv0[quadIndex * 4 + 1] = Vector2f(cur.TexCoords[1].x, cur.TexCoords[0].y) + uvOfs;
        attr.uv0[quadIndex * 4 + 2] = Vector2f(cur.TexCoords[0].x, cur.TexCoords[1].y) + uvOfs;
        attr.uv0[quadIndex * 4 + 3] = Vector2f(cur.TexCoords[1].x, cur.TexCoords[1].y) + uvOfs;

        quadIndex++;
    }

    // Surf.graphicsCommand.GpuState.polygonMode = GL_LINE;
    Surf.graphicsCommand.GpuState.cullEnable = false;
    Surf.geo.indexCount = quadIndex * 6;
    Surf.geo.Update(attr);
}

//==============================
// ovrBeamRenderer::RenderEyeView
void ovrBeamRenderer::RenderEyeView(
    const Matrix4f& /*viewMatrix*/,
    const Matrix4f& /*projMatrix*/,
    std::vector<ovrDrawSurface>& surfaceList) {
    if (Surf.geo.indexCount > 0) {
        surfaceList.push_back(ovrDrawSurface(ModelMatrix, &Surf));
    }
}

//==============================
// ovrBeamRenderer::SetPose
void ovrBeamRenderer::SetPose(const Posef& pose) {
    ModelMatrix = Matrix4f(pose);
}

} // namespace OVRFW
