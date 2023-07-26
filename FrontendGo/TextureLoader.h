#pragma once

#include <GLES3/gl3.h>
#include "OVR_FileSys.h"

namespace TextureLoader {

    GLuint CreateWhiteTexture();

    GLuint Load(OVRFW::ovrFileSys *fileSys, const char *path);

}  // namespace TextureLoader