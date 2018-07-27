LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

include ../../../cflags.mk

LOCAL_MODULE			:= ovrapp
LOCAL_SRC_FILES			:= 	../../Src/OvrApp.cpp \
							../../Src/DrawHelper.cpp \
							../../Src/FontMaster.cpp \
							../../Src/TextureLoader.cpp \
							../../Src/MenuHelper.cpp \
							../../Src/Audio/OpenSLWrap.cpp \
							../../Src/LayerBuilder.cpp \
							../../Src/GBEmulator.cpp \
							../../Src/VBEmulator.cpp
							
LOCAL_STATIC_LIBRARIES	:= vrsound vrmodel vrlocale vrgui vrappframework libovrkernel freetype gearboy vbEmulator
LOCAL_SHARED_LIBRARIES	:= vrapi

LOCAL_ALLOW_UNDEFINED_SYMBOLS := true

# for native audio
LOCAL_LDLIBS    += -lOpenSLES

APP_STL := gnustl_static
LOCAL_C_INCLUDES := ../../../FreeType ../../../Gearboy ../../../beetle-vb-libretro/mednafen ../../../

LOCAL_CFLAGS	+= -Wno-error
LOCAL_CFLAGS	+= -Wno-format
LOCAL_CFLAGS	+= -Wno-unused-variable
LOCAL_CFLAGS	+= -Wno-ignored-qualifiers
LOCAL_CFLAGS	+= -frtti
LOCAL_CFLAGS	+= -Wno-sign-compare
LOCAL_CFLAGS 	+= -Wno-unused-function

CFLAGS			+= -Wall
CFLAGS			+= -mword-relocations
CFLAGS			+= -mtune=mpcore
CFLAGS			+= -mfloat-abi=hard
CFLAGS			+= -fomit-frame-pointer
CFLAGS			+= -ffast-math

include $(BUILD_SHARED_LIBRARY)

$(call import-module,beetle-vb-libretro/jni)
$(call import-module,FreeType)
$(call import-module,Gearboy/jni)
$(call import-module,LibOVRKernel/Projects/Android/jni)
$(call import-module,VrApi/Projects/AndroidPrebuilt/jni)
$(call import-module,VrAppFramework/Projects/Android/jni)
$(call import-module,VrAppSupport/VrGUI/Projects/Android/jni)
$(call import-module,VrAppSupport/VrLocale/Projects/Android/jni)
$(call import-module,VrAppSupport/VrModel/Projects/Android/jni)
$(call import-module,VrAppSupport/VrSound/Projects/Android/jni)