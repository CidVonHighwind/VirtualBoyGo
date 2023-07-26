#pragma once

#include <glm/glm.hpp>
#include "Appl.h"

using namespace OVRFW;

class DrawHelper {
private:
    GlProgram glTextureProgram;

    GLuint texture_vao;
    GLuint texture_vbo;

public:
    void Free();

    void Init(glm::mat4 projection);

    void DrawTexture(GLuint textureId, GLfloat posX, GLfloat posY, GLfloat width, GLfloat height, ovrVector4f color, float transparency);

};