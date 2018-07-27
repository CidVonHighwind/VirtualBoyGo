#ifndef LAYERBUILDER_H
#define LAYERBUILDER_H

#include "App.h"

using namespace OVR;

namespace LayerBuilder {

extern float screenPitch, screenYaw, screenRoll, radiusMenuScreen, screenSize;

void UpdateDirection(const ovrFrameInput &vrFrame);

void ResetValues();

ovrLayerCylinder2 BuildSettingsCylinderLayer(ovrTextureSwapChain *cylinderSwapChain,
                                             const int textureWidth, const int textureHeight,
                                             const ovrTracking2 *tracking, bool followHead,
                                             float offsetY);

ovrLayerCylinder2 BuildGameCylinderLayer(ovrTextureSwapChain *cylinderSwapChain,
                                         const int textureWidth, const int textureHeight,
                                         const ovrTracking2 *tracking, bool followHead);

ovrLayerCylinder2 BuildGameCylinderLayer3D(ovrTextureSwapChain *cylinderSwapChain,
                                          const int textureWidth, const int textureHeight,
                                          const ovrTracking2 *tracking, bool followHead, bool threedee);
}  // namespace LayerBuilder

#endif