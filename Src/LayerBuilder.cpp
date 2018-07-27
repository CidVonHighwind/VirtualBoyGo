#include "LayerBuilder.h"
#include <VrApi/Include/VrApi_Types.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

using namespace OVR;

namespace LayerBuilder {

#define FOLLOW_SPEED 0.05f

float screenYaw = 0;
float screenPitch = 0;
float screenRoll = 0;
float radiusMenuScreen = 5.5f;
float screenSize = 1.0f;

glm::fquat currentQuat, goalQuat;
ovrQuatf currentfQuat;

void ResetValues() {
  screenYaw = 0;
  screenPitch = 0;
  screenRoll = 0;
  radiusMenuScreen = 0.75f;
  screenSize = 1.0f;
}

static void toEulerAngle(const ovrQuatf &q, float &roll, float &pitch, float &yaw) {
  // roll (x-axis rotation)
  float sinr = +2.0f * (q.w * q.x + q.y * q.z);
  float cosr = +1.0f - 2.0f * (q.x * q.x + q.y * q.y);
  roll = atan2f(sinr, cosr);

  // pitch (y-axis rotation)
  float sinp = +2.0f * (q.w * q.y - q.z * q.x);
  if (fabs(sinp) >= 1)
    pitch = copysignf(VRAPI_PI / 2.0f, sinp);  // use 90 degrees if out of range
  else
    pitch = asinf(sinp);

  // yaw (z-axis rotation)
  float siny = +2.0f * (q.w * q.z + q.x * q.y);
  float cosy = +1.0f - 2.0f * (q.y * q.y + q.z * q.z);
  yaw = atan2f(siny, cosy);
}

void UpdateDirection(const ovrFrameInput &vrFrame) {
  goalQuat.w = vrFrame.Tracking.HeadPose.Pose.Orientation.w;
  goalQuat.x = vrFrame.Tracking.HeadPose.Pose.Orientation.x;
  goalQuat.y = vrFrame.Tracking.HeadPose.Pose.Orientation.y;
  goalQuat.z = vrFrame.Tracking.HeadPose.Pose.Orientation.z;

  currentQuat = glm::mix(currentQuat, goalQuat, FOLLOW_SPEED);

  currentfQuat = {currentQuat.x, currentQuat.y, currentQuat.z, currentQuat.w};
}

static ovrMatrix4f CylinderModelMatrix(const int texHeight, const ovrVector3f translation,
                                       const float radius, const ovrQuatf *q, const float density,
                                       const float offsetY) {
  const ovrMatrix4f rotXMatrix = ovrMatrix4f_CreateRotation(screenPitch + offsetY, 0.0f, 0.0f);
  const ovrMatrix4f rotYMatrix = ovrMatrix4f_CreateRotation(0.0f, screenYaw, 0.0f);
  const ovrMatrix4f rotZMatrix = ovrMatrix4f_CreateRotation(0.0f, 0.0f, screenRoll);

  const ovrMatrix4f scaleMatrix =
      ovrMatrix4f_CreateScale(radius, radius * (float) texHeight * VRAPI_PI / density, radius);

  const ovrMatrix4f transMatrix =
      ovrMatrix4f_CreateTranslation(translation.x, translation.y, translation.z);

  // fixed
  if (q == nullptr) {
    const ovrMatrix4f m0 = ovrMatrix4f_Multiply(&rotYMatrix, &rotXMatrix);
    const ovrMatrix4f m1 = ovrMatrix4f_Multiply(&m0, &rotZMatrix);
    const ovrMatrix4f m2 = ovrMatrix4f_Multiply(&m1, &scaleMatrix);
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

ovrLayerCylinder2 BuildSettingsCylinderLayer(ovrTextureSwapChain *cylinderSwapChain,
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

  float roll, pitch, yaw;
  toEulerAngle(tracking->HeadPose.Pose.Orientation, roll, pitch, yaw);

  // clamp the settings size
  // maybe this should be separate from the emulator screen
  float settingsSize = screenSize;
  if (settingsSize < 0.75f)
    settingsSize = 0.75f;
  else if (settingsSize > 1.0f)
    settingsSize = 1.0f;

  const float density = 6400.0f / settingsSize;
  const ovrVector3f _translation =
      tracking->HeadPose.Pose.Position;  // {0.0f, 0.0f, 0.0f}; // tracking->HeadPose.Pose.Position;
  // maybe use vrapi_GetTransformFromPose instead

  ovrMatrix4f cylinderTransform =
      CylinderModelMatrix(textureHeight, _translation, radiusMenuScreen,
                          followHead ? &currentfQuat : nullptr, density, (1 - offsetY) * 0.025f);

  const float circScale = density * 0.5f / textureWidth;
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
    const float texScaleY = 0.5f;
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

ovrLayerCylinder2 BuildGameCylinderLayer(ovrTextureSwapChain *cylinderSwapChain,
                                         const int textureWidth, const int textureHeight,
                                         const ovrTracking2 *tracking, bool followHead) {
  ovrLayerCylinder2 layer = vrapi_DefaultLayerCylinder2();

  const float fadeLevel = 1.0f;
  layer.Header.ColorScale.x = layer.Header.ColorScale.y = layer.Header.ColorScale.z =
  layer.Header.ColorScale.w = fadeLevel;
  layer.Header.SrcBlend = VRAPI_FRAME_LAYER_BLEND_SRC_ALPHA;
  layer.Header.DstBlend = VRAPI_FRAME_LAYER_BLEND_ONE_MINUS_SRC_ALPHA;

  layer.HeadPose = tracking->HeadPose;

  // x2: 3250.0f
  const float density = 4875 / screenSize;
  const ovrVector3f translation = tracking->HeadPose.Pose.Position;

  ovrMatrix4f cylinderTransform =
      CylinderModelMatrix(textureHeight, translation, radiusMenuScreen + radiusMenuScreen * 0.02f,
                          followHead ? &currentfQuat : nullptr, density, 0);

  const float circScale = density * 0.5f / textureWidth;
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
    const float texScaleY = 0.5f;
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

ovrLayerCylinder2 BuildGameCylinderLayer3D(ovrTextureSwapChain *cylinderSwapChain,
                                           const int textureWidth,
                                           const int textureHeight,
                                           const ovrTracking2 *tracking,
                                           bool followHead,
                                           bool threedee) {
  ovrLayerCylinder2 layer = vrapi_DefaultLayerCylinder2();

  const float fadeLevel = 1.0f;
  layer.Header.ColorScale.x = layer.Header.ColorScale.y = layer.Header.ColorScale.z =
  layer.Header.ColorScale.w = fadeLevel;
  layer.Header.SrcBlend = VRAPI_FRAME_LAYER_BLEND_SRC_ALPHA;
  layer.Header.DstBlend = VRAPI_FRAME_LAYER_BLEND_ONE_MINUS_SRC_ALPHA;

  layer.HeadPose = tracking->HeadPose;

  // x2: 3250.0f
  const float density = 4875 / screenSize / 0.95f / 2.0f;
  const ovrVector3f translation = tracking->HeadPose.Pose.Position;

  ovrMatrix4f cylinderTransform =
      CylinderModelMatrix(textureHeight, translation, radiusMenuScreen + radiusMenuScreen * 0.02f,
                          followHead ? &currentfQuat : nullptr, density, 0);

  const float circScale = density * 0.5f / textureWidth;
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
    const float texScaleY = 0.25f;
    float texBiasY = 0.0625f * 2;

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

static inline ovrLayerCube2 vrapi_ModLayerCube2() {
  ovrLayerCube2 layer = {};

  const ovrMatrix4f projectionMatrix =
      ovrMatrix4f_CreateProjectionFov(90.0f, 90.0f, 0.0f, 0.0f, 0.1f, 0.0f);
  const ovrMatrix4f texCoordsFromTanAngles =
      ovrMatrix4f_TanAngleMatrixFromProjection(&projectionMatrix);

  layer.Header.Type = VRAPI_LAYER_TYPE_CUBE2;
  layer.Header.Flags = 0;
  layer.Header.ColorScale.x = 1.0f;
  layer.Header.ColorScale.y = 1.0f;
  layer.Header.ColorScale.z = 1.0f;
  layer.Header.ColorScale.w = 1.0f;
  layer.Header.SrcBlend = VRAPI_FRAME_LAYER_BLEND_ONE;
  layer.Header.DstBlend = VRAPI_FRAME_LAYER_BLEND_ZERO;
  //layer.Header.SurfaceTextureObject = NULL;

  // layer.HeadPose.Pose.Orientation.w = 1.0f;
  layer.TexCoordsFromTanAngles = texCoordsFromTanAngles;

  layer.Offset.x = 0.0f;
  layer.Offset.y = 0.0f;
  layer.Offset.z = 0.0f;

  return layer;
}

/*
ovrLayerCube2 BuildCubeLayer(ovrTextureSwapChain *cubeSwapChainLeft,
                             ovrTextureSwapChain *cubeSwapChainRight,
                             const ovrTracking2 *tracking, bool followHead) {
  float scale = screenSize * 0.5f;
  const ovrMatrix4f scaleMatrix = ovrMatrix4f_CreateScale(-1.0f, 1.0f, scale);

  const ovrMatrix4f rotXMatrix = ovrMatrix4f_CreateRotation(-screenPitch, 0.0f, 0.0f);
  const ovrMatrix4f rotYMatrix = ovrMatrix4f_CreateRotation(0.0f, -screenYaw, 0.0f);
  const ovrMatrix4f rotZMatrix = ovrMatrix4f_CreateRotation(0.0f, 0.0f, screenRoll);

  const ovrMatrix4f centerEyeViewMatrix = vrapi_GetViewMatrixFromPose(&tracking->HeadPose.Pose);
  const ovrMatrix4f vMatrix = ovrMatrix4f_TanAngleMatrixForCubeMap(&centerEyeViewMatrix);

  //const ovrMatrix4f m1 = ovrMatrix4f_Multiply(&rotYMatrix, &scaleMatrix);
  //const ovrMatrix4f m2 = ovrMatrix4f_Multiply(&m1, &vMatrix);

  // const ovrVector3f translation = tracking->HeadPose.Pose.Position;

  ovrLayerCube2 layer = vrapi_ModLayerCube2();
  layer.HeadPose = tracking->HeadPose;

  const ovrMatrix4f m0 = ovrMatrix4f_Multiply(&rotZMatrix, &scaleMatrix);
  const ovrMatrix4f m1 = ovrMatrix4f_Multiply(&m0, &rotYMatrix);
  const ovrMatrix4f m2 = ovrMatrix4f_Multiply(&m1, &rotXMatrix);
  const ovrMatrix4f m3 = ovrMatrix4f_Multiply(&m2, &vMatrix);

  layer.TexCoordsFromTanAngles = m3;

  // follow head
  if (followHead) {
    // follow head
    const ovrMatrix4f rotOneMatrix = ovrMatrix4f_CreateFromQuaternion(&currentfQuat);
    const ovrMatrix4f rotinfMatrix = ovrMatrix4f_Inverse(&rotOneMatrix);
    const ovrMatrix4f m4 = ovrMatrix4f_Multiply(&m3, &rotinfMatrix);
    layer.TexCoordsFromTanAngles = m4;
  }

  // Apply an optional directional bias [-1.0f,1.0f] to improve
  // quality and sample density in a particular direction.
  layer.Offset.x = 0.0f;
  layer.Offset.y = 0.0f;
  layer.Offset.z = 0.0f;

  layer.Textures[0].ColorSwapChain = cubeSwapChainLeft;
  layer.Textures[0].SwapChainIndex = 0;
  layer.Textures[1].ColorSwapChain = cubeSwapChainRight;
  layer.Textures[1].SwapChainIndex = 0;

  return layer;
}
 */

}  // namespace LayerBuilder