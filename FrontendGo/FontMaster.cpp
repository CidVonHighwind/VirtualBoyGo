#include "FontMaster.h"

#include <FreeType/src/psaux/pstypes.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <OVR_FileSys.h>
#include <OVR_LogUtils.h>

const char VERTEX_SHADER[] =
        "#version 330 core\n"
        "layout (location = 0) in vec4 vertex; // <vec2 pos, vec2 tex>\n"
        "out vec2 TexCoords;\n"
        "uniform mat4 projection;\n"

        "void main()\n"
        "{\n"
        "	gl_Position = projection * vec4(vertex.xy, 0.0, 1.0);\n"
        "	TexCoords = vertex.zw;\n"
        "}\n";

const char FRAGMENT_SHADER[] =
        "#version 330 core\n"
        "in vec2 TexCoords;\n"

        "out vec4 color;\n"

        "uniform sampler2D text;\n"
        "uniform vec4 textColor;\n"

        "void main()\n"
        "{\n"
        "	color = textColor * texture(text, TexCoords).a;\n"
        "}\n";

FT_Library FontManager::ft;

FontManager::FontManager() = default;

void FontManager::Free() {
    OVR_LOG("font master destruction");

    OVRFW::GlProgram::Free(glProgram);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
}

void FontManager::Init(glm::mat4 projection) {
    if (FT_Init_FreeType(&ft))
        OVR_LOG_WITH_TAG("FontMaster", "ERROR::FREETYPE: Could not init FreeType Library");

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 6 * 4 * 100, NULL, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // create the rendering program and set the projection matrix
    glProgram = OVRFW::GlProgram::Build(VERTEX_SHADER, FRAGMENT_SHADER, NULL, 0);
    glUseProgram(glProgram.Program);
    glUniformMatrix4fv(glGetUniformLocation(glProgram.Program, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

    OVR_LOG_WITH_TAG("FontMaster", "Initialized");
}

void FontManager::LoadFont(FontManager::RenderFont *font, FT_Face face, uint fontSize) {
    font->FontSize = fontSize;

    FT_Set_Pixel_Sizes(face, 0, fontSize);

    // Disable byte-alignment restriction
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    // create a texture for the characters
    int textureWidth = 30 * fontSize;
    int textureHeight = 8 * fontSize;
    glGenTextures(1, &font->textureID);
    glBindTexture(GL_TEXTURE_2D, font->textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, textureWidth, textureHeight, 0, GL_ALPHA, GL_UNSIGNED_BYTE, NULL);

    int posX = 1;
    int posY = 1;

    for (GLubyte c = 32; c < 192; c++) {
        // Load character glyph
        if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
            // std::cout << "ERROR::FREETYTPE: Failed to load Glyph" << std::endl;
            continue;
        }

        float descent = 0.0f;
        if (descent < (face->glyph->bitmap.rows - face->glyph->bitmap_top)) {
            descent = face->glyph->bitmap.rows - face->glyph->bitmap_top;
        }

        float ascent_calc = 0.0f;
        if (face->glyph->bitmap_top < face->glyph->bitmap.rows) {
            ascent_calc = face->glyph->bitmap.rows;
        } else {
            ascent_calc = face->glyph->bitmap_top;
        }
        float ascent = 0.0f;
        if (ascent < (ascent_calc - descent)) {
            ascent = ascent_calc - descent;
        }
        if (font->offsetY < (int) ascent) {
            OVR_LOG("new ascent %i to %f", font->offsetY, ascent);
            font->offsetY = (int) ascent;
        }

        // go to the next line
        if (posX + face->glyph->bitmap.width > textureWidth) {
            posX = 0;
            posY += fontSize + fontSize * 1 / 2.0f;
        }

        glTexSubImage2D(GL_TEXTURE_2D, 0, posX, posY, face->glyph->bitmap.width,
                        face->glyph->bitmap.rows, GL_ALPHA, GL_UNSIGNED_BYTE,
                        face->glyph->bitmap.buffer);

        // Now store character for later use
        FontManager::Character character = {glm::fvec4((float) posX / textureWidth, (float) posY / textureHeight,
                                                       (float) (posX + face->glyph->bitmap.width) / textureWidth,
                                                       (float) (posY + face->glyph->bitmap.rows) / textureHeight),
                                            glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows),
                                            glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top),
                                            ((GLuint) face->glyph->advance.x >> 6)};
        font->Characters.insert(std::pair<GLchar, FontManager::Character>(c, character));

        posX += face->glyph->bitmap.width + 2;
    }

    auto ch = font->Characters['P'];
    font->PHeight = ch.Size.y;
    font->PStart = font->offsetY - ch.Bearing.y;
    OVR_LOG_WITH_TAG("FontMaster", "offset: %i, height: %i, start: %i", font->offsetY, font->PHeight, font->PStart);

    // Set texture options
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindTexture(GL_TEXTURE_2D, 0);

    FT_Done_Face(face);

    OVR_LOG_WITH_TAG("FontMaster", "finished loading font");
}

void FontManager::LoadFontFromAssets(OVRFW::ovrFileSys *fileSys, RenderFont *font, const char *filePath, uint fontSize) {
    OVR_LOG_WITH_TAG("FontMaster", "start loading font from assets");
    std::vector<uint8_t> buffer;

    FT_Face face;
    if (fileSys->ReadFile(filePath, buffer))
        if (FT_New_Memory_Face(ft, buffer.data(), buffer.size(), 0, &face)) {
            OVR_LOG_WITH_TAG("FontMaster", "ERRO::FREETYPE: Faild to load font from memory");
        } else {
            LoadFont(font, face, fontSize);
        }
}

void FontManager::LoadFont(RenderFont *font, const char *filePath, uint fontSize) {
    OVR_LOG_WITH_TAG("FontMaster", "start loading font from android system");

    FT_Face face;
    if (FT_New_Face(ft, filePath, 0, &face))
        OVR_LOG_WITH_TAG("FontMaster", "ERROR::FREETYPE: Failed to load font");

    LoadFont(font, face, fontSize);
}

void FontManager::CloseFontLoader() { FT_Done_FreeType(ft); }

int FontManager::GetWidth(RenderFont font, std::string text) {
    int width = 0;

    // Iterate through all characters
    std::string::const_iterator c;
    for (c = text.begin(); c != text.end(); c++) {
        Character ch = font.Characters[*c];
        width += (ch.Advance);
    }

    return width;
}

void FontManager::Begin() {
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);

    glActiveTexture(GL_TEXTURE0);

    glBindVertexArray(VAO);

    glUseProgram(glProgram.Program);
}

void FontManager::End() {
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void FontManager::RenderText(RenderFont font, std::string text, GLfloat x, GLfloat y, GLfloat scale,
                             ovrVector4f color, float transparency) {
    // Iterate through all characters
    std::string::const_iterator c;
    int index = 0;
    for (c = text.begin(); c != text.end(); c++) {
        Character ch = font.Characters[*c];

        GLfloat xpos = x + ch.Bearing.x * scale;
        GLfloat ypos = y + font.offsetY - ch.Bearing.y;  // font.FontSize;// - (ch.Bearing.y) * scale;

        GLfloat w = ch.Size.x * scale;
        GLfloat h = ch.Size.y * scale;
        // Update VBO for each character

        GLfloat charVertices[6][4] = {{xpos,     ypos + h, ch.Position.x, ch.Position.w},
                                      {xpos,     ypos,     ch.Position.x, ch.Position.y},
                                      {xpos + w, ypos,     ch.Position.z, ch.Position.y},

                                      {xpos,     ypos + h, ch.Position.x, ch.Position.w},
                                      {xpos + w, ypos,     ch.Position.z, ch.Position.y},
                                      {xpos + w, ypos + h, ch.Position.z, ch.Position.w}};

        memcpy(&vertices[6 * index], &charVertices, sizeof(charVertices));

        // Now advance cursors for next glyph (note that advance is number of 1/64 pixels)
        x += (ch.Advance) * scale;  // Bitshift by 6 to get value in pixels (2^6 = 64)
        index++;
    }

    // Activate corresponding render state
    glUniform4f(glGetUniformLocation(glProgram.Program, "textColor"), color.x * transparency,
                color.y * transparency, color.z * transparency, color.w * transparency);

    glBindTexture(GL_TEXTURE_2D, font.textureID);

    // Update content of VBO memory
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    // Render quad
    // .length() will not work when a char is bigger than 1 byte
    glDrawArrays(GL_TRIANGLES, 0, 6 * text.length());
}

void FontManager::RenderFontTexture(FontManager::RenderFont font, ovrVector4f color, float transparency) {
    GLfloat posX = 0.0f;
    GLfloat posY = 200.0f;
    GLfloat posZ = posX + font.FontSize * 30.0f;
    GLfloat posW = posY + font.FontSize * 8.0f;

    // Update VBO for each character
    GLfloat charVertices[6][4] = {{posX, posW, 0, 1},
                                  {posX, posY, 0, 0},
                                  {posZ, posY, 1, 0},

                                  {posX, posW, 0, 1},
                                  {posZ, posY, 1, 0},
                                  {posZ, posW, 1, 1}};

    // Activate corresponding render state
    glUniform4f(glGetUniformLocation(glProgram.Program, "textColor"),
                color.x * transparency, color.y * transparency, color.z * transparency, color.w * transparency);

    glBindTexture(GL_TEXTURE_2D, font.textureID);

    // Update content of VBO memory
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(charVertices), charVertices);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    // Render quad
    glDrawArrays(GL_TRIANGLES, 0, 6);
}
