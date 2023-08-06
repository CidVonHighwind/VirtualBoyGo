LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

include ../../../../cflags.mk


LOCAL_MODULE := virtualboygo

LOCAL_C_INCLUDES := $(LOCAL_PATH)/../../../../ \
					$(LOCAL_PATH)/../../../../VirtualBoyGo/Src \
					$(LOCAL_PATH)/../../../../../SampleCommon/Src \
					$(LOCAL_PATH)/../../../../../SampleFramework/Src \
					$(LOCAL_PATH)/../../../../../1stParty/OVR/Include \
					$(LOCAL_PATH)/../../../../../1stParty/utilities/include \
					$(LOCAL_PATH)/../../../../../3rdParty/stb/src \
					$(LOCAL_PATH)/../../../../FrontendGo \
					$(LOCAL_PATH)/../../../../FreeType/include/

LOCAL_SRC_FILES		:=	../../../Src/main.cpp \
						../../../Src/Emulator.cpp \
						../../../../FrontendGo/TextureLoader.cpp \
						../../../../FrontendGo/Audio/OpenSLWrap.cpp \
						../../../../FrontendGo/LayerBuilder.cpp \
						../../../../FrontendGo/DrawHelper.cpp \
						../../../../FrontendGo/FontMaster.cpp \
						../../../../FrontendGo/MenuHelper.cpp \
						../../../../FrontendGo/Menu.cpp \
						../../../../FrontendGo/Global.cpp \

LOCAL_LDLIBS := -lEGL -lGLESv3 -landroid -llog -lOpenSLES

LOCAL_STATIC_LIBRARIES := sampleframework freetype vbEmulator
LOCAL_SHARED_LIBRARIES := vrapi

include $(BUILD_SHARED_LIBRARY)

$(call import-module,VBGo/BeetleVBLibretroGo/jni)
$(call import-module,VBGo/FreeType)

$(call import-module,VrApi/Projects/AndroidPrebuilt/jni)
$(call import-module,SampleFramework/Projects/Android/jni)
