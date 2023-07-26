LOCAL_PATH := $(call my-dir)
PROJECT_ROOT:= $(call my-dir)/../../../..

include $(CLEAR_VARS)

include $(PROJECT_ROOT)/ovr_sdk_mobile_1.35.0/cflags.mk

#FIXUP: change LOCAL_MODULE to project name
LOCAL_MODULE			:= virtualboygo

LOCAL_C_INCLUDES := $(PROJECT_ROOT)/VirtualBoyGo/Src \
					$(PROJECT_ROOT)/ovr_sdk_mobile_1.35.0/VrSamples/SampleFramework/Src \
  					$(PROJECT_ROOT)/ovr_sdk_mobile_1.35.0/1stParty/OVR/Include \
  					$(PROJECT_ROOT)/ovr_sdk_mobile_1.35.0/1stParty/utilities/include \
  					$(PROJECT_ROOT)/ovr_sdk_mobile_1.35.0/3rdParty/stb/src \
					$(PROJECT_ROOT)/FrontendGo \
					$(PROJECT_ROOT)/FreeType/include/

LOCAL_SRC_FILES		:= 	main.cpp \
						Emulator.cpp \
						$(PROJECT_ROOT)/FrontendGo/TextureLoader.cpp \
						$(PROJECT_ROOT)/FrontendGo/Audio/OpenSLWrap.cpp \
						$(PROJECT_ROOT)/FrontendGo/LayerBuilder.cpp \
						$(PROJECT_ROOT)/FrontendGo/DrawHelper.cpp \
						$(PROJECT_ROOT)/FrontendGo/FontMaster.cpp \
						$(PROJECT_ROOT)/FrontendGo/MenuHelper.cpp \
						$(PROJECT_ROOT)/FrontendGo/Menu.cpp \
						$(PROJECT_ROOT)/FrontendGo/Global.cpp \

# include default libraries
LOCAL_LDLIBS 			:= -llog -landroid -lGLESv3 -lEGL -lOpenSLES
LOCAL_STATIC_LIBRARIES 	:= sampleframework freetype vbEmulator
LOCAL_SHARED_LIBRARIES	:= vrapi

include $(BUILD_SHARED_LIBRARY)

$(call import-add-path,$(PROJECT_ROOT))
$(call import-module,BeetleVBLibretroGo/jni)
$(call import-module,FreeType)

$(call import-add-path,$(PROJECT_ROOT)/ovr_sdk_mobile_1.35.0)
$(call import-module,VrSamples/SampleFramework/Projects/Android/jni)
$(call import-module,VrApi/Projects/AndroidPrebuilt/jni)
