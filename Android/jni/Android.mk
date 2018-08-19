LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

include ../../../cflags.mk

LOCAL_MODULE			:= ovrapp
LOCAL_SRC_FILES			:= 	../../../FrontendGo/OvrApp.cpp \
							../../../FrontendGo/TextureLoader.cpp \
							../../../FrontendGo/Audio/OpenSLWrap.cpp \
							../../../FrontendGo/LayerBuilder.cpp \
							../../../FrontendGo/DrawHelper.cpp \
							../../../FrontendGo/FontMaster.cpp \
							../../../FrontendGo/MenuHelper.cpp \
							../../../FrontendGo/Menu.cpp \
							../../Src/Emulator.cpp
							
LOCAL_STATIC_LIBRARIES	:= vrsound vrmodel vrlocale vrgui vrappframework libovrkernel freetype vbEmulator
LOCAL_SHARED_LIBRARIES	:= vrapi

LOCAL_LDLIBS    += -lOpenSLES

APP_STL := gnustl_static
LOCAL_C_INCLUDES := ../Src/ ../../../FreeType ../../../beetle-vb-libretro/mednafen ../../../
LOCAL_C_INCLUDES += ../../FrontendGo

include $(BUILD_SHARED_LIBRARY)

$(call import-module,beetle-vb-libretro/jni)
$(call import-module,FreeType)

$(call import-module,LibOVRKernel/Projects/Android/jni)
$(call import-module,VrApi/Projects/AndroidPrebuilt/jni)
$(call import-module,VrAppFramework/Projects/Android/jni)
$(call import-module,VrAppSupport/VrGUI/Projects/Android/jni)
$(call import-module,VrAppSupport/VrLocale/Projects/Android/jni)
$(call import-module,VrAppSupport/VrModel/Projects/Android/jni)
$(call import-module,VrAppSupport/VrSound/Projects/Android/jni)
