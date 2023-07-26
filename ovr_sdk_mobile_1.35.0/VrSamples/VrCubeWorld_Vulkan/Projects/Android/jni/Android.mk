
LOCAL_PATH:= $(call my-dir)

#--------------------------------------------------------
# libvrcubeworldvk.so
#--------------------------------------------------------
include $(CLEAR_VARS)

LOCAL_MODULE			:= vrcubeworldvk
LOCAL_CFLAGS			:= -std=c99 -Werror

LOCAL_SRC_FILES			:= 	../../../Src/VrCubeWorld_Vulkan.c \
							../../../Src/Framework_Vulkan.c

LOCAL_LDLIBS			:= -llog -landroid

LOCAL_LDFLAGS			:= -u ANativeActivity_onCreate

LOCAL_C_INCLUDES        := $(LOCAL_PATH)/../../../../../3rdParty/khronos/vulkan_1.1.100.0

LOCAL_STATIC_LIBRARIES	:= android_native_app_glue

LOCAL_SHARED_LIBRARIES	:= vrapi

include $(BUILD_SHARED_LIBRARY)

$(call import-module,android/native_app_glue)
$(call import-module,VrApi/Projects/AndroidPrebuilt/jni)
