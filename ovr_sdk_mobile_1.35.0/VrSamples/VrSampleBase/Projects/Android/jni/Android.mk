LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

include ../../../../cflags.mk

#FIXUP: change LOCAL_MODULE to project name
LOCAL_MODULE			:= vrsamplebase

LOCAL_C_INCLUDES := $(LOCAL_PATH)/../../../../SampleFramework/Src \
  					$(LOCAL_PATH)/../../../../../1stParty/OVR/Include \
  					$(LOCAL_PATH)/../../../../../1stParty/utilities/include \
  					$(LOCAL_PATH)/../../../../../3rdParty/stb/src \

LOCAL_SRC_FILES		:= 	../../../Src/main.cpp \

# include default libraries
LOCAL_LDLIBS 			:= -llog -landroid -lGLESv3 -lEGL
LOCAL_STATIC_LIBRARIES 	:= sampleframework
LOCAL_SHARED_LIBRARIES	:= vrapi

include $(BUILD_SHARED_LIBRARY)

$(call import-module,VrSamples/SampleFramework/Projects/Android/jni)
$(call import-module,VrApi/Projects/AndroidPrebuilt/jni)
