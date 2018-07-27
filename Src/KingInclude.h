#ifndef ANDROID_KINGINCLUDE_H
#define ANDROID_KINGINCLUDE_H

// TODO: figure out why "SceneView.h" can't be at the bottom of this file
#include "SceneView.h"
#include <OVR_FileSys.h>
#include <VRMenu.h>
#include <VrApi_Input.h>
#include <VrApi_Types.h>
#include <dirent.h>
#include <ft2build.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <gli/convert.hpp>    // convert a texture from a format to another
#include <gli/duplicate.hpp>  // duplicate the data of a texture, allocating a new texture storage
#include <gli/dx.hpp>         // facilitate the use of GLI with Direct3D API
#include <gli/format.hpp>     // list of the supported formats
#include <gli/generate_mipmaps.hpp>  // generating the mipmaps of a texture
#include <gli/gl.hpp>                // translate GLI enums to OpenGL enums
#include <gli/gli.hpp>
#include <gli/image.hpp>  // use images, a representation of a single texture level.
#include <gli/levels.hpp>  // compute the number of mipmaps levels necessary to create a mipmap complete texture.
#include <gli/load.hpp>                // load DDS, KTX or KMG textures from files or memory.
#include <gli/load_dds.hpp>            // load DDS textures from files or memory.
#include <gli/reduce.hpp>              // include to perform reduction operations.
#include <gli/sampler.hpp>             // enumations for texture sampling
#include <gli/sampler1d.hpp>           // class to sample 1d texture
#include <gli/sampler1d_array.hpp>     // class to sample 1d array texture
#include <gli/sampler2d.hpp>           // class to sample 2d texture
#include <gli/sampler2d_array.hpp>     // class to sample 2d array texture
#include <gli/sampler_cube.hpp>        // class to sample cube texture
#include <gli/sampler_cube_array.hpp>  // class to sample cube array texture
#include <gli/target.hpp>              // helper function to query property of a generic texture
#include <gli/texture.hpp>          // generic texture class that can represent any kind of texture
#include <gli/texture2d.hpp>        // representation of a 2d texture
#include <gli/texture2d_array.hpp>  // representation of a 2d array texture
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <map>
#include <string>

#include "DrawHelper.h"
#include "Global.h"
#include "MenuHelper.h"
#include "FontMaster.h"
#include "GuiSys.h"
#include "LayerBuilder.h"
#include "TextureLoader.h"
#include "OVR_Locale.h"
#include "SoundEffectContext.h"
#include "Audio/OpenSLWrap.h"
#include FT_FREETYPE_H

#endif  // ANDROID_KINGINCLUDE_H
