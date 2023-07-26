#include "DrawHelper.h"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <OVR_LogUtils.h>

using namespace OVRFW;

const char VERTEX_SHADER_TEXTURE[] =
        "#version 330 core\n"
        "layout (location = 0) in vec4 vertex; // <vec2 pos, vec2 tex>\n"

        "out vec2 TexCoords;\n"

        "uniform mat4 projection;\n"

        "void main()\n"
        "{\n"
        "	gl_Position = projection * vec4(vertex.xy, 0.0, 1.0);\n"
        "	TexCoords = vertex.zw;\n"
        "}\n";

const char FRAGMENT_SHADER_TEXTURE[] =
        "#version 330 core\n"
        "in vec2 TexCoords;\n"

        "out vec4 color;\n"

        "uniform sampler2D text;\n"
        "uniform vec4 textColor;\n"

        "void main()\n"
        "{\n"
        "	vec4 tex_sample = texture(text, TexCoords);\n"
        "	color = tex_sample * textColor * tex_sample.a;\n"
        "}\n";

void DrawHelper::Free() {
    OVR_LOG("draw helper destruction");

    GlProgram::Free(glTextureProgram);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    glDeleteVertexArrays(1, &texture_vao);
    glDeleteBuffers(1, &texture_vbo);
}

void DrawHelper::Init(glm::mat4 projection) {
    glTextureProgram = GlProgram::Build(VERTEX_SHADER_TEXTURE, FRAGMENT_SHADER_TEXTURE, NULL, 0);

    glUseProgram(glTextureProgram.Program);
    glUniformMatrix4fv(glGetUniformLocation(glTextureProgram.Program, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

    // Create Vertex Array Object
    glGenVertexArrays(1, &texture_vao);
    glBindVertexArray(texture_vao);
    // Create a Vertex Buffer Object and copy the vertex data to it
    glGenBuffers(1, &texture_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, texture_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 6 * 4, NULL, GL_DYNAMIC_DRAW);
    // Specify the layout of the vertex data
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), 0);
}

void DrawHelper::DrawTexture(GLuint textureId, GLfloat posX, GLfloat posY, GLfloat width, GLfloat height, ovrVector4f color, float transparency) {
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(texture_vao);

    glUseProgram(glTextureProgram.Program);
    glBindTexture(GL_TEXTURE_2D, textureId);

    GLfloat charVertices[6][4] = {
            {posX,         posY + height, 0.0f, 1.0f},
            {posX,         posY,          0.0f, 0.0f},
            {posX + width, posY,          1.0f, 0.0f},
            {posX,         posY + height, 0.0f, 1.0f},
            {posX + width, posY,          1.0f, 0.0f},
            {posX + width, posY + height, 1.0f, 1.0f}};

    glBindBuffer(GL_ARRAY_BUFFER, texture_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(charVertices), charVertices);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glUniform4f(glGetUniformLocation(glTextureProgram.Program, "textColor"),
                color.x * transparency, color.y * transparency, color.z * transparency, color.w * transparency);

    // Draw a triangle from the 3 vertices
    glDrawArrays(GL_TRIANGLES, 0, 6);
}
