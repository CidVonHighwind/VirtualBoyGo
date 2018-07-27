LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

include ../../../cflags.mk

LOCAL_MODULE			:= ovrapp
LOCAL_SRC_FILES			:= 	../../Src/OvrApp.cpp \
							../../Src/TextureLoader.cpp \
							../../Src/Audio/OpenSLWrap.cpp \
							../../Src/LayerBuilder.cpp \
							../../Src/DrawHelper.cpp \
							../../Src/FontMaster.cpp \
							../../Src/MenuHelper.cpp \
							../../Src/GBEmulator.cpp \
							../../Src/VBEmulator.cpp
							
							
LOCAL_STATIC_LIBRARIES	:= vrsound vrmodel vrlocale vrgui vrappframework libovrkernel freetype gearboy vbEmulator
LOCAL_SHARED_LIBRARIES	:= vrapi

LOCAL_LDLIBS    += -lOpenSLES

APP_STL := gnustl_static
LOCAL_C_INCLUDES := ../../../FreeType ../../../Gearboy ../../../beetle-vb-libretro/mednafen ../../../

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
