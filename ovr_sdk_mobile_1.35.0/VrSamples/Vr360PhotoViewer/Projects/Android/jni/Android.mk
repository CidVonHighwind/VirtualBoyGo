LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)					# clean everything up to prepare for a module

include $(LOCAL_PATH)/../../../../../cflags.mk

LOCAL_MODULE		:= vr360photoviewer

LOCAL_C_INCLUDES 	:= 	$(LOCAL_PATH)/../../../../SampleFramework/Src \
						$(LOCAL_PATH)/../../../../../VrApi/Include \
						$(LOCAL_PATH)/../../../../../1stParty/OVR/Include \
						$(LOCAL_PATH)/../../../../../1stParty/utilities/include \
						$(LOCAL_PATH)/../../../../../3rdParty/stb/src \

LOCAL_SRC_FILES		:= 	../../../Src/main.cpp \
					    ../../../Src/Vr360PhotoViewer.cpp \

# include default libraries
LOCAL_LDLIBS 			:= -llog -landroid -lGLESv3 -lEGL -lz
LOCAL_STATIC_LIBRARIES 	:= sampleframework jpeg
LOCAL_SHARED_LIBRARIES	:= vrapi

include $(BUILD_SHARED_LIBRARY)

$(call import-module,VrSamples/SampleFramework/Projects/Android/jni)
$(call import-module,VrApi/Projects/AndroidPrebuilt/jni)
