#pragma once

#include <glm/glm.hpp>
#include <map>
#include <string>
#include <FreeType/src/psaux/pstypes.h>
#include "Appl.h"

class FontManager {
private:
    OVRFW::GlProgram glProgram;

    GLuint VAO;
    GLuint VBO;

    GLfloat vertices[6 * 100][4];

public:
    static FT_Library ft;

    struct Character {
        glm::fvec4 Position;  // the position of the character inside of the texture
        glm::ivec2 Size;      // size of glyph
        glm::ivec2 Bearing;   // offset from baseline to left/top of glyph
        GLuint Advance;       // offset to advance to next glyph
    };

    struct RenderFont {
        int FontSize;
        int offsetY;
        int PHeight;
        int PStart;
        GLuint textureID;
        std::map<GLchar, Character> Characters;
    };

    FontManager();
    void Free();

    FontManager(FontManager&) = delete;
    FontManager(FontManager&&) = delete;

    FontManager &operator=(FontManager&) = delete;
    FontManager &operator=(FontManager&&) = delete;

    void Init(glm::mat4 projection);

    static void CloseFontLoader();

    void Begin();

    void RenderText(RenderFont font, std::string text, GLfloat x, GLfloat y, GLfloat scale, ovrVector4f color, float transparency);

    void RenderFontTexture(FontManager::RenderFont font, ovrVector4f color, float transparency);

    void End();

    static int GetWidth(RenderFont font, std::string text);

    static void LoadFont(RenderFont *font, const char *filePath, uint fontSize);

    static void LoadFontFromAssets(OVRFW::ovrFileSys *fileSys, RenderFont *font, const char *filePath, uint fontSize);

    static void LoadFont(RenderFont *font, FT_Face face, uint fontSize);

};