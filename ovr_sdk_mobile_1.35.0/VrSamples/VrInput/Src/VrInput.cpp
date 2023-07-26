/************************************************************************************

Filename    :   VrInput.cpp
Content     :   Trivial use of the application framework.
Created     :
Authors     :	Jonathan E. Wright, Robert Memmott

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/

#include "VrInput.h"
#include "ControllerGUI.h"

#include "VrApi.h"

#include "GUI/GuiSys.h"
#include "GUI/GazeCursor.h"
#include "Locale/OVR_Locale.h"
#include "Misc/Log.h"

using OVR::Axis_X;
using OVR::Axis_Y;
using OVR::Axis_Z;
using OVR::Bounds3f;
using OVR::Matrix4f;
using OVR::Posef;
using OVR::Quatf;
using OVR::Vector2f;
using OVR::Vector3f;
using OVR::Vector4f;

//#define PERSISTENT_RIBBONS

namespace OVRFW {

#if defined(PERSISTENT_RIBBONS)
static const int NUM_RIBBON_POINTS = 1024;
#else
static const int NUM_RIBBON_POINTS = 32;
#endif

static const Vector4f LASER_COLOR(0.0f, 1.0f, 1.0f, 1.0f);

static const char* PrelitVertexShaderSrc = R"glsl(
attribute highp vec4 Position;
attribute highp vec2 TexCoord;

varying lowp vec3 oEye;
varying highp vec2 oTexCoord;

vec3 transposeMultiply( mat4 m, vec3 v )
{
	return vec3(
		m[0].x * v.x + m[0].y * v.y + m[0].z * v.z,
		m[1].x * v.x + m[1].y * v.y + m[1].z * v.z,
		m[2].x * v.x + m[2].y * v.y + m[2].z * v.z );
}

void main()
{
	gl_Position = TransformVertex( Position );
	oTexCoord = TexCoord;
	vec3 eye = transposeMultiply( sm.ViewMatrix[VIEW_ID], -vec3( sm.ViewMatrix[VIEW_ID][3] ) );
	oEye = eye - vec3( ModelMatrix * Position );
}
)glsl";

static const char* PrelitFragmentShaderSrc = R"glsl(
uniform sampler2D Texture0;

varying highp vec2 oTexCoord;
varying lowp vec3 oEye;

void main()
{
	gl_FragColor = texture2D( Texture0, oTexCoord );
	lowp vec3 eyeScaled = oEye * 5.0f;
	lowp float eyeDistSq = eyeScaled.x * eyeScaled.x + eyeScaled.y * eyeScaled.y + eyeScaled.z * eyeScaled.z;
	gl_FragColor.w = eyeDistSq - 0.5f;
}
)glsl";

static const char* SpecularVertexShaderSrc = R"glsl(
attribute lowp vec4 Position;
attribute lowp vec3 Normal;
attribute lowp vec2 TexCoord;

varying lowp vec3 oEye;
varying lowp vec3 oNormal;
varying lowp vec2 oTexCoord;
varying lowp float oFade;

vec3 multiply( mat4 m, vec3 v )
{
	return vec3(
		m[0].x * v.x + m[1].x * v.y + m[2].x * v.z,
		m[0].y * v.x + m[1].y * v.y + m[2].y * v.z,
		m[0].z * v.x + m[1].z * v.y + m[2].z * v.z );
}

vec3 transposeMultiply( mat4 m, vec3 v )
{
	return vec3(
		m[0].x * v.x + m[0].y * v.y + m[0].z * v.z,
		m[1].x * v.x + m[1].y * v.y + m[1].z * v.z,
		m[2].x * v.x + m[2].y * v.y + m[2].z * v.z );
}

void main()
{
	gl_Position = TransformVertex( Position );
	vec3 eye = transposeMultiply( sm.ViewMatrix[VIEW_ID], -vec3( sm.ViewMatrix[VIEW_ID][3] ) );
	oEye = eye - vec3( ModelMatrix * Position );
	oNormal = multiply( ModelMatrix, Normal );
	oTexCoord = TexCoord;

	// fade controller if it is too close to the camera
	lowp float FadeDistanceScale = 5.0f;
	lowp float FadeDistanceOffset = 0.525f;
	oFade = ( sm.ViewMatrix[0] * ( ModelMatrix * Position ) ).z;
	oFade *= FadeDistanceScale;
	oFade *= oFade;
	oFade -= FadeDistanceOffset;
	oFade = clamp( oFade, 0.0f, 1.0f);
}
)glsl";

static const char* SpecularFragmentWithHighlight = R"glsl(
uniform sampler2D Texture0;
uniform lowp vec3 SpecularLightDirection;
uniform lowp vec3 SpecularLightColor;
uniform lowp vec3 AmbientLightColor;
uniform sampler2D ButtonMaskTexture;
uniform lowp vec4 HighLightMask;
uniform lowp vec3 HighLightColor;

varying lowp vec3 oEye;
varying lowp vec3 oNormal;
varying lowp vec2 oTexCoord;
varying lowp float oFade;

void main()
{
	lowp vec4 hightLightFilter = texture2D( ButtonMaskTexture, oTexCoord );
	lowp float highLight = hightLightFilter.x * HighLightMask.x;
	highLight += hightLightFilter.y * HighLightMask.y;
	highLight += hightLightFilter.z * HighLightMask.z;
	highLight += hightLightFilter.w * HighLightMask.w;

	lowp vec3 eyeDir = normalize( oEye.xyz );
	lowp vec3 Normal = normalize( oNormal );
	lowp vec3 reflectionDir = dot( eyeDir, Normal ) * 2.0 * Normal - eyeDir;
	lowp vec4 diffuse = texture2D( Texture0, oTexCoord );
	lowp vec3 ambientValue = diffuse.xyz * AmbientLightColor;

	lowp float nDotL = max( dot( Normal , SpecularLightDirection ), 0.0 );
	lowp vec3 diffuseValue = diffuse.xyz * SpecularLightColor * nDotL;

	lowp float specularPower = 1.0f - diffuse.a;
	specularPower = specularPower * specularPower;

	lowp vec3 H = normalize( SpecularLightDirection + eyeDir );
	lowp float nDotH = max( dot( Normal, H ), 0.0 );
	lowp float specularIntensity = pow( nDotH, 64.0f * ( specularPower ) ) * specularPower;
	lowp vec3 specularValue = specularIntensity * SpecularLightColor;

	lowp vec3 controllerColor = diffuseValue + ambientValue + specularValue;
	gl_FragColor.xyz = highLight * HighLightColor + (1.0f - highLight) * controllerColor;

	// fade controller if it is too close to the camera
	gl_FragColor.w = oFade;
}
)glsl";

static const char* SpecularFragmentShaderSrc = R"glsl(
uniform sampler2D Texture0;
uniform lowp vec3 SpecularLightDirection;
uniform lowp vec3 SpecularLightColor;
uniform lowp vec3 AmbientLightColor;
uniform sampler2D ButtonMaskTexture;

varying lowp vec3 oEye;
varying lowp vec3 oNormal;
varying lowp vec2 oTexCoord;
varying lowp float oFade;

void main()
{

	lowp vec3 eyeDir = normalize( oEye.xyz );
	lowp vec3 Normal = normalize( oNormal );
	lowp vec3 reflectionDir = dot( eyeDir, Normal ) * 2.0 * Normal - eyeDir;
	lowp vec4 diffuse = texture2D( Texture0, oTexCoord );
	lowp vec3 ambientValue = diffuse.xyz * AmbientLightColor;

	lowp float nDotL = max( dot( Normal , SpecularLightDirection ), 0.0 );
	lowp vec3 diffuseValue = diffuse.xyz * SpecularLightColor * nDotL;

	lowp float specularPower = 1.0f - diffuse.a;
	specularPower = specularPower * specularPower;

	lowp vec3 H = normalize( SpecularLightDirection + eyeDir );
	lowp float nDotH = max( dot( Normal, H ), 0.0 );
	lowp float specularIntensity = pow( nDotH, 64.0f * ( specularPower ) ) * specularPower;
	lowp vec3 specularValue = specularIntensity * SpecularLightColor;

	lowp vec3 controllerColor = diffuseValue + ambientValue + specularValue;
	gl_FragColor.xyz = controllerColor;

	// fade controller if it is too close to the camera
	gl_FragColor.w = oFade;
}
)glsl";

static const char* OculusTouchVertexShaderSrc = R"glsl(
attribute highp vec4 Position;
attribute highp vec3 Normal;
attribute highp vec3 Tangent;
attribute highp vec3 Binormal;
attribute highp vec2 TexCoord;

varying lowp vec3 oEye;
varying lowp vec3 oNormal;
varying lowp vec2 oTexCoord;

vec3 multiply( mat4 m, vec3 v )
{
	return vec3(
		m[0].x * v.x + m[1].x * v.y + m[2].x * v.z,
		m[0].y * v.x + m[1].y * v.y + m[2].y * v.z,
		m[0].z * v.x + m[1].z * v.y + m[2].z * v.z );
}

vec3 transposeMultiply( mat4 m, vec3 v )
{
	return vec3(
		m[0].x * v.x + m[0].y * v.y + m[0].z * v.z,
		m[1].x * v.x + m[1].y * v.y + m[1].z * v.z,
		m[2].x * v.x + m[2].y * v.y + m[2].z * v.z );
}

void main()
{
	gl_Position = TransformVertex( Position );
	vec3 eye = transposeMultiply( sm.ViewMatrix[VIEW_ID], -vec3( sm.ViewMatrix[VIEW_ID][3] ) );
	oEye = eye - vec3( ModelMatrix * Position );
	vec3 iNormal = Normal * 100.0f;
	oNormal = multiply( ModelMatrix, iNormal );
	oTexCoord = TexCoord;
}
)glsl";

static const char* OculusTouchFragmentShaderSrc = R"glsl(
uniform sampler2D Texture0;
uniform lowp vec3 SpecularLightDirection;
uniform lowp vec3 SpecularLightColor;
uniform lowp vec3 AmbientLightColor;

varying lowp vec3 oEye;
varying lowp vec3 oNormal;
varying lowp vec2 oTexCoord;

lowp vec3 multiply( lowp mat3 m, lowp vec3 v )
{
	return vec3(
		m[0].x * v.x + m[1].x * v.y + m[2].x * v.z,
		m[0].y * v.x + m[1].y * v.y + m[2].y * v.z,
		m[0].z * v.x + m[1].z * v.y + m[2].z * v.z );
}

void main()
{
	lowp vec3 eyeDir = normalize( oEye.xyz );
	lowp vec3 Normal = normalize( oNormal );

	lowp vec3 reflectionDir = dot( eyeDir, Normal ) * 2.0 * Normal - eyeDir;
	lowp vec4 diffuse = texture2D( Texture0, oTexCoord );
	lowp vec3 ambientValue = diffuse.xyz * AmbientLightColor;

	lowp float nDotL = max( dot( Normal , SpecularLightDirection ), 0.0 );
	lowp vec3 diffuseValue = diffuse.xyz * SpecularLightColor * nDotL;

	lowp float specularPower = 1.0f - diffuse.a;
	specularPower = specularPower * specularPower;

	lowp vec3 H = normalize( SpecularLightDirection + eyeDir );
	lowp float nDotH = max( dot( Normal, H ), 0.0 );
	lowp float specularIntensity = pow( nDotH, 64.0f * ( specularPower ) ) * specularPower;
	lowp vec3 specularValue = specularIntensity * SpecularLightColor;

	lowp vec3 controllerColor = diffuseValue + ambientValue + specularValue;
	gl_FragColor.xyz = controllerColor;
	gl_FragColor.w = 1.0f;
}
)glsl";

static void
SetObjectColor(OvrGuiSys& guiSys, VRMenu* menu, char const* name, Vector4f const& color) {
    VRMenuObject* obj = menu->ObjectForName(guiSys, name);
    if (obj != nullptr) {
        obj->SetSurfaceColor(0, color);
    }
}

static void SetObjectText(OvrGuiSys& guiSys, VRMenu* menu, char const* name, char const* fmt, ...) {
    VRMenuObject* obj = menu->ObjectForName(guiSys, name);
    if (obj != nullptr) {
        char text[1024];
        va_list argPtr;
        va_start(argPtr, fmt);
        OVR::OVR_vsprintf(text, sizeof(text), fmt, argPtr);
        va_end(argPtr);
        obj->SetText(text);
    }
}

static void
SetObjectVisible(OvrGuiSys& guiSys, VRMenu* menu, char const* name, const bool visible) {
    VRMenuObject* obj = menu->ObjectForName(guiSys, name);
    if (obj != nullptr) {
        obj->SetVisible(visible);
    }
}

static inline Vector3f ovrMatrix4f_GetTranslation(const ovrMatrix4f& matrix) {
    ovrVector3f t;
    t.x = matrix.M[0][3];
    t.y = matrix.M[1][3];
    t.z = matrix.M[2][3];
    return t;
}

#if !defined(PERSISTENT_RIBBONS)
static void UpdateRibbon(
    ovrPointList& points,
    ovrPointList& velocities,
    const Vector3f& anchorPos,
    const float maxDist,
    const float deltaSeconds) {
    const Vector3f g(0.0f, -9.8f, 0.0f);
    const float invDeltaSeconds = 1.0f / deltaSeconds;

    int count = 0;
    int i = points.GetFirst();
    Vector3f& firstPoint = points.Get(i);

    Vector3f delta = anchorPos - firstPoint;

    firstPoint = anchorPos; // move the first point
    // translate( firstPoint, Vector3f( 0.0f, -1.0f, 0.0f ), deltaSeconds );

    Vector3f prevPoint = firstPoint;

    // move and accelerate all subsequent points
    for (;;) {
        i = points.GetNext(i);
        if (i < 0) {
            break;
        }

        count++;

        Vector3f& curPoint = points.Get(i);
        Vector3f& curVel = velocities.Get(i);
        curVel += g * deltaSeconds;
        curPoint = curPoint + curVel * deltaSeconds;

        delta = curPoint - prevPoint;
        const float dist = delta.Length();
        Vector3f dir = delta * 1.0f / dist;
        if (dist > maxDist) {
            Vector3f newPoint = prevPoint + dir * maxDist;
            Vector3f dragDelta = newPoint - curPoint;
            Vector3f dragVel = dragDelta * invDeltaSeconds;
            curVel = dragVel * 0.1f;
            curPoint = newPoint;
        } else {
            // damping
            curVel = curVel * 0.995f;
        }

        prevPoint = curPoint;
    }

    //	ALOG( "Ribbon: Updated %i points", count );
}
#endif

//======================================================================
// ovrControllerRibbon

//==============================
// ovrControllerRibbon::ovrControllerRibbon
ovrControllerRibbon::ovrControllerRibbon(
    const int numPoints,
    const float width,
    const float length,
    const Vector4f& color)
    : NumPoints(numPoints), Length(length) {
#if defined(PERSISTENT_RIBBONS)
    Points = new ovrPointList_Circular(numPoints);
#else
    Points = new ovrPointList_Vector(numPoints);
    Velocities = new ovrPointList_Vector(numPoints);

    for (int i = 0; i < numPoints; ++i) {
        Points->AddToTail(Vector3f(0.0f, i * (length / numPoints), 0.0f));
        Velocities->AddToTail(Vector3f(0.0f));
    }
#endif

    Ribbon = new ovrRibbon(*Points, width, color);
}

ovrControllerRibbon::~ovrControllerRibbon() {
    delete Points;
    Points = nullptr;
    delete Velocities;
    Velocities = nullptr;
    delete Ribbon;
    Ribbon = nullptr;
}

//==============================
// ovrControllerRibbon::Update
void ovrControllerRibbon::Update(
    const Matrix4f& centerViewMatrix,
    const Vector3f& anchorPos,
    const float deltaSeconds) {
    assert(Points != nullptr);
#if defined(PERSISTENT_RIBBONS)
    if (Points->GetCurPoints() == 0) {
        Points->AddToTail(anchorPos);
    } else {
        Vector3f delta = anchorPos - Points->Get(Points->GetLast());
        if (delta.Length() > 0.01f) {
            if (Points->IsFull()) {
                Points->RemoveHead();
            }
            Points->AddToTail(anchorPos);
        }
    }
#else
    assert(Velocities != nullptr);
    UpdateRibbon(*Points, *Velocities, anchorPos, Length / Points->GetMaxPoints(), deltaSeconds);
#endif
    Ribbon->Update(*Points, centerViewMatrix, true);
}

//==============================
// ovrVrInput::ovrVrInput
ovrVrInput::ovrVrInput(
    const int32_t mainThreadTid,
    const int32_t renderThreadTid,
    const int cpuLevel,
    const int gpuLevel)
    : ovrAppl(
          mainThreadTid,
          renderThreadTid,
          cpuLevel,
          gpuLevel,
          true /* useMultiView */,
          true /* overrideBackButtons */),
      RenderState(RENDER_STATE_LOADING),
      FileSys(nullptr),
      DebugLines(nullptr),
      SoundEffectPlayer(nullptr),
      GuiSys(nullptr),
      Locale(nullptr),
      SceneModel(nullptr),
      SpriteAtlas(nullptr),
      ParticleSystem(nullptr),
      BeamAtlas(nullptr),
      RemoteBeamRenderer(nullptr),
      LaserPointerBeamHandle(),
      LaserPointerParticleHandle(),
      LaserHit(false),
      ControllerModelOculusGo(nullptr),
      ControllerModelOculusGoPreLit(nullptr),
      ControllerModelOculusTouchLeft(nullptr),
      ControllerModelOculusTouchRight(nullptr),
            LastGamepadUpdateTimeInSeconds(0),
      Ribbons{nullptr, nullptr},
      ActiveInputDeviceID(uint32_t(-1)) {}

//==============================
// ovrVrInput::~ovrVrInput
ovrVrInput::~ovrVrInput() {
    for (int i = 0; i < ovrArmModel::HAND_MAX; ++i) {
        delete Ribbons[i];
        Ribbons[i] = nullptr;
    }
    delete ControllerModelOculusGo;
    ControllerModelOculusGo = nullptr;
    delete ControllerModelOculusGoPreLit;
    ControllerModelOculusGoPreLit = nullptr;
    delete ControllerModelOculusTouchLeft;
    ControllerModelOculusTouchLeft = nullptr;
    delete ControllerModelOculusTouchRight;
    ControllerModelOculusTouchRight = nullptr;
        delete SoundEffectPlayer;
    SoundEffectPlayer = nullptr;
    delete RemoteBeamRenderer;
    RemoteBeamRenderer = nullptr;
    delete ParticleSystem;
    ParticleSystem = nullptr;
    delete SpriteAtlas;
    SpriteAtlas = nullptr;

    OvrGuiSys::Destroy(GuiSys);
    if (SceneModel != nullptr) {
        delete SceneModel;
    }
}

//==============================
// ovrVrInput::AppInit
bool ovrVrInput::AppInit(const OVRFW::ovrAppContext* context) {
    const ovrJava* java = reinterpret_cast<const ovrJava*>(context->ContextForVrApi());

    FileSys = ovrFileSys::Create(*java);
    if (nullptr == FileSys) {
        ALOGE("Couldn't create FileSys");
        return false;
    }

    Locale = ovrLocale::Create(*java->Env, java->ActivityObject, "default");
    if (nullptr == Locale) {
        ALOGE("Couldn't create Locale");
        return false;
    }

    DebugLines = OvrDebugLines::Create();
    if (nullptr == DebugLines) {
        ALOGE("Couldn't create DebugLines");
        return false;
    }
    DebugLines->Init();

    SoundEffectPlayer = new OvrGuiSys::ovrDummySoundEffectPlayer();
    if (nullptr == SoundEffectPlayer) {
        ALOGE("Couldn't create SoundEffectPlayer");
        return false;
    }

    GuiSys = OvrGuiSys::Create(context);
    if (nullptr == GuiSys) {
        ALOGE("Couldn't create GUI");
        return false;
    }

    std::string fontName;
    GetLocale().GetLocalizedString("@string/font_name", "efigs.fnt", fontName);

    GuiSys->Init(FileSys, *SoundEffectPlayer, fontName.c_str(), DebugLines);

    static ovrProgramParm PreLitUniformParms[] = {
        {"Texture0", ovrProgramParmType::TEXTURE_SAMPLED},
    };

    static ovrProgramParm LitUniformParms[] = {
        {"Texture0", ovrProgramParmType::TEXTURE_SAMPLED},
        {"SpecularLightDirection", ovrProgramParmType::FLOAT_VECTOR3},
        {"SpecularLightColor", ovrProgramParmType::FLOAT_VECTOR3},
        {"AmbientLightColor", ovrProgramParmType::FLOAT_VECTOR3},
    };

    static ovrProgramParm LitWithHighlightUniformParms[] = {
        {"Texture0", ovrProgramParmType::TEXTURE_SAMPLED},
        {"SpecularLightDirection", ovrProgramParmType::FLOAT_VECTOR3},
        {"SpecularLightColor", ovrProgramParmType::FLOAT_VECTOR3},
        {"AmbientLightColor", ovrProgramParmType::FLOAT_VECTOR3},
        {"ButtonMaskTexture", ovrProgramParmType::TEXTURE_SAMPLED},
        {"HighLightMask", ovrProgramParmType::FLOAT_VECTOR4},
        {"HighLightColor", ovrProgramParmType::FLOAT_VECTOR3},
    };

    static ovrProgramParm OculusTouchUniformParms[] = {
        {"Texture0", ovrProgramParmType::TEXTURE_SAMPLED},
        {"SpecularLightDirection", ovrProgramParmType::FLOAT_VECTOR3},
        {"SpecularLightColor", ovrProgramParmType::FLOAT_VECTOR3},
        {"AmbientLightColor", ovrProgramParmType::FLOAT_VECTOR3},
    };

    SpecularLightDirection = Vector3f(0.0f, 1.0f, 0.0f);
    SpecularLightColor = Vector3f(1.0f, 0.95f, 0.8f) * 0.75f;
    AmbientLightColor = Vector3f(1.0f, 1.0f, 1.0f) * 0.15f;
    HighLightMask = Vector4f(0.0f, 0.0f, 0.0f, 0.0f);
    HighLightColor = Vector3f(1.0f, 0.55f, 0.0f) * 1.5f;

    ProgSingleTexture = OVRFW::GlProgram::Build(
        PrelitVertexShaderSrc,
        PrelitFragmentShaderSrc,
        PreLitUniformParms,
        sizeof(PreLitUniformParms) / sizeof(ovrProgramParm));

    ProgLitSpecularWithHighlight = GlProgram::Build(
        SpecularVertexShaderSrc,
        SpecularFragmentWithHighlight,
        LitWithHighlightUniformParms,
        sizeof(LitWithHighlightUniformParms) / sizeof(ovrProgramParm));

    ProgLitSpecular = GlProgram::Build(
        SpecularVertexShaderSrc,
        SpecularFragmentShaderSrc,
        LitUniformParms,
        sizeof(LitUniformParms) / sizeof(ovrProgramParm));

    ProgOculusTouch = GlProgram::Build(
        OculusTouchVertexShaderSrc,
        OculusTouchFragmentShaderSrc,
        OculusTouchUniformParms,
        sizeof(OculusTouchUniformParms) / sizeof(ovrProgramParm));

    {
        ModelGlPrograms programs;
        const char* controllerModelFile = "apk:///assets/oculusgo_controller.ovrscene";
        programs.ProgSingleTexture = &ProgLitSpecularWithHighlight;
        programs.ProgBaseColorPBR = &ProgLitSpecularWithHighlight;
        programs.ProgLightMapped = &ProgLitSpecularWithHighlight;
        MaterialParms materials;
        ControllerModelOculusGo =
            LoadModelFile(GuiSys->GetFileSys(), controllerModelFile, programs, materials);

        if (ControllerModelOculusGo == NULL ||
            static_cast<int>(ControllerModelOculusGo->Models.size()) < 1) {
            ALOGE_FAIL("Couldn't load oculus go controller model");
        }

        auto& gc = ControllerModelOculusGo->Models[0].surfaces[0].surfaceDef.graphicsCommand;
        gc.UniformData[0].Data = &gc.Textures[0];
        gc.UniformData[1].Data = &SpecularLightDirection;
        gc.UniformData[2].Data = &SpecularLightColor;
        gc.UniformData[3].Data = &AmbientLightColor;
        gc.UniformData[4].Data = &gc.Textures[1];
        gc.UniformData[5].Data = &HighLightMask;
        gc.UniformData[6].Data = &HighLightColor;

        gc.GpuState.blendEnable = ovrGpuState::BLEND_ENABLE;
        gc.GpuState.blendSrc = GL_SRC_ALPHA;
        gc.GpuState.blendDst = GL_ONE_MINUS_SRC_ALPHA;
    }

    {
        ModelGlPrograms programs;
        const char* controllerModelFile = "apk:///assets/oculusgo_controller_prelit.ovrscene";
        programs.ProgSingleTexture = &ProgSingleTexture;
        programs.ProgBaseColorPBR = &ProgSingleTexture;
        MaterialParms materials;
        ControllerModelOculusGoPreLit =
            LoadModelFile(GuiSys->GetFileSys(), controllerModelFile, programs, materials);

        if (ControllerModelOculusGoPreLit == NULL ||
            static_cast<int>(ControllerModelOculusGoPreLit->Models.size()) < 1) {
            ALOGE_FAIL("Couldn't load prelit oculus go controller model");
        }

        auto& gc = ControllerModelOculusGoPreLit->Models[0].surfaces[0].surfaceDef.graphicsCommand;
        gc.GpuState.blendEnable = ovrGpuState::BLEND_ENABLE;
        gc.GpuState.blendSrc = GL_SRC_ALPHA;
        gc.GpuState.blendDst = GL_ONE_MINUS_SRC_ALPHA;
    }

    {
        ModelGlPrograms programs;
        const char* controllerModelFile =
            "apk:///assets/oculusQuest_oculusTouch_Left.gltf.ovrscene";
        programs.ProgSingleTexture = &ProgOculusTouch;
        programs.ProgBaseColorPBR = &ProgOculusTouch;
        programs.ProgSkinnedBaseColorPBR = &ProgOculusTouch;
        programs.ProgLightMapped = &ProgOculusTouch;
        programs.ProgBaseColorEmissivePBR = &ProgOculusTouch;
        programs.ProgSkinnedBaseColorEmissivePBR = &ProgOculusTouch;
        MaterialParms materials;
        ControllerModelOculusTouchLeft =
            LoadModelFile(GuiSys->GetFileSys(), controllerModelFile, programs, materials);

        if (ControllerModelOculusTouchLeft == NULL ||
            static_cast<int>(ControllerModelOculusTouchLeft->Models.size()) < 1) {
            ALOGE_FAIL("Couldn't load Oculus Touch for Oculus Quest Controller left model");
        }

        for (auto& model : ControllerModelOculusTouchLeft->Models) {
            auto& gc = model.surfaces[0].surfaceDef.graphicsCommand;
            gc.UniformData[0].Data = &gc.Textures[0];
            gc.UniformData[1].Data = &SpecularLightDirection;
            gc.UniformData[2].Data = &SpecularLightColor;
            gc.UniformData[3].Data = &AmbientLightColor;
            gc.UniformData[4].Data = &gc.Textures[1];
        }
    }
    {
        ModelGlPrograms programs;
        const char* controllerModelFile =
            "apk:///assets/oculusQuest_oculusTouch_Right.gltf.ovrscene";
        programs.ProgSingleTexture = &ProgOculusTouch;
        programs.ProgBaseColorPBR = &ProgOculusTouch;
        programs.ProgSkinnedBaseColorPBR = &ProgOculusTouch;
        programs.ProgLightMapped = &ProgOculusTouch;
        programs.ProgBaseColorEmissivePBR = &ProgOculusTouch;
        programs.ProgSkinnedBaseColorEmissivePBR = &ProgOculusTouch;
        MaterialParms materials;
        ControllerModelOculusTouchRight =
            LoadModelFile(GuiSys->GetFileSys(), controllerModelFile, programs, materials);

        if (ControllerModelOculusTouchRight == NULL ||
            static_cast<int>(ControllerModelOculusTouchRight->Models.size()) < 1) {
            ALOGE_FAIL(
                "Couldn't load Oculus Touch for Oculus Quest Controller Controller right model");
        }

        for (auto& model : ControllerModelOculusTouchRight->Models) {
            auto& gc = model.surfaces[0].surfaceDef.graphicsCommand;
            gc.UniformData[0].Data = &gc.Textures[0];
            gc.UniformData[1].Data = &SpecularLightDirection;
            gc.UniformData[2].Data = &SpecularLightColor;
            gc.UniformData[3].Data = &AmbientLightColor;
            gc.UniformData[4].Data = &gc.Textures[1];
        }
    }

    
    {
        MaterialParms materialParms;
        materialParms.UseSrgbTextureFormats = false;
        const char* sceneUri = "apk:///assets/box.ovrscene";
        SceneModel = LoadModelFile(
            GuiSys->GetFileSys(), sceneUri, Scene.GetDefaultGLPrograms(), materialParms);

        if (SceneModel != nullptr) {
            Scene.SetWorldModel(*SceneModel);
            Vector3f modelOffset;
            modelOffset.x = 0.5f;
            modelOffset.y = 0.0f;
            modelOffset.z = -2.25f;
            Scene.GetWorldModel()->State.SetMatrix(
                Matrix4f::Scaling(2.5f, 2.5f, 2.5f) * Matrix4f::Translation(modelOffset));
        }
    }

    //------------------------------------------------------------------------------------------

    SpriteAtlas = new ovrTextureAtlas();
    SpriteAtlas->Init(GuiSys->GetFileSys(), "apk:///assets/particles2.ktx");
    SpriteAtlas->BuildSpritesFromGrid(4, 2, 8);

    ParticleSystem = new ovrParticleSystem();
    ParticleSystem->Init(2048, *SpriteAtlas, ovrParticleSystem::GetDefaultGpuState(), false);

    BeamAtlas = new ovrTextureAtlas();
    BeamAtlas->Init(GuiSys->GetFileSys(), "apk:///assets/beams.ktx");
    BeamAtlas->BuildSpritesFromGrid(2, 1, 2);

    RemoteBeamRenderer = new ovrBeamRenderer();
    RemoteBeamRenderer->Init(256, true);

    //------------------------------------------------------------------------------------------

    for (int i = 0; i < ovrArmModel::HAND_MAX; ++i) {
        Ribbons[i] = new ovrControllerRibbon(
            NUM_RIBBON_POINTS, 0.025f, 1.0f, Vector4f(0.0f, 0.5f, 1.0f, 1.0f));
    }

    //------------------------------------------------------------------------------------------

    Menu = ovrControllerGUI::Create(*this);
    if (Menu != nullptr) {
        GuiSys->AddMenu(Menu);
        GuiSys->OpenMenu(Menu->GetName());

        OVR::Posef pose = Menu->GetMenuPose();
        pose.Translation = Vector3f(0.0f, 1.0f, -2.0f);
        Menu->SetMenuPose(pose);

        SetObjectText(*GuiSys, Menu, "panel", "VrInput");
    }

    LastGamepadUpdateTimeInSeconds = 0.0;

    SurfaceRender.Init();

    return true;
}

//==============================
// ovrVrInput::ResetLaserPointer
void ovrVrInput::ResetLaserPointer() {
    if (LaserPointerBeamHandle.IsValid()) {
        RemoteBeamRenderer->RemoveBeam(LaserPointerBeamHandle);
        LaserPointerBeamHandle.Release();
    }
    if (LaserPointerParticleHandle.IsValid()) {
        ParticleSystem->RemoveParticle(LaserPointerParticleHandle);
        LaserPointerParticleHandle.Release();
    }

    // Show the gaze cursor when the remote laser pointer is not active.
    GuiSys->GetGazeCursor().ShowCursor();
}

//==============================
// ovrVrInput::AppShutdown
void ovrVrInput::AppShutdown(const OVRFW::ovrAppContext* context) {
    ALOG("AppShutdown");
    for (int i = InputDevices.size() - 1; i >= 0; --i) {
        OnDeviceDisconnected(InputDevices[i]->GetDeviceID());
    }

    ResetLaserPointer();

    OVRFW::GlProgram::Free(ProgSingleTexture);
    OVRFW::GlProgram::Free(ProgLitSpecularWithHighlight);
    OVRFW::GlProgram::Free(ProgLitSpecular);
    OVRFW::GlProgram::Free(ProgOculusTouch);

    SurfaceRender.Shutdown();
}

//==============================
// ovrVrInput::OnKeyEvent
bool ovrVrInput::OnKeyEvent(const int keyCode, const int action) {
    if (GuiSys->OnKeyEvent(keyCode, action)) {
        return true;
    }
    return false;
}

static void RenderBones(
    const ovrApplFrameIn& frame,
    ovrParticleSystem* ps,
    ovrTextureAtlas& particleAtlas,
    const uint16_t particleAtlasIndex,
    ovrBeamRenderer* br,
    ovrTextureAtlas& beamAtlas,
    const uint16_t beamAtlasIndex,
    const Posef& worldPose,
    const std::vector<ovrJoint>& joints,
    jointHandles_t& handles) {
    for (int i = 0; i < static_cast<int>(joints.size()); ++i) {
        const ovrJoint& joint = joints[i];
        const Posef jw = worldPose * joint.Pose;

        if (!handles[i].first.IsValid()) {
            handles[i].first = ps->AddParticle(
                frame,
                jw.Translation,
                0.0f,
                Vector3f(0.0f),
                Vector3f(0.0f),
                joint.Color,
                ovrEaseFunc::NONE,
                0.0f,
                0.08f,
                FLT_MAX,
                particleAtlasIndex);
        } else {
            ps->UpdateParticle(
                frame,
                handles[i].first,
                jw.Translation,
                0.0f,
                Vector3f(0.0f),
                Vector3f(0.0f),
                joint.Color,
                ovrEaseFunc::NONE,
                0.0f,
                0.08f,
                FLT_MAX,
                particleAtlasIndex);
        }

        if (i > 0) {
            const ovrJoint& parentJoint = joints[joint.ParentIndex];
            const Posef pw = worldPose * parentJoint.Pose;
            if (!handles[i].second.IsValid()) {
                handles[i].second = br->AddBeam(
                    frame,
                    beamAtlas,
                    beamAtlasIndex,
                    0.064f,
                    pw.Translation,
                    jw.Translation,
                    Vector4f(0.0f, 0.0f, 1.0f, 1.0f),
                    ovrBeamRenderer::LIFETIME_INFINITE);
            } else {
                br->UpdateBeam(
                    frame,
                    handles[i].second,
                    beamAtlas,
                    beamAtlasIndex,
                    0.064f,
                    pw.Translation,
                    jw.Translation,
                    Vector4f(0.0f, 0.0f, 1.0f, 1.0f));
            }
        }
    }
}

static void ResetBones(ovrParticleSystem* ps, ovrBeamRenderer* br, jointHandles_t& handles) {
    for (int i = 0; i < static_cast<int>(handles.size()); ++i) {
        if (handles[i].first.IsValid()) {
            ps->RemoveParticle(handles[i].first);
            handles[i].first.Release();
        }
        if (handles[i].second.IsValid()) {
            br->RemoveBeam(handles[i].second);
            handles[i].second.Release();
        }
    }
}

static void TrackpadStats(
    const Vector2f& pos,
    const Vector2f& range,
    const Vector2f& size,
    Vector2f& min,
    Vector2f& max,
    Vector2f& mm) {
    if (pos.x < min.x) {
        min.x = pos.x;
    }
    if (pos.y < min.y) {
        min.y = pos.y;
    }
    if (pos.x > max.x) {
        max.x = pos.x;
    }
    if (pos.y > max.y) {
        max.y = pos.y;
    }

    const Vector2f trackpadNormalized(pos.x / range.x, pos.y / range.y);
    mm = Vector2f(trackpadNormalized.x * size.x, trackpadNormalized.x * size.y);
}

//==============================
// ovrVrInput::AppFrame
OVRFW::ovrApplFrameOut ovrVrInput::AppFrame(const OVRFW::ovrApplFrameIn& vrFrame) {
    // process input events first because this mirrors the behavior when OnKeyEvent was
    // a virtual function on VrAppInterface and was called by VrAppFramework.
    for (int i = 0; i < static_cast<int>(vrFrame.KeyEvents.size()); i++) {
        const int keyCode = vrFrame.KeyEvents[i].KeyCode;
        const int action = vrFrame.KeyEvents[i].Action;

        if (OnKeyEvent(keyCode, action)) {
            continue; // consumed the event
        }
    }

    return OVRFW::ovrApplFrameOut();
}

void ovrVrInput::RenderRunningFrame(
    const OVRFW::ovrApplFrameIn& in,
    OVRFW::ovrRendererOutput& out) {
    // disallow player movement
    ovrApplFrameIn vrFrameWithoutMove = in;
    vrFrameWithoutMove.LeftRemote.Joystick.x = 0.0f;
    vrFrameWithoutMove.LeftRemote.Joystick.y = 0.0f;

    //------------------------------------------------------------------------------------------

    const ovrJava* java = reinterpret_cast<const ovrJava*>(GetContext()->ContextForVrApi());

    EnumerateInputDevices();
    bool recenteredController = false;
    bool hasActiveController = false;
    int iActiveInputDeviceID;
    vrapi_GetPropertyInt(java, VRAPI_ACTIVE_INPUT_DEVICE_ID, &iActiveInputDeviceID);
    ActiveInputDeviceID = (uint32_t)iActiveInputDeviceID;

    ClearAndHideMenuItems();

    // for each device, query its current tracking state and input state
    // it's possible for a device to be removed during this loop, so we go through it backwards
    for (int i = (int)InputDevices.size() - 1; i >= 0; --i) {
        ovrInputDeviceBase* device = InputDevices[i];
        if (device == nullptr) {
            assert(false); // this should never happen!
            continue;
        }
        ovrDeviceID deviceID = device->GetDeviceID();
        if (deviceID == ovrDeviceIdType_Invalid) {
            assert(deviceID != ovrDeviceIdType_Invalid);
            continue;
        }
        if (device->GetType() == ovrControllerType_TrackedRemote) {
            ovrInputDevice_TrackedRemote& trDevice =
                *static_cast<ovrInputDevice_TrackedRemote*>(device);

            if (deviceID != ovrDeviceIdType_Invalid) {
                ovrTracking remoteTracking;
                ovrResult result = vrapi_GetInputTrackingState(
                    GetSessionObject(), deviceID, in.PredictedDisplayTime, &remoteTracking);
                if (result != ovrSuccess) {
                    OnDeviceDisconnected(deviceID);
                    continue;
                }

                trDevice.SetTracking(remoteTracking);

                float yaw;
                float pitch;
                float roll;
                Quatf r(remoteTracking.HeadPose.Pose.Orientation);
                r.GetEulerAngles<Axis_Y, Axis_X, Axis_Z>(&yaw, &pitch, &roll);
                // ALOG( "MLBUPose", "Pose.r = ( %.2f, %.2f, %.2f, %.2f ), ypr( %.2f, %.2f, %.2f ),
                // t( %.2f, %.2f, %.2f )", 	r.x, r.y, r.z, r.w, 	MATH_FLOAT_RADTODEGREEFACTOR *
                // yaw,
                // MATH_FLOAT_RADTODEGREEFACTOR * pitch, MATH_FLOAT_RADTODEGREEFACTOR * roll,
                //	remoteTracking.HeadPose.Pose.Position.x,
                // remoteTracking.HeadPose.Pose.Position.y, remoteTracking.HeadPose.Pose.Position.z
                //);
                trDevice.IsActiveInputDevice = (trDevice.GetDeviceID() == ActiveInputDeviceID);
                result = PopulateRemoteControllerInfo(trDevice, recenteredController);

                if (result == ovrSuccess) {
                    if (trDevice.IsActiveInputDevice) {
                        hasActiveController = true;
                    }
                }
            }
        } else {
            ALOG("MLBUState - Unexpected Device Type %d on %d", device->GetType(), i);
        }
    }

    //------------------------------------------------------------------------------------------

    // if the orientation is tracked by the headset, don't allow the gamepad to rotate the view
    if ((vrFrameWithoutMove.Tracking.Status & VRAPI_TRACKING_STATUS_ORIENTATION_TRACKED) != 0) {
        vrFrameWithoutMove.RightRemote.Joystick.x = 0.0f;
        vrFrameWithoutMove.RightRemote.Joystick.y = 0.0f;
    }

    // Force ignoring motion
    vrFrameWithoutMove.LeftRemoteTracked = false;
    vrFrameWithoutMove.RightRemoteTracked = false;
    vrFrameWithoutMove.SingleHandRemoteTracked = false;

    // Player movement.
    Scene.SetFreeMove(true);
    Scene.Frame(vrFrameWithoutMove);

    Scene.GetFrameMatrices(SuggestedEyeFovDegreesX, SuggestedEyeFovDegreesY, out.FrameMatrices);
    Scene.GenerateFrameSurfaceList(out.FrameMatrices, out.Surfaces);

    //------------------------------------------------------------------------------------------
    // calculate the controller pose from the most recent scene pose
    Vector3f pointerStart(0.0f);
    Vector3f pointerEnd(0.0f);

    // loop through all devices to update controller arm models and place the pointer for the
    // dominant hand
    Matrix4f traceMat(out.FrameMatrices.CenterView.Inverted());
    for (int i = (int)InputDevices.size() - 1; i >= 0; --i) {
        ovrInputDeviceBase* device = InputDevices[i];
        if (device == nullptr) {
            assert(false); // this should never happen!
            continue;
        }
        ovrDeviceID deviceID = device->GetDeviceID();
        if (deviceID == ovrDeviceIdType_Invalid) {
            assert(deviceID != ovrDeviceIdType_Invalid);
            continue;
        }
        if (device->GetType() == ovrControllerType_TrackedRemote) {
            ovrInputDevice_TrackedRemote& trDevice =
                *static_cast<ovrInputDevice_TrackedRemote*>(device);

            ovrArmModel& armModel = trDevice.GetArmModel();
            const ovrTracking& tracking = trDevice.GetTracking();

            std::vector<ovrJoint> worldJoints = armModel.GetSkeleton().GetJoints();

            Posef remotePoseWithoutPosition(tracking.HeadPose.Pose);
            remotePoseWithoutPosition.Translation = Vector3f(0.0f, 0.0f, 0.0f);

            Posef headPoseWithoutPosition(in.Tracking.HeadPose.Pose);
            headPoseWithoutPosition.Translation = Vector3f(0.0f, 0.0f, 0.0f);

            Posef remotePose;
            armModel.Update(
                headPoseWithoutPosition,
                remotePoseWithoutPosition,
                trDevice.GetHand(),
                recenteredController,
                remotePose);

            if ((trDevice.GetTrackedRemoteCaps().ControllerCapabilities &
                 ovrControllerCaps_HasPositionTracking) == 0) {
                Posef neckPose(Quatf(), Vector3f(0.0f, in.EyeHeight, 0.0f));
                RenderBones(
                    in,
                    ParticleSystem,
                    *SpriteAtlas,
                    0,
                    RemoteBeamRenderer,
                    *BeamAtlas,
                    0,
                    neckPose,
                    armModel.GetTransformedJoints(),
                    trDevice.GetJointHandles());
            }

            Matrix4f mat = Matrix4f(tracking.HeadPose.Pose);

            std::vector<ovrDrawSurface>& controllerSurfaces = trDevice.GetControllerSurfaces();
            float controllerYaw = 0.0f;
            if (trDevice.GetTrackedRemoteCaps().ControllerCapabilities &
                ovrControllerCaps_ModelOculusGo) {
                controllerYaw = OVR::DegreeToRad(180.0f);
            }
            for (uint32_t k = 0; k < controllerSurfaces.size(); k++) {
                controllerSurfaces[k].modelMatrix = mat * Matrix4f::RotationY(controllerYaw);
            }

            trDevice.UpdateHaptics(GetSessionObject(), in);

            // only do the trace for the user's dominant hand
            bool updateLaser = trDevice.IsActiveInputDevice;

            if (updateLaser) {
                traceMat = mat;
                pointerStart = traceMat.Transform(Vector3f(0.0f));
                pointerEnd = traceMat.Transform(Vector3f(0.0f, 0.0f, -10.0f));

                Vector3f const pointerDir = (pointerEnd - pointerStart).Normalized();
                HitTestResult hit = GuiSys->TestRayIntersection(pointerStart, pointerDir);
                LaserHit = hit.HitHandle.IsValid();
                if (LaserHit) {
                    pointerEnd = pointerStart + hit.RayDir * hit.t -
                        pointerDir * 0.025f; // pointerDir * 0.15f;
                } else {
                    pointerEnd = pointerStart + pointerDir * 10.0f;
                }
            }
            // update ribbons
            if (Ribbons[trDevice.GetHand()] != nullptr) {
                Ribbons[trDevice.GetHand()]->Update(
                    out.FrameMatrices.CenterView, ovrMatrix4f_GetTranslation(mat), in.DeltaSeconds);
            }
        }
    }
    //------------------------------------------------------------------------------------------

    //------------------------------------------------------------------------------------------
    // if there an active controller, draw the laser pointer at the dominant hand position
    if (hasActiveController) {
        if (!LaserPointerBeamHandle.IsValid()) {
            LaserPointerBeamHandle = RemoteBeamRenderer->AddBeam(
                in,
                *BeamAtlas,
                0,
                0.032f,
                pointerStart,
                pointerEnd,
                LASER_COLOR,
                ovrBeamRenderer::LIFETIME_INFINITE);
            ALOG("MLBULaser - AddBeam %i", LaserPointerBeamHandle.Get());

            // Hide the gaze cursor when the remote laser pointer is active.
            GuiSys->GetGazeCursor().HideCursor();
        } else {
            RemoteBeamRenderer->UpdateBeam(
                in,
                LaserPointerBeamHandle,
                *BeamAtlas,
                0,
                0.032f,
                pointerStart,
                pointerEnd,
                LASER_COLOR);
        }

        if (!LaserPointerParticleHandle.IsValid()) {
            if (LaserHit) {
                LaserPointerParticleHandle = ParticleSystem->AddParticle(
                    in,
                    pointerEnd,
                    0.0f,
                    Vector3f(0.0f),
                    Vector3f(0.0f),
                    LASER_COLOR,
                    ovrEaseFunc::NONE,
                    0.0f,
                    0.1f,
                    0.1f,
                    0);
                ALOG("MLBULaser - AddParticle %i", LaserPointerParticleHandle.Get());
            }
        } else {
            if (LaserHit) {
                ParticleSystem->UpdateParticle(
                    in,
                    LaserPointerParticleHandle,
                    pointerEnd,
                    0.0f,
                    Vector3f(0.0f),
                    Vector3f(0.0f),
                    LASER_COLOR,
                    ovrEaseFunc::NONE,
                    0.0f,
                    0.1f,
                    0.1f,
                    0);
            } else {
                ParticleSystem->RemoveParticle(LaserPointerParticleHandle);
                LaserPointerParticleHandle.Release();
            }
        }

        //		ParticleSystem->AddParticle( in, pointerStart_NoWrist, 0.0f, Vector3f( 0.0f ),
        // Vector3f( 0.0f ), 				Vector4f( 0.0f, 1.0f, 1.0f, 1.0f ),
        // ovrEaseFunc::ALPHA_IN_OUT_LINEAR, 0.0f, 0.025f, 0.5f, 0 );
        ParticleSystem->AddParticle(
            in,
            pointerStart,
            0.0f,
            Vector3f(0.0f),
            Vector3f(0.0f),
            Vector4f(0.0f, 1.0f, 0.0f, 0.5f),
            ovrEaseFunc::ALPHA_IN_OUT_LINEAR,
            0.0f,
            0.025f,
            0.05f,
            0);
    } else {
        ResetLaserPointer();
    }

    GuiSys->Frame(in, out.FrameMatrices.CenterView, traceMat);

    // since we don't delete any lines, we don't need to run its frame at all
    RemoteBeamRenderer->Frame(in, out.FrameMatrices.CenterView, *BeamAtlas);
    ParticleSystem->Frame(in, *SpriteAtlas, out.FrameMatrices.CenterView);

    GuiSys->AppendSurfaceList(out.FrameMatrices.CenterView, &out.Surfaces);

    // add the controller model surfaces to the list of surfaces to render
    for (int i = 0; i < (int)InputDevices.size(); ++i) {
        ovrInputDeviceBase* device = InputDevices[i];
        if (device == nullptr) {
            assert(false); // this should never happen!
            continue;
        }
        if (device->GetType() != ovrControllerType_TrackedRemote) {
            continue;
        }
        ovrInputDevice_TrackedRemote& trDevice =
            *static_cast<ovrInputDevice_TrackedRemote*>(device);

        std::vector<ovrDrawSurface>& controllerSurfaces = trDevice.GetControllerSurfaces();
        for (auto& surface : controllerSurfaces) {
            if (surface.surface != nullptr) {
                out.Surfaces.push_back(surface);
            }
        }

        if (Ribbons[trDevice.GetHand()] != nullptr) {
            Ribbons[trDevice.GetHand()]->Ribbon->GenerateSurfaceList(out.Surfaces);
        }
    }

    const Matrix4f projectionMatrix;
    ParticleSystem->RenderEyeView(out.FrameMatrices.CenterView, projectionMatrix, out.Surfaces);
    RemoteBeamRenderer->RenderEyeView(out.FrameMatrices.CenterView, projectionMatrix, out.Surfaces);
}

void ovrVrInput::AppRenderEye(
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

void ovrVrInput::AppRenderFrame(const OVRFW::ovrApplFrameIn& in, OVRFW::ovrRendererOutput& out) {
    switch (RenderState) {
        case RENDER_STATE_LOADING: {
            DefaultRenderFrame_Loading(in, out);
        } break;
        case RENDER_STATE_RUNNING: {
            RenderRunningFrame(in, out);
            DefaultRenderFrame_Running(in, out);
        } break;
        case RENDER_STATE_ENDING: {
            DefaultRenderFrame_Ending(in, out);
        } break;
    }
}

void ovrVrInput::ClearAndHideMenuItems() {
    SetObjectColor(*GuiSys, Menu, "primary_input_trigger", Vector4f(0.25f, 0.25f, 0.25f, 1.0f));
    SetObjectColor(*GuiSys, Menu, "primary_input_triggerana", Vector4f(0.25f, 0.25f, 0.25f, 1.0f));
    SetObjectColor(
        *GuiSys, Menu, "primary_input_touchpad_click", Vector4f(0.25f, 0.25f, 0.25f, 1.0f));
    SetObjectColor(*GuiSys, Menu, "primary_input_touch", Vector4f(0.25f, 0.25f, 0.25f, 1.0f));
    SetObjectColor(*GuiSys, Menu, "primary_input_a", Vector4f(0.25f, 0.25f, 0.25f, 1.0f));
    SetObjectColor(*GuiSys, Menu, "primary_input_b", Vector4f(0.25f, 0.25f, 0.25f, 1.0f));
    SetObjectColor(*GuiSys, Menu, "primary_input_back", Vector4f(0.25f, 0.25f, 0.25f, 1.0f));
    SetObjectColor(*GuiSys, Menu, "primary_input_grip", Vector4f(0.25f, 0.25f, 0.25f, 1.0f));
    SetObjectColor(*GuiSys, Menu, "primary_input_gripana", Vector4f(0.25f, 0.25f, 0.25f, 1.0f));
    SetObjectColor(*GuiSys, Menu, "primary_input_index_point", Vector4f(0.25f, 0.25f, 0.25f, 1.0f));
    SetObjectColor(*GuiSys, Menu, "primary_input_thumb_up", Vector4f(0.25f, 0.25f, 0.25f, 1.0f));

    SetObjectColor(*GuiSys, Menu, "secondary_input_trigger", Vector4f(0.25f, 0.25f, 0.25f, 1.0f));
    SetObjectColor(
        *GuiSys, Menu, "secondary_input_triggerana", Vector4f(0.25f, 0.25f, 0.25f, 1.0f));
    SetObjectColor(
        *GuiSys, Menu, "secondary_input_touchpad_click", Vector4f(0.25f, 0.25f, 0.25f, 1.0f));
    SetObjectColor(*GuiSys, Menu, "secondary_input_touch", Vector4f(0.25f, 0.25f, 0.25f, 1.0f));
    SetObjectColor(*GuiSys, Menu, "secondary_input_a", Vector4f(0.25f, 0.25f, 0.25f, 1.0f));
    SetObjectColor(*GuiSys, Menu, "secondary_input_b", Vector4f(0.25f, 0.25f, 0.25f, 1.0f));
    SetObjectColor(*GuiSys, Menu, "secondary_input_back", Vector4f(0.25f, 0.25f, 0.25f, 1.0f));
    SetObjectColor(*GuiSys, Menu, "secondary_input_grip", Vector4f(0.25f, 0.25f, 0.25f, 1.0f));
    SetObjectColor(*GuiSys, Menu, "secondary_input_gripana", Vector4f(0.25f, 0.25f, 0.25f, 1.0f));
    SetObjectColor(
        *GuiSys, Menu, "secondary_input_index_point", Vector4f(0.25f, 0.25f, 0.25f, 1.0f));
    SetObjectColor(*GuiSys, Menu, "secondary_input_thumb_up", Vector4f(0.25f, 0.25f, 0.25f, 1.0f));

    SetObjectColor(*GuiSys, Menu, "tertiary_input_l1", Vector4f(0.25f, 0.25f, 0.25f, 1.0f));
    SetObjectColor(*GuiSys, Menu, "tertiary_input_l2", Vector4f(0.25f, 0.25f, 0.25f, 1.0f));
    SetObjectColor(*GuiSys, Menu, "tertiary_input_r1", Vector4f(0.25f, 0.25f, 0.25f, 1.0f));
    SetObjectColor(*GuiSys, Menu, "tertiary_input_r2", Vector4f(0.25f, 0.25f, 0.25f, 1.0f));
    SetObjectColor(*GuiSys, Menu, "tertiary_input_x", Vector4f(0.25f, 0.25f, 0.25f, 1.0f));
    SetObjectColor(*GuiSys, Menu, "tertiary_input_y", Vector4f(0.25f, 0.25f, 0.25f, 1.0f));
    SetObjectColor(*GuiSys, Menu, "tertiary_input_a", Vector4f(0.25f, 0.25f, 0.25f, 1.0f));
    SetObjectColor(*GuiSys, Menu, "tertiary_input_b", Vector4f(0.25f, 0.25f, 0.25f, 1.0f));
    SetObjectColor(*GuiSys, Menu, "tertiary_input_dpad", Vector4f(0.25f, 0.25f, 0.25f, 1.0f));
    SetObjectColor(*GuiSys, Menu, "tertiary_input_lstick", Vector4f(0.25f, 0.25f, 0.25f, 1.0f));
    SetObjectColor(*GuiSys, Menu, "tertiary_input_rstick", Vector4f(0.25f, 0.25f, 0.25f, 1.0f));
    SetObjectColor(*GuiSys, Menu, "tertiary_input_back", Vector4f(0.25f, 0.25f, 0.25f, 1.0f));

    SetObjectVisible(*GuiSys, Menu, "primary_input_trigger", false);
    SetObjectVisible(*GuiSys, Menu, "primary_input_triggerana", false);
    SetObjectVisible(*GuiSys, Menu, "primary_input_touchpad_click", false);
    SetObjectVisible(*GuiSys, Menu, "primary_input_touch", false);
    SetObjectVisible(*GuiSys, Menu, "primary_input_a", false);
    SetObjectVisible(*GuiSys, Menu, "primary_input_b", false);
    SetObjectVisible(*GuiSys, Menu, "primary_input_touch_pos", false);
    SetObjectVisible(*GuiSys, Menu, "primary_input_back", false);
    SetObjectVisible(*GuiSys, Menu, "primary_input_grip", false);
    SetObjectVisible(*GuiSys, Menu, "primary_input_gripana", false);
    SetObjectVisible(*GuiSys, Menu, "primary_input_touch_pos", false);
    SetObjectVisible(*GuiSys, Menu, "primary_input_button_caps", false);
    SetObjectVisible(*GuiSys, Menu, "primary_input_size_caps", false);
    SetObjectVisible(*GuiSys, Menu, "primary_input_range_caps", false);
    SetObjectVisible(*GuiSys, Menu, "primary_input_battery", false);
    SetObjectVisible(*GuiSys, Menu, "primary_input_hand", false);
    SetObjectVisible(*GuiSys, Menu, "primary_input_index_point", false);
    SetObjectVisible(*GuiSys, Menu, "primary_input_thumb_up", false);

    SetObjectVisible(*GuiSys, Menu, "secondary_input_trigger", false);
    SetObjectVisible(*GuiSys, Menu, "secondary_input_triggerana", false);
    SetObjectVisible(*GuiSys, Menu, "secondary_input_touchpad_click", false);
    SetObjectVisible(*GuiSys, Menu, "secondary_input_touch", false);
    SetObjectVisible(*GuiSys, Menu, "secondary_input_a", false);
    SetObjectVisible(*GuiSys, Menu, "secondary_input_b", false);
    SetObjectVisible(*GuiSys, Menu, "secondary_input_touch_pos", false);
    SetObjectVisible(*GuiSys, Menu, "secondary_input_back", false);
    SetObjectVisible(*GuiSys, Menu, "secondary_input_grip", false);
    SetObjectVisible(*GuiSys, Menu, "secondary_input_gripana", false);
    SetObjectVisible(*GuiSys, Menu, "secondary_input_touch_pos", false);
    SetObjectVisible(*GuiSys, Menu, "secondary_input_button_caps", false);
    SetObjectVisible(*GuiSys, Menu, "secondary_input_size_caps", false);
    SetObjectVisible(*GuiSys, Menu, "secondary_input_range_caps", false);
    SetObjectVisible(*GuiSys, Menu, "secondary_input_battery", false);
    SetObjectVisible(*GuiSys, Menu, "secondary_input_hand", false);
    SetObjectVisible(*GuiSys, Menu, "secondary_input_index_point", false);
    SetObjectVisible(*GuiSys, Menu, "secondary_input_thumb_up", false);

    SetObjectVisible(*GuiSys, Menu, "tertiary_input_l1", false);
    SetObjectVisible(*GuiSys, Menu, "tertiary_input_l2", false);
    SetObjectVisible(*GuiSys, Menu, "tertiary_input_r1", false);
    SetObjectVisible(*GuiSys, Menu, "tertiary_input_r2", false);
    SetObjectVisible(*GuiSys, Menu, "tertiary_input_x", false);
    SetObjectVisible(*GuiSys, Menu, "tertiary_input_y", false);
    SetObjectVisible(*GuiSys, Menu, "tertiary_input_a", false);
    SetObjectVisible(*GuiSys, Menu, "tertiary_input_b", false);
    SetObjectVisible(*GuiSys, Menu, "tertiary_input_dpad", false);
    SetObjectVisible(*GuiSys, Menu, "tertiary_input_lstick", false);
    SetObjectVisible(*GuiSys, Menu, "tertiary_input_rstick", false);
    SetObjectVisible(*GuiSys, Menu, "tertiary_input_back", false);
    SetObjectVisible(*GuiSys, Menu, "tertiary_input_header", false);
}

ovrResult ovrVrInput::PopulateRemoteControllerInfo(
    ovrInputDevice_TrackedRemote& trDevice,
    bool recenteredController) {
    ovrDeviceID deviceID = trDevice.GetDeviceID();

    const ovrArmModel::ovrHandedness controllerHand = trDevice.GetHand();

    const ovrJava* java = reinterpret_cast<const ovrJava*>(GetContext()->ContextForVrApi());
    ovrArmModel::ovrHandedness dominantHand =
        vrapi_GetSystemPropertyInt(java, VRAPI_SYS_PROP_DOMINANT_HAND) == VRAPI_HAND_LEFT
        ? ovrArmModel::HAND_LEFT
        : ovrArmModel::HAND_RIGHT;

    ovrInputStateTrackedRemote remoteInputState;
    remoteInputState.Header.ControllerType = trDevice.GetType();

    ovrResult result;
    result = vrapi_GetCurrentInputState(GetSessionObject(), deviceID, &remoteInputState.Header);

    if (result != ovrSuccess) {
        ALOG("MLBUState - ERROR %i getting remote input state!", result);
        OnDeviceDisconnected(deviceID);
        return result;
    }

    std::string headerObjectName;
    std::string triggerObjectName;
    std::string triggerAnalogObjectName;
    std::string gripObjectName;
    std::string gripAnalogObjectName;
    std::string touchpadClickObjectName;
    std::string touchObjectName;
    std::string touchPosObjectName;
    std::string backObjectName;
    std::string rangeObjectName;
    std::string sizeObjectName;
    std::string buttonCapsObjectName;
    std::string handObjectName;
    std::string batteryObjectName;
    std::string pointingObjectName;
    std::string thumbUpObjectName;
    std::string aButtonObjectName;
    std::string bButtonObjectName;

    if (controllerHand == dominantHand) {
        headerObjectName = "primary_input_header";
        triggerObjectName = "primary_input_trigger";
        triggerAnalogObjectName = "primary_input_triggerana";
        gripObjectName = "primary_input_grip";
        gripAnalogObjectName = "primary_input_gripana";
        touchpadClickObjectName = "primary_input_touchpad_click";
        touchObjectName = "primary_input_touch";
        touchPosObjectName = "primary_input_touch_pos";
        backObjectName = "primary_input_back";
        rangeObjectName = "primary_input_range_caps";
        sizeObjectName = "primary_input_size_caps";
        buttonCapsObjectName = "primary_input_button_caps";
        handObjectName = "primary_input_hand";
        batteryObjectName = "primary_input_battery";
        pointingObjectName = "primary_input_index_point";
        thumbUpObjectName = "primary_input_thumb_up";
        aButtonObjectName = "primary_input_a";
        bButtonObjectName = "primary_input_b";
    } else {
        headerObjectName = "secondary_input_header";
        triggerObjectName = "secondary_input_trigger";
        triggerAnalogObjectName = "secondary_input_triggerana";
        gripObjectName = "secondary_input_grip";
        gripAnalogObjectName = "secondary_input_gripana";
        touchpadClickObjectName = "secondary_input_touchpad_click";
        touchObjectName = "secondary_input_touch";
        touchPosObjectName = "secondary_input_touch_pos";
        backObjectName = "secondary_input_back";
        rangeObjectName = "secondary_input_range_caps";
        sizeObjectName = "secondary_input_size_caps";
        buttonCapsObjectName = "secondary_input_button_caps";
        handObjectName = "secondary_input_hand";
        batteryObjectName = "secondary_input_battery";
        pointingObjectName = "secondary_input_index_point";
        thumbUpObjectName = "secondary_input_thumb_up";
        aButtonObjectName = "secondary_input_a";
        bButtonObjectName = "secondary_input_b";
    }

    std::string buttons;
    char temp[128];
    OVR::OVR_sprintf(
        temp,
        sizeof(temp),
        "( %.2f, %.2f ) ",
        remoteInputState.TrackpadPosition.x,
        remoteInputState.TrackpadPosition.y);
    buttons += temp;

    if (trDevice.IsActiveInputDevice) {
        SetObjectColor(
            *GuiSys, Menu, headerObjectName.c_str(), Vector4f(0.25f, 0.75f, 0.25f, 1.0f));
    } else {
        SetObjectColor(
            *GuiSys, Menu, headerObjectName.c_str(), Vector4f(0.25f, 0.25f, 0.75f, 1.0f));
    }

    const ovrInputTrackedRemoteCapabilities* inputTrackedRemoteCapabilities =
        reinterpret_cast<const ovrInputTrackedRemoteCapabilities*>(trDevice.GetCaps());

    if ((inputTrackedRemoteCapabilities->ControllerCapabilities &
         ovrControllerCaps_ModelOculusGo) != 0) {
        SetObjectText(*GuiSys, Menu, headerObjectName.c_str(), "Oculus Go Controller");
    }
        else if (
        (inputTrackedRemoteCapabilities->ControllerCapabilities &
         ovrControllerCaps_ModelOculusTouch) != 0) {
        SetObjectText(*GuiSys, Menu, headerObjectName.c_str(), "Oculus Touch Controller");
    } else {
        SetObjectText(*GuiSys, Menu, headerObjectName.c_str(), "UNKNOWN CONTROLLER TYPE");
    }

    std::string buttonStr = "";

    if (inputTrackedRemoteCapabilities->ButtonCapabilities & ovrButton_A) {
        buttonStr += "A ";
        SetObjectVisible(*GuiSys, Menu, aButtonObjectName.c_str(), true);
        SetObjectText(*GuiSys, Menu, aButtonObjectName.c_str(), "A");
    }

    if (inputTrackedRemoteCapabilities->ButtonCapabilities & ovrButton_Trigger) {
        buttonStr += "TRG ";
        SetObjectVisible(*GuiSys, Menu, triggerObjectName.c_str(), true);
        SetObjectVisible(*GuiSys, Menu, triggerAnalogObjectName.c_str(), true);
    }

    if (inputTrackedRemoteCapabilities->ButtonCapabilities & ovrButton_B) {
        buttonStr += "B ";
        SetObjectVisible(*GuiSys, Menu, bButtonObjectName.c_str(), true);
        SetObjectText(*GuiSys, Menu, bButtonObjectName.c_str(), "B");
    }

    if (inputTrackedRemoteCapabilities->ButtonCapabilities & ovrButton_X) {
        buttonStr += "X ";
        SetObjectVisible(*GuiSys, Menu, aButtonObjectName.c_str(), true);
        SetObjectText(*GuiSys, Menu, aButtonObjectName.c_str(), "X");
    }

    if (inputTrackedRemoteCapabilities->ButtonCapabilities & ovrButton_Y) {
        buttonStr += "Y ";
        SetObjectVisible(*GuiSys, Menu, bButtonObjectName.c_str(), true);
        SetObjectText(*GuiSys, Menu, bButtonObjectName.c_str(), "Y");
    }

    if (inputTrackedRemoteCapabilities->TouchCapabilities & ovrTouch_IndexTrigger) {
        buttonStr += "TrgT ";
    }

    if (inputTrackedRemoteCapabilities->ButtonCapabilities & ovrButton_GripTrigger) {
        buttonStr += "GRIP ";
        SetObjectVisible(*GuiSys, Menu, gripObjectName.c_str(), true);
        SetObjectVisible(*GuiSys, Menu, gripAnalogObjectName.c_str(), true);
    }

    if (inputTrackedRemoteCapabilities->TouchCapabilities & ovrTouch_IndexTrigger) {
        buttonStr += "Pnt ";
        SetObjectVisible(*GuiSys, Menu, pointingObjectName.c_str(), true);
    }

    if (inputTrackedRemoteCapabilities->TouchCapabilities & ovrTouch_ThumbUp) {
        buttonStr += "Tmb ";
        SetObjectVisible(*GuiSys, Menu, thumbUpObjectName.c_str(), true);
    }

    
    if (inputTrackedRemoteCapabilities->ButtonCapabilities & ovrButton_Back) {
        buttonStr += "BACK ";
        SetObjectVisible(*GuiSys, Menu, backObjectName.c_str(), true);
    }

    if (inputTrackedRemoteCapabilities->ControllerCapabilities &
        ovrControllerCaps_ModelOculusTouch) {
        if (inputTrackedRemoteCapabilities->ButtonCapabilities & ovrButton_Enter) {
            buttonStr += "Enter ";
            SetObjectVisible(*GuiSys, Menu, backObjectName.c_str(), true);
            SetObjectText(*GuiSys, Menu, backObjectName.c_str(), "Enter");
        }
    }

    SetObjectVisible(*GuiSys, Menu, buttonCapsObjectName.c_str(), true);
    SetObjectText(*GuiSys, Menu, buttonCapsObjectName.c_str(), "Buttons: %s", buttonStr.c_str());

    if (remoteInputState.Touches & ovrTouch_IndexTrigger) {
        buttons += "TA ";
        SetObjectColor(
            *GuiSys, Menu, triggerAnalogObjectName.c_str(), Vector4f(0.25f, 1.0f, 0.0f, 1.0f));
    }

    SetObjectText(
        *GuiSys, Menu, triggerAnalogObjectName.c_str(), "%.2f", remoteInputState.IndexTrigger);

    if (remoteInputState.Buttons & ovrButton_GripTrigger) {
        buttons += "GRIP ";
        SetObjectColor(*GuiSys, Menu, gripObjectName.c_str(), Vector4f(1.0f, 0.25f, 0.25f, 1.0f));
    }
    SetObjectText(
        *GuiSys, Menu, gripAnalogObjectName.c_str(), "%.2f", remoteInputState.GripTrigger);

    if (remoteInputState.Touches & ovrTouch_IndexPointing) {
        buttons += "Pnt ";
        SetObjectColor(
            *GuiSys, Menu, pointingObjectName.c_str(), Vector4f(1.0f, 0.25f, 0.25f, 1.0f));
    }

    if (remoteInputState.Touches & ovrTouch_ThumbUp) {
        buttons += "Tmb ";
        SetObjectColor(
            *GuiSys, Menu, thumbUpObjectName.c_str(), Vector4f(1.0f, 0.25f, 0.25f, 1.0f));
    }
    
    Vector4f* hightLightMaskPointer = &HighLightMask;

    if (inputTrackedRemoteCapabilities->ControllerCapabilities &
        ovrControllerCaps_ModelOculusTouch) {
        if (controllerHand == ovrArmModel::ovrHandedness::HAND_LEFT) {
            hightLightMaskPointer = &HighLightMaskLeft;
        } else if (controllerHand == ovrArmModel::ovrHandedness::HAND_RIGHT) {
            hightLightMaskPointer = &HighLightMaskRight;
        }
    }

    if (remoteInputState.Buttons & ovrButton_Trigger &&
        (remoteInputState.Buttons & ovrButton_Enter ||
         remoteInputState.Buttons & ovrButton_Joystick)) {
        hightLightMaskPointer->x = 1.0f;
        hightLightMaskPointer->y = 1.0f;
        hightLightMaskPointer->z = 1.0f;
        hightLightMaskPointer->w = 1.0f;
    } else {
        hightLightMaskPointer->x = 0.0f;
        hightLightMaskPointer->y = 0.0f;
        hightLightMaskPointer->z = 0.0f;
        hightLightMaskPointer->w = 0.0f;
    }

    if (remoteInputState.Buttons & ovrButton_A) {
        buttons += "A ";
        SetObjectColor(
            *GuiSys, Menu, aButtonObjectName.c_str(), Vector4f(1.25f, 0.25f, 0.25f, 1.0f));
    }

    if (remoteInputState.Buttons & ovrButton_Trigger) {
        buttons += "Trigger ";
        SetObjectColor(
            *GuiSys, Menu, triggerObjectName.c_str(), Vector4f(1.0f, 0.25f, 0.25f, 1.0f));
    }

    if (remoteInputState.Touches & ovrTouch_A && remoteInputState.Buttons & ovrButton_A) {
        buttons += "A ";
        SetObjectColor(
            *GuiSys, Menu, aButtonObjectName.c_str(), Vector4f(0.25f, 0.25f, 1.0f, 1.0f));
    } else if (remoteInputState.Touches & ovrTouch_A) {
        buttons += "A ";
        SetObjectColor(
            *GuiSys, Menu, aButtonObjectName.c_str(), Vector4f(0.25f, 1.0f, 0.25f, 1.0f));
    }

    if (remoteInputState.Touches & ovrTouch_B && remoteInputState.Buttons & ovrButton_B) {
        buttons += "B ";
        SetObjectColor(
            *GuiSys, Menu, bButtonObjectName.c_str(), Vector4f(1.0f, 0.25f, 0.25f, 1.0f));
    } else if (remoteInputState.Touches & ovrTouch_B) {
        buttons += "B ";
        SetObjectColor(
            *GuiSys, Menu, bButtonObjectName.c_str(), Vector4f(0.25f, 1.0f, 0.25f, 1.0f));
    } else if (remoteInputState.Buttons & ovrButton_B) {
        buttons += "B ";
        SetObjectColor(
            *GuiSys, Menu, bButtonObjectName.c_str(), Vector4f(0.25f, 0.25f, 1.0f, 1.0f));
    }

    if (remoteInputState.Touches & ovrTouch_X && remoteInputState.Buttons & ovrButton_X) {
        buttons += "X ";
        SetObjectColor(
            *GuiSys, Menu, aButtonObjectName.c_str(), Vector4f(1.0f, 0.25f, 0.25f, 1.0f));
    } else if (remoteInputState.Touches & ovrTouch_X) {
        buttons += "X ";
        SetObjectColor(
            *GuiSys, Menu, aButtonObjectName.c_str(), Vector4f(0.25f, 1.0f, 0.25f, 1.0f));
    } else if (remoteInputState.Buttons & ovrButton_X) {
        buttons += "X ";
        SetObjectColor(
            *GuiSys, Menu, aButtonObjectName.c_str(), Vector4f(0.25f, 0.25f, 1.0f, 1.0f));
    }

    if (remoteInputState.Touches & ovrTouch_Y && remoteInputState.Buttons & ovrButton_Y) {
        buttons += "Y ";
        SetObjectColor(
            *GuiSys, Menu, bButtonObjectName.c_str(), Vector4f(1.0f, 0.25f, 0.25f, 1.0f));
    } else if (remoteInputState.Touches & ovrTouch_Y) {
        buttons += "y ";
        SetObjectColor(
            *GuiSys, Menu, bButtonObjectName.c_str(), Vector4f(0.25f, 1.0f, 0.25f, 1.0f));
    } else if (remoteInputState.Buttons & ovrButton_Y) {
        buttons += "Y ";
        SetObjectColor(
            *GuiSys, Menu, bButtonObjectName.c_str(), Vector4f(0.25f, 0.25f, 1.0f, 1.0f));
    }

    if (remoteInputState.Buttons & ovrButton_Back) {
        buttons += "BACK ";
        SetObjectColor(*GuiSys, Menu, backObjectName.c_str(), Vector4f(1.0f, 0.25f, 0.25f, 1.0f));
    }

    if (inputTrackedRemoteCapabilities->ControllerCapabilities &
        ovrControllerCaps_ModelOculusTouch) {
        if (remoteInputState.Buttons & ovrButton_Enter) {
            buttons += "ENTER";
            SetObjectColor(
                *GuiSys, Menu, backObjectName.c_str(), Vector4f(1.0f, 0.25f, 0.25f, 1.0f));
        }
    }

    if (inputTrackedRemoteCapabilities->ControllerCapabilities & ovrControllerCaps_HasTrackpad) {
        SetObjectVisible(*GuiSys, Menu, rangeObjectName.c_str(), true);
        SetObjectText(
            *GuiSys,
            Menu,
            rangeObjectName.c_str(),
            "Touch Range: ( %i, %i )",
            (int)inputTrackedRemoteCapabilities->TrackpadMaxX,
            (int)inputTrackedRemoteCapabilities->TrackpadMaxY);
        SetObjectVisible(*GuiSys, Menu, sizeObjectName.c_str(), true);
        SetObjectText(
            *GuiSys,
            Menu,
            sizeObjectName.c_str(),
            "Touch Size: ( %.0f, %.0f )",
            inputTrackedRemoteCapabilities->TrackpadSizeX,
            inputTrackedRemoteCapabilities->TrackpadSizeY);

        SetObjectVisible(*GuiSys, Menu, touchObjectName.c_str(), true);
        SetObjectVisible(*GuiSys, Menu, touchpadClickObjectName.c_str(), true);
        SetObjectText(*GuiSys, Menu, touchObjectName.c_str(), "TP Touch");
        SetObjectText(*GuiSys, Menu, touchpadClickObjectName.c_str(), "TP Click");

        if (remoteInputState.TrackpadStatus) {
            SetObjectColor(
                *GuiSys, Menu, touchObjectName.c_str(), Vector4f(1.0f, 0.25f, 0.25f, 1.0f));
        }

        if (remoteInputState.Touches & ovrTouch_TrackPad) {
            // this code is a duplicate of the above, using a slightly different color to
            // differentiate.
            SetObjectColor(
                *GuiSys, Menu, touchObjectName.c_str(), Vector4f(1.0f, 0.25f, 0.3f, 1.0f));
        }

        if (remoteInputState.Buttons & ovrButton_Enter) {
            SetObjectColor(
                *GuiSys, Menu, touchpadClickObjectName.c_str(), Vector4f(1.0f, 0.25f, 0.25f, 1.0f));
        }

        Vector2f mm(0.0f);
        TrackpadStats(
            remoteInputState.TrackpadPosition,
            Vector2f(
                trDevice.GetTrackedRemoteCaps().TrackpadMaxX,
                trDevice.GetTrackedRemoteCaps().TrackpadMaxY),
            Vector2f(
                trDevice.GetTrackedRemoteCaps().TrackpadSizeX,
                trDevice.GetTrackedRemoteCaps().TrackpadSizeY),
            trDevice.MinTrackpad,
            trDevice.MaxTrackpad,
            mm);
        SetObjectVisible(*GuiSys, Menu, touchPosObjectName.c_str(), true);
        SetObjectText(
            *GuiSys,
            Menu,
            touchPosObjectName.c_str(),
            "TP( %.2f, %.2f ) Min( %.2f, %.2f ) Max( %.2f, %.2f )",
            remoteInputState.TrackpadPosition.x,
            remoteInputState.TrackpadPosition.y,
            trDevice.MinTrackpad.x,
            trDevice.MinTrackpad.y,
            trDevice.MaxTrackpad.x,
            trDevice.MaxTrackpad.y);
    } else if (
        inputTrackedRemoteCapabilities->ControllerCapabilities & ovrControllerCaps_HasJoystick) {
        SetObjectVisible(*GuiSys, Menu, touchObjectName.c_str(), true);
        SetObjectVisible(*GuiSys, Menu, touchpadClickObjectName.c_str(), true);
        SetObjectText(*GuiSys, Menu, touchObjectName.c_str(), "JS Touch");
        SetObjectText(*GuiSys, Menu, touchpadClickObjectName.c_str(), "JS Click");

        if (remoteInputState.Touches & ovrTouch_Joystick) {
            SetObjectColor(
                *GuiSys, Menu, touchObjectName.c_str(), Vector4f(1.0f, 0.25f, 0.25f, 1.0f));
        }

        if (remoteInputState.Buttons & ovrButton_Joystick) {
            SetObjectColor(
                *GuiSys, Menu, touchpadClickObjectName.c_str(), Vector4f(1.0f, 0.25f, 0.25f, 1.0f));
        }

        Vector2f mm(0.0f);
        TrackpadStats(
            remoteInputState.Joystick,
            Vector2f(2.0f, 2.0f),
            Vector2f(2.0f, 2.0f),
            trDevice.MinTrackpad,
            trDevice.MaxTrackpad,
            mm);
        SetObjectVisible(*GuiSys, Menu, touchPosObjectName.c_str(), true);
        SetObjectText(
            *GuiSys,
            Menu,
            touchPosObjectName.c_str(),
            "JS( %.2f, %.2f ) Min( %.2f, %.2f ) Max( %.2f, %.2f )",
            remoteInputState.Joystick.x,
            remoteInputState.Joystick.y,
            trDevice.MinTrackpad.x,
            trDevice.MinTrackpad.y,
            trDevice.MaxTrackpad.x,
            trDevice.MaxTrackpad.y);
        if (remoteInputState.Joystick.x != remoteInputState.JoystickNoDeadZone.x ||
            remoteInputState.Joystick.y != remoteInputState.JoystickNoDeadZone.y) {
            SetObjectColor(
                *GuiSys, Menu, touchPosObjectName.c_str(), Vector4f(0.35f, 0.25f, 0.25f, 1.0f));
        } else {
            SetObjectColor(
                *GuiSys, Menu, touchPosObjectName.c_str(), Vector4f(0.25f, 0.25f, 0.25f, 1.0f));
        }
    }

    char const* handStr = controllerHand == ovrArmModel::HAND_LEFT ? "Left" : "Right";
    SetObjectVisible(*GuiSys, Menu, handObjectName.c_str(), true);
    SetObjectText(*GuiSys, Menu, handObjectName.c_str(), "Hand: %s", handStr);

    SetObjectVisible(*GuiSys, Menu, batteryObjectName.c_str(), true);
    SetObjectText(
        *GuiSys,
        Menu,
        batteryObjectName.c_str(),
        "Battery: %d",
        remoteInputState.BatteryPercentRemaining);

    if (remoteInputState.RecenterCount != trDevice.GetLastRecenterCount()) {
        recenteredController = true;
        ALOG(
            "MLBUState - **RECENTERED** (%i != %i )",
            (int)remoteInputState.RecenterCount,
            (int)trDevice.GetLastRecenterCount());
        trDevice.SetLastRecenterCount(remoteInputState.RecenterCount);
    }

    return result;
}

//---------------------------------------------------------------------------------------------------
// Input device management
//---------------------------------------------------------------------------------------------------

//==============================
// ovrVrInput::FindInputDevice
int ovrVrInput::FindInputDevice(const ovrDeviceID deviceID) const {
    for (int i = 0; i < (int)InputDevices.size(); ++i) {
        if (InputDevices[i]->GetDeviceID() == deviceID) {
            return i;
        }
    }
    return -1;
}

//==============================
// ovrVrInput::RemoveDevice
void ovrVrInput::RemoveDevice(const ovrDeviceID deviceID) {
    int index = FindInputDevice(deviceID);
    if (index < 0) {
        return;
    }
    ovrInputDeviceBase* device = InputDevices[index];
    delete device;
    InputDevices[index] = InputDevices.back();
    InputDevices[InputDevices.size() - 1] = nullptr;
    InputDevices.pop_back();
}

//==============================
// ovrVrInput::IsDeviceTracked
bool ovrVrInput::IsDeviceTracked(const ovrDeviceID deviceID) const {
    return FindInputDevice(deviceID) >= 0;
}

//==============================
// ovrVrInput::EnumerateInputDevices
void ovrVrInput::EnumerateInputDevices() {
    for (uint32_t deviceIndex = 0;; deviceIndex++) {
        ovrInputCapabilityHeader curCaps;

        if (vrapi_EnumerateInputDevices(GetSessionObject(), deviceIndex, &curCaps) < 0) {
            // ALOG( "Input - No more devices!" );
            break; // no more devices
        }

        if (!IsDeviceTracked(curCaps.DeviceID)) {
            ALOG("Input -      tracked");
            OnDeviceConnected(curCaps);
        }
    }
}

//==============================
// ovrVrInput::OnDeviceConnected
void ovrVrInput::OnDeviceConnected(const ovrInputCapabilityHeader& capsHeader) {
    ovrInputDeviceBase* device = nullptr;
    ovrResult result = ovrError_NotInitialized;
    switch (capsHeader.Type) {
        case ovrControllerType_TrackedRemote: {
            ALOG("MLBUConnect - Controller connected, ID = %u", capsHeader.DeviceID);

            ovrInputTrackedRemoteCapabilities remoteCapabilities;
            remoteCapabilities.Header = capsHeader;
            result =
                vrapi_GetInputDeviceCapabilities(GetSessionObject(), &remoteCapabilities.Header);
            if (result == ovrSuccess) {
                device =
                    ovrInputDevice_TrackedRemote::Create(*this, *GuiSys, *Menu, remoteCapabilities);

                // populate model surfaces.
                ovrInputDevice_TrackedRemote& trDevice =
                    *static_cast<ovrInputDevice_TrackedRemote*>(device);
                std::vector<ovrDrawSurface>& controllerSurfaces = trDevice.GetControllerSurfaces();
                ModelFile* modelFile = ControllerModelOculusGo;
                                    if (trDevice.GetTrackedRemoteCaps().ControllerCapabilities &
                        ovrControllerCaps_ModelOculusTouch) {
                        if (trDevice.GetHand() == ovrArmModel::HAND_LEFT) {
                            modelFile = ControllerModelOculusTouchLeft;
                        } else {
                            modelFile = ControllerModelOculusTouchRight;
                        }
                    }
                    
                controllerSurfaces.clear();
                for (auto& model : modelFile->Models) {
                    ovrDrawSurface controllerSurface;
                    controllerSurface.surface = &(model.surfaces[0].surfaceDef);
                    controllerSurfaces.push_back(controllerSurface);
                }

                // reflect the device type in the UI
                VRMenuObject* header = Menu->ObjectForName(*GuiSys, "primary_input_header");
                if (header != nullptr) {
                    if ((remoteCapabilities.ControllerCapabilities &
                         ovrControllerCaps_ModelOculusGo) != 0) {
                        header->SetText("Oculus Go old Controller");
                    }
                                        else if (
                        (remoteCapabilities.ControllerCapabilities &
                         ovrControllerCaps_ModelOculusTouch) != 0) {
                        header->SetText("Oculus Touch Controller");
                    }
                }
            }
            break;
        }

        default:
            ALOG("Unknown device connected");
            return;
    }

    if (result != ovrSuccess) {
        ALOG("MLBUConnect - vrapi_GetInputDeviceCapabilities: Error %i", result);
    }
    if (device != nullptr) {
        ALOG("MLBUConnect - Added device '%s', id = %u", device->GetName(), capsHeader.DeviceID);
        InputDevices.push_back(device);
    } else {
        ALOG("MLBUConnect - Device creation failed for id = %u", capsHeader.DeviceID);
    }
}

//==============================
// ovrVrInput::OnDeviceDisconnected
void ovrVrInput::OnDeviceDisconnected(const ovrDeviceID deviceID) {
    ALOG("MLBUConnect - Controller disconnected, ID = %i", deviceID);
    int deviceIndex = FindInputDevice(deviceID);
    if (deviceIndex >= 0) {
        ovrInputDeviceBase* device = InputDevices[deviceIndex];
        if (device != nullptr && device->GetType() == ovrControllerType_TrackedRemote) {
            ovrInputDevice_TrackedRemote& trDevice =
                *static_cast<ovrInputDevice_TrackedRemote*>(device);
            ResetBones(ParticleSystem, RemoteBeamRenderer, trDevice.GetJointHandles());
        }
    }
    RemoveDevice(deviceID);
}

void ovrVrInput::AppResumed(const OVRFW::ovrAppContext* /* context */) {
    ALOGV("ovrVrInput::AppResumed");
    RenderState = RENDER_STATE_RUNNING;
}

void ovrVrInput::AppPaused(const OVRFW::ovrAppContext* /* context */) {
    ALOGV("ovrVrInput::AppPaused");
}

//==============================
// ovrInputDevice_TrackedRemote::Create
ovrInputDeviceBase* ovrInputDevice_TrackedRemote::Create(
    OVRFW::ovrAppl& app,
    OvrGuiSys& guiSys,
    VRMenu& menu,
    const ovrInputTrackedRemoteCapabilities& remoteCapabilities) {
    ALOG("MLBUConnect - ovrInputDevice_TrackedRemote::Create");

    ovrInputStateTrackedRemote remoteInputState;
    remoteInputState.Header.ControllerType = remoteCapabilities.Header.Type;
    ovrResult result = vrapi_GetCurrentInputState(
        app.GetSessionObject(), remoteCapabilities.Header.DeviceID, &remoteInputState.Header);
    if (result == ovrSuccess) {
        ovrInputDevice_TrackedRemote* device =
            new ovrInputDevice_TrackedRemote(remoteCapabilities, remoteInputState.RecenterCount);

        ovrArmModel::ovrHandedness controllerHand = ovrArmModel::HAND_RIGHT;
        if ((remoteCapabilities.ControllerCapabilities & ovrControllerCaps_LeftHand) != 0) {
            controllerHand = ovrArmModel::HAND_LEFT;
        }

        char const* handStr = controllerHand == ovrArmModel::HAND_LEFT ? "left" : "right";
        ALOG(
            "MLBUConnect - Controller caps: hand = %s, Button %x, Controller %x, MaxX %d, MaxY %d, SizeX %f SizeY %f",
            handStr,
            remoteCapabilities.ButtonCapabilities,
            remoteCapabilities.ControllerCapabilities,
            remoteCapabilities.TrackpadMaxX,
            remoteCapabilities.TrackpadMaxY,
            remoteCapabilities.TrackpadSizeX,
            remoteCapabilities.TrackpadSizeY);

        device->ArmModel.InitSkeleton(controllerHand == ovrArmModel::HAND_LEFT);
        device->JointHandles.resize(device->ArmModel.GetSkeleton().GetJoints().size());

        device->HapticState = 0;
        device->HapticsSimpleValue = 0.0f;

        return device;
    } else {
        ALOG("MLBUConnect - vrapi_GetCurrentInputState: Error %i", result);
    }

    return nullptr;
}

enum HapticStates {
    HAPTICS_NONE = 0,
    HAPTICS_BUFFERED = 1,
    HAPTICS_SIMPLE = 2,
    HAPTICS_SIMPLE_CLICKED = 3
};

//==============================
// ovrInputDevice_TrackedRemote::Create
void ovrInputDevice_TrackedRemote::UpdateHaptics(ovrMobile* ovr, const ovrApplFrameIn& vrFrame) {
    if (GetTrackedRemoteCaps().ControllerCapabilities &
            ovrControllerCaps_HasSimpleHapticVibration ||
        GetTrackedRemoteCaps().ControllerCapabilities &
            ovrControllerCaps_HasBufferedHapticVibration) {
        ovrResult result;
        ovrInputStateTrackedRemote remoteInputState;
        remoteInputState.Header.ControllerType = GetType();
        result = vrapi_GetCurrentInputState(ovr, GetDeviceID(), &remoteInputState.Header);

        bool gripDown = (remoteInputState.Buttons & ovrButton_GripTrigger) > 0;
        bool trigDown = (remoteInputState.Buttons & ovrButton_A) > 0;
        trigDown |= (remoteInputState.Buttons & ovrButton_Trigger) > 0;
        bool touchDown = remoteInputState.TrackpadStatus;
        bool touchClicked = (remoteInputState.Buttons & ovrButton_Enter ||
                             remoteInputState.Buttons & ovrButton_Joystick) > 0;

        const int maxSamples = GetTrackedRemoteCaps().HapticSamplesMax;

        if (gripDown && (touchDown || touchClicked)) {
            if (trigDown) {
                if (GetTrackedRemoteCaps().ControllerCapabilities &
                    ovrControllerCaps_HasBufferedHapticVibration) {
                    // buffered haptics
                    float intensity = 0.0f;
                    intensity = fmodf(vrFrame.PredictedDisplayTime, 1.0f);

                    ovrHapticBuffer hapticBuffer;
                    uint8_t dataBuffer[maxSamples];
                    hapticBuffer.BufferTime = vrFrame.PredictedDisplayTime;
                    hapticBuffer.NumSamples = maxSamples;
                    hapticBuffer.HapticBuffer = dataBuffer;
                    hapticBuffer.Terminated = false;

                    for (int i = 0; i < maxSamples; i++) {
                        dataBuffer[i] = intensity * 255;
                        intensity += (float)GetTrackedRemoteCaps().HapticSampleDurationMS * 0.001f;
                        intensity = fmodf(intensity, 1.0f);
                    }

                    vrapi_SetHapticVibrationBuffer(ovr, GetDeviceID(), &hapticBuffer);
                    HapticState = HAPTICS_BUFFERED;
                } else {
                    ALOG("Device does not support buffered haptics?");
                }

            } else {
                // simple haptics
                if (touchClicked) {
                    if (GetTrackedRemoteCaps().ControllerCapabilities &
                        ovrControllerCaps_HasSimpleHapticVibration) {
                        if (HapticState != HAPTICS_SIMPLE_CLICKED) {
                            vrapi_SetHapticVibrationSimple(ovr, GetDeviceID(), 1.0f);
                            HapticState = HAPTICS_SIMPLE_CLICKED;
                            HapticsSimpleValue = 1.0f;
                        }
                    } else {
                        ALOG("Device does not support simple haptics?");
                    }
                } else {
                    // huge epsilon value since there is so much noise in the grip trigger
                    // and currently a problem with sending too many haptics values.
                    if (HapticsSimpleValue < (remoteInputState.GripTrigger - 0.05f) ||
                        HapticsSimpleValue > (remoteInputState.GripTrigger + 0.05f)) {
                        vrapi_SetHapticVibrationSimple(
                            ovr, GetDeviceID(), remoteInputState.GripTrigger);
                        HapticState = HAPTICS_SIMPLE;
                        HapticsSimpleValue = remoteInputState.GripTrigger;
                    }
                }
            }
        } else if (HapticState == HAPTICS_BUFFERED) {
            ovrHapticBuffer hapticBuffer;
            uint8_t dataBuffer[maxSamples];
            hapticBuffer.BufferTime = vrFrame.PredictedDisplayTime;
            hapticBuffer.NumSamples = maxSamples;
            hapticBuffer.HapticBuffer = dataBuffer;
            hapticBuffer.Terminated = true;

            for (int i = 0; i < maxSamples; i++) {
                dataBuffer[i] = (((float)i) / (float)maxSamples) * 255;
            }

            vrapi_SetHapticVibrationBuffer(ovr, GetDeviceID(), &hapticBuffer);
            HapticState = HAPTICS_NONE;
        } else if (HapticState == HAPTICS_SIMPLE || HapticState == HAPTICS_SIMPLE_CLICKED) {
            vrapi_SetHapticVibrationSimple(ovr, GetDeviceID(), 0.0f);
            HapticState = HAPTICS_NONE;
            HapticsSimpleValue = 0.0f;
        }
    }
}

} // namespace OVRFW
