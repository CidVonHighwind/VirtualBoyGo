#include "TextureLoader.h"

#include <gli/texture.hpp>
#include <gli/load.hpp>

#include <OVR_LogUtils.h>

namespace TextureLoader {

    GLuint CreateWhiteTexture() {
        uint32_t white = 0xFFFFFFFF;
        GLuint textureId;

        glGenTextures(1, &textureId);
        glBindTexture(GL_TEXTURE_2D, textureId);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &white);
        glBindTexture(GL_TEXTURE_2D, 0);

        return textureId;
    }

    /// Filename can be KTX or DDS files
    GLuint Load_Texture(const void *Data, int Size) {
        OVR_LOG_WITH_TAG("TextureLoader", "loading Texture");

        gli::texture Texture = gli::load((const char *) Data, Size);
        if (Texture.empty()) {
            OVR_LOG_WITH_TAG("TextureLoader", "failed loading");
            return 0;
        }
        OVR_LOG_WITH_TAG("TextureLoader", "finished loading");

        gli::gl GL1(gli::gl::PROFILE_ES30);
        gli::gl::format const Format = GL1.translate(Texture.format(), Texture.swizzles());
        GLenum Target = GL1.translate(Texture.target());

        GLuint TextureName = 0;
        glGenTextures(1, &TextureName);
        glBindTexture(Target, TextureName);
        glTexParameteri(Target, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(Target, GL_TEXTURE_MAX_LEVEL, static_cast<GLint>(Texture.levels() - 1));
        glTexParameteri(Target, GL_TEXTURE_SWIZZLE_R, Format.Swizzles[0]);
        glTexParameteri(Target, GL_TEXTURE_SWIZZLE_G, Format.Swizzles[1]);
        glTexParameteri(Target, GL_TEXTURE_SWIZZLE_B, Format.Swizzles[2]);
        glTexParameteri(Target, GL_TEXTURE_SWIZZLE_A, Format.Swizzles[3]);

        glTexParameteri(Target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(Target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        glm::tvec3<GLsizei> const Extent(Texture.extent());
        auto const FaceTotal = static_cast<GLsizei>(Texture.layers() * Texture.faces());

        glTexStorage2D(Target, static_cast<GLint>(Texture.levels()), Format.Internal, Extent.x, Texture.target() == gli::TARGET_2D ? Extent.y : FaceTotal);

        for (std::size_t Layer = 0; Layer < Texture.layers(); ++Layer)
            for (std::size_t Face = 0; Face < Texture.faces(); ++Face)
                for (std::size_t Level = 0; Level < Texture.levels(); ++Level) {
                    auto const LayerGL = static_cast<GLsizei>(Layer);
                    glm::tvec3<GLsizei> LevelExtent(Texture.extent(Level));
                    Target = gli::is_target_cube(Texture.target()) ? static_cast<GLenum>(GL_TEXTURE_CUBE_MAP_POSITIVE_X + Face) : Target;

                    if (gli::is_compressed(Texture.format())) {
                        if (Texture.target() == gli::TARGET_1D_ARRAY)
                            OVR_LOG_WITH_TAG("TextureLoader", "TARGET_1D");
                        if (Texture.target() == gli::TARGET_2D)
                            OVR_LOG_WITH_TAG("TextureLoader", "TARGET_2D");

                        glCompressedTexSubImage2D(Target, static_cast<GLint>(Level), 0, 0, LevelExtent.x,
                                                  Texture.target() == gli::TARGET_1D_ARRAY ? LayerGL : LevelExtent.y,
                                                  Format.Internal, static_cast<GLsizei>(Texture.size(Level)),
                                                  Texture.data(Layer, Face, Level));
                    } else {
                        glTexSubImage2D(Target, static_cast<GLint>(Level), 0, 0, LevelExtent.x,
                                        Texture.target() == gli::TARGET_1D_ARRAY ? LayerGL : LevelExtent.y,
                                        Format.External, Format.Type, Texture.data(Layer, Face, Level));
                    }
                }

        return TextureName;
    }

    GLuint Load(OVRFW::ovrFileSys *fileSys, const char *path) {
        std::vector<uint8_t> buffer;
        if (fileSys->ReadFile(path, buffer))
            return Load_Texture(buffer.data(), buffer.size());

        return 0;
    }

}  // namespace TextureLoader