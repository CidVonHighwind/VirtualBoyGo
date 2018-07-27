#ifndef DRAW_HELPER_H
#define DRAW_HELPER_H

#include "App.h"

using namespace OVR;

namespace DrawHelper {

void Init();

void DrawTexture(GLuint textureId, GLfloat posX, GLfloat posY, GLfloat width, GLfloat height,
                 ovrVector4f color, float transparency);

}  // namespace DrawHelper

#endif