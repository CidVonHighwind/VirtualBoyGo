#ifndef TEXTURELOADER_H
#define TEXTURELOADER_H

#include "App.h"

using namespace OVR;

namespace TextureLoader {

GLuint CreateWhiteTexture();

GLuint Load(App *app, const char *path);

}  // namespace TextureLoader

#endif