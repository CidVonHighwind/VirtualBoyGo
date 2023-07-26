#include "LayerBuilder.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <OVR_LogUtils.h>
#include <OVR_Math.h>

#include "Appl.h"

using namespace OVR;

void LayerBuilder::ResetValues() {
    screenYaw = 0;
    screenPitch = 0;
    screenRoll = 0;
    radiusMenuScreen = 5.5f;
    screenSize = 1.0f;
}

float LayerBuilder::GetScreenScale() {
    return screenSize * 0.65f;
}

void LayerBuilder::UpdateDirection(const OVRFW::ovrApplFrameIn &in) {
    goalQuat.x = in.Tracking.HeadPose.Pose.Orientation.x;
    goalQuat.y = in.Tracking.HeadPose.Pose.Orientation.y;
    goalQuat.z = in.Tracking.HeadPose.Pose.Orientation.z;
    goalQuat.w = in.Tracking.HeadPose.Pose.Orientation.w;

    currentQuat = glm::slerp(currentQuat, goalQuat, FOLLOW_SPEED * in.DeltaSeconds);

    currentfQuat = {currentQuat.x, currentQuat.y, currentQuat.z, currentQuat.w};

    // if the tracking was recentered reset the forward direction
    if (in.RecenterCount != resetCounter){
        resetCounter = in.RecenterCount;
        forwardYaw = 0;
    }
}

void LayerBuilder::ResetForwardDirection(const OVRFW::ovrRendererOutput &out) {
    float pitch, roll;
    auto viewMatrix = out.FrameMatrices.CenterView;
    viewMatrix.ToEulerAngles<Axis_Y, Axis_X, Axis_Z, Rotate_CCW, Handed_R>(&forwardYaw, &pitch, &roll);
    OVR_LOG("Reset %f %f %f", forwardYaw, pitch, roll);
}

ovrMatrix4f LayerBuilder::CylinderModelMatrix(const int texHeight, const ovrVector3f translation, const float radius, const ovrQuatf *q, const float offsetY) {
    const ovrMatrix4f rotXMatrix = ovrMatrix4f_CreateRotation(DegreeToRad(screenPitch) + offsetY, 0.0f, 0.0f);
    const ovrMatrix4f rotYMatrix = ovrMatrix4f_CreateRotation(0.0f, DegreeToRad(screenYaw), 0.0f);
    const ovrMatrix4f rotZMatrix = ovrMatrix4f_CreateRotation(0.0f, 0.0f, DegreeToRad(screenRoll));

    const ovrMatrix4f scaleMatrix = ovrMatrix4f_CreateScale(radius, radius, radius);

    const ovrMatrix4f transMatrix = ovrMatrix4f_CreateTranslation(translation.x, translation.y, translation.z);

    // fixed
    if (q == nullptr) {
        const ovrMatrix4f m0 = ovrMatrix4f_Multiply(&rotYMatrix, &rotXMatrix);
        const ovrMatrix4f m1 = ovrMatrix4f_Multiply(&m0, &rotZMatrix);

        Matrix4f unrotYawMatrix(Quatf(Axis_Y, -forwardYaw));
        const ovrMatrix4f mf = ovrMatrix4f_Multiply(&m1, (ovrMatrix4f *) &unrotYawMatrix);

        const ovrMatrix4f m2 = ovrMatrix4f_Multiply(&mf, &scaleMatrix);
        const ovrMatrix4f m3 = ovrMatrix4f_Multiply(&transMatrix, &m2);

        return m3;
    }

    // follow head
    const ovrMatrix4f rotOneMatrix = ovrMatrix4f_CreateFromQuaternion(q);

    const ovrMatrix4f m0 = ovrMatrix4f_Multiply(&rotYMatrix, &rotXMatrix);
    const ovrMatrix4f m1 = ovrMatrix4f_Multiply(&m0, &rotZMatrix);
    const ovrMatrix4f m2 = ovrMatrix4f_Multiply(&m1, &scaleMatrix);
    const ovrMatrix4f m3 = ovrMatrix4f_Multiply(&rotOneMatrix, &m2);
    const ovrMatrix4f m4 = ovrMatrix4f_Multiply(&transMatrix, &m3);

    return m4;
}

ovrLayerCylinder2 LayerBuilder::BuildSettingsCylinderLayer(ovrTextureSwapChain *cylinderSwapChain,
                                                           const int textureWidth, const int textureHeight,
                                                           const ovrTracking2 *tracking, bool followHead,
                                                           float offsetY) {
    ovrLayerCylinder2 layer = vrapi_DefaultLayerCylinder2();

    const float fadeLevel = 1.0f;
    layer.Header.ColorScale.x = layer.Header.ColorScale.y = layer.Header.ColorScale.z =
    layer.Header.ColorScale.w = fadeLevel;
    layer.Header.SrcBlend = VRAPI_FRAME_LAYER_BLEND_SRC_ALPHA;
    layer.Header.DstBlend = VRAPI_FRAME_LAYER_BLEND_ONE_MINUS_SRC_ALPHA;

    layer.HeadPose = tracking->HeadPose;

    //float roll, pitch, yaw;
    //toEulerAngle(tracking->HeadPose.Pose.Orientation, roll, pitch, yaw);

    // clamp the settings size
    // maybe this should be separate from the emulator screen
    float settingsSize = GetScreenScale();
    if (settingsSize < 0.5f)
        settingsSize = 0.5f;
    else if (settingsSize > 0.65f)
        settingsSize = 0.65f;

    const ovrVector3f _translation = tracking->HeadPose.Pose.Position;  // {0.0f, 0.0f, 0.0f}; // tracking->HeadPose.Pose.Position;
    // maybe use vrapi_GetTransformFromPose instead

    ovrMatrix4f cylinderTransform =
            CylinderModelMatrix(textureHeight, _translation, radiusMenuScreen, followHead ? &currentfQuat : nullptr, (1 - offsetY) * 0.025f);

    float aspectRation = (float) textureWidth / (float) textureHeight;
    const float circScale = (1.0f * (180 / 60.0f) / aspectRation) / settingsSize;// * (textureWidth / (float)textureHeight);
    const float circBias = -circScale * (0.5f * (1.0f - 1.0f / circScale));

    for (int eye = 0; eye < VRAPI_FRAME_LAYER_EYE_MAX; eye++) {
        ovrMatrix4f modelViewMatrix =
                ovrMatrix4f_Multiply(&tracking->Eye[eye].ViewMatrix, &cylinderTransform);
        layer.Textures[eye].TexCoordsFromTanAngles = ovrMatrix4f_Inverse(&modelViewMatrix);
        layer.Textures[eye].ColorSwapChain = cylinderSwapChain;
        layer.Textures[eye].SwapChainIndex = 0;

        // Texcoord scale and bias is just a representation of the aspect ratio. The positioning
        // of the cylinder is handled entirely by the TexCoordsFromTanAngles matrix.

        const float texScaleX = circScale;
        const float texBiasX = circBias;
        const float texScaleY = 1.0f / settingsSize;
        const float texBiasY = -texScaleY * (0.5f * (1.0f - (1.0f / texScaleY)));

        layer.Textures[eye].TextureMatrix.M[0][0] = texScaleX;
        layer.Textures[eye].TextureMatrix.M[0][2] = texBiasX;
        layer.Textures[eye].TextureMatrix.M[1][1] = texScaleY;
        layer.Textures[eye].TextureMatrix.M[1][2] = texBiasY;

        layer.Textures[eye].TextureRect.width = 1.0f;
        layer.Textures[eye].TextureRect.height = 1.0f;
    }

    return layer;
}

ovrLayerCylinder2 LayerBuilder::BuildGameCylinderLayer3D(ovrTextureSwapChain *cylinderSwapChain, const int textureWidth, const int textureHeight,
                                                         const ovrTracking2 *tracking, bool followHead, bool threedee, float threedeeoffset, float ipd) {
    ovrLayerCylinder2 layer = vrapi_DefaultLayerCylinder2();

    const float fadeLevel = 1.0f;
    layer.Header.ColorScale.x = layer.Header.ColorScale.y = layer.Header.ColorScale.z = layer.Header.ColorScale.w = fadeLevel;
    layer.Header.SrcBlend = VRAPI_FRAME_LAYER_BLEND_SRC_ALPHA;
    layer.Header.DstBlend = VRAPI_FRAME_LAYER_BLEND_ONE_MINUS_SRC_ALPHA;

    layer.HeadPose = tracking->HeadPose;

    float aspectRation = (float) textureWidth / (float) textureHeight;
    const float circScale = (1.0f * (180 / 60.0f) / aspectRation) / GetScreenScale();// density * 0.5f / textureWidth;
    const float circBias = -circScale * (0.5f * (1.0f - 1.0f / circScale));

    for (int eye = 0; eye < VRAPI_FRAME_LAYER_EYE_MAX; eye++) {

        // x2: 3250.0f
        Vector3f transform = {tracking->HeadPose.Pose.Position.x, tracking->HeadPose.Pose.Position.y, tracking->HeadPose.Pose.Position.z};
        ovrQuatf orientation = tracking->HeadPose.Pose.Orientation;
        Quatf rotation = {orientation.x, orientation.y, orientation.z, orientation.w};

        if (threedee)
            if (eye == VRAPI_FRAME_LAYER_EYE_LEFT) {
                transform += (Vector3f(-1.0f, 0.0f, 0.0f) * rotation) * ((threedeeoffset / 2) * radiusMenuScreen + ipd / 2);
            } else {
                transform += (Vector3f(1.0f, 0.0f, 0.0f) * rotation) * ((threedeeoffset / 2) * radiusMenuScreen + ipd / 2);
            }

        // 0.252717949
        ovrVector3f transf = {transform.x, transform.y, transform.z};
        ovrMatrix4f cylinderTransform = CylinderModelMatrix(textureHeight, transf,
                                                            radiusMenuScreen + radiusMenuScreen * 0.02f, followHead ? &currentfQuat : nullptr, 0);

        ovrMatrix4f modelViewMatrix = ovrMatrix4f_Multiply(&tracking->Eye[eye].ViewMatrix, &cylinderTransform);

        layer.Textures[eye].TexCoordsFromTanAngles = ovrMatrix4f_Inverse(&modelViewMatrix);
        layer.Textures[eye].ColorSwapChain = cylinderSwapChain;
        layer.Textures[eye].SwapChainIndex = 0;

        // Texcoord scale and bias is just a representation of the aspect ratio. The positioning
        // of the cylinder is handled entirely by the TexCoordsFromTanAngles matrix.

        const float texScaleX = circScale;
        const float texBiasX = circBias;
        const float texScaleY = 0.5f / GetScreenScale();
        float texBiasY = -0.25f / GetScreenScale() + 0.25f;

        if (eye == 1 && threedee)
            texBiasY += 0.5f;

        layer.Textures[eye].TextureMatrix.M[0][0] = texScaleX;
        layer.Textures[eye].TextureMatrix.M[0][2] = texBiasX;
        layer.Textures[eye].TextureMatrix.M[1][1] = texScaleY;
        layer.Textures[eye].TextureMatrix.M[1][2] = texBiasY;

        layer.Textures[eye].TextureRect.y = (eye == 1 && threedee) ? 0.5f : 0.0f;
        layer.Textures[eye].TextureRect.width = 1.0f;
        layer.Textures[eye].TextureRect.height = 0.5f;
    }

    return layer;
}