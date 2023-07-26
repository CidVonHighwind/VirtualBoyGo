#pragma once

#include <glm/fwd.hpp>
#include <glm/gtc/quaternion.hpp>
#include "Appl.h"

#define FOLLOW_SPEED 1.0f

class LayerBuilder {
public:
    float screenYaw = 0;
    float screenPitch = 0;
    float screenRoll = 0;
    float radiusMenuScreen = 5.5f;
    float screenSize = 1.0f;

    float forwardYaw = 0.0f;

    int resetCounter = 0;

    glm::quat currentQuat;
    glm::quat goalQuat;
    ovrQuatf currentfQuat;

    void UpdateDirection(const OVRFW::ovrApplFrameIn &in);

    void ResetForwardDirection(const OVRFW::ovrRendererOutput &out);

    void ResetValues();

    float GetScreenScale();

    ovrLayerCylinder2 BuildSettingsCylinderLayer(ovrTextureSwapChain *cylinderSwapChain,
                                                 const int textureWidth, const int textureHeight,
                                                 const ovrTracking2 *tracking, bool followHead, float offsetY);

    ovrLayerCylinder2 BuildGameCylinderLayer3D(ovrTextureSwapChain *cylinderSwapChain,
                                               const int textureWidth, const int textureHeight,
                                               const ovrTracking2 *tracking, bool followHead,
                                               bool threedee, float threedeeoffset, float ipd);

    ovrMatrix4f CylinderModelMatrix(const int texHeight, const ovrVector3f translation, const float radius, const ovrQuatf *q, const float offsetY);
};