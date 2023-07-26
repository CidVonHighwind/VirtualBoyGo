LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE       := vbEmulator

CORE_DIR := $(LOCAL_PATH)/..

DEBUG                    := 0
FRONTEND_SUPPORTS_RGB565 := 1
NEED_BPP                 := 32
NEED_BLIP                := 1
IS_X86                   := 0
FLAGS                    :=

MEDNAFEN_DIR := ../mednafen
CORE_EMU_DIR := $(MEDNAFEN_DIR)/vb
LIBRETRO_COMM_DIR := $(CORE_DIR)/libretro-common

# $(FLAGS)
COREFLAGS := -funroll-loops -DMEDNAFEN_VERSION=\"0.9.26\" -DMEDNAFEN_VERSION_NUMERIC=926 -DPSS_STYLE=1 -D__LIBRETRO__ -D_LOW_ACCURACY_ -DINLINE="inline" -DWANT_8BPP
COREFLAGS += -DWANT_VB_EMU -DHAVE_STDINT_H=1

LOCAL_C_INCLUDES := \
	$(MEDNAFEN_DIR) \
	$(MEDNAFEN_DIR)/hw_cpu/v810 \
	$(MEDNAFEN_DIR)/hw_cpu/v810/fpu-new \
	$(MEDNAFEN_DIR)/include \
	$(MEDNAFEN_DIR)/include/blip \
	$(MEDNAFEN_DIR)/sound \
	$(MEDNAFEN_DIR)/vb \
	$(MEDNAFEN_DIR)/video
					
LOCAL_SRC_FILES := \
	$(CORE_EMU_DIR)/input.cpp \
	$(CORE_EMU_DIR)/timer.cpp \
	$(CORE_EMU_DIR)/vip.cpp \
	$(CORE_EMU_DIR)/vsu.cpp \
	$(MEDNAFEN_DIR)/hw_cpu/v810/v810_cpu.cpp \
	$(MEDNAFEN_DIR)/settings.cpp \
	$(MEDNAFEN_DIR)/state.cpp \
	$(MEDNAFEN_DIR)/mempatcher.cpp \
	$(MEDNAFEN_DIR)/video/surface.cpp \
	$(MEDNAFEN_DIR)/vrvb.cpp
	
LOCAL_SRC_FILES += \
	$(MEDNAFEN_DIR)/hw_cpu/v810/fpu-new/softfloat.c $(MEDNAFEN_DIR)/sound/Blip_Buffer.c
	
# LOCAL_SRC_FILES    := $(SOURCES_CXX) $(SOURCES_C)  
LOCAL_CFLAGS       := $(COREFLAGS) # -D__ANDROID_API__=$API # -D__GXX_EXPERIMENTAL_CXX0X__
# LOCAL_CXXFLAGS     := $(COREFLAGS)

# LOCAL_ALLOW_UNDEFINED_SYMBOLS := true
# LOCAL_LDFLAGS      := -Wl,-version-script=$(CORE_DIR)/link.T
# LOCAL_CPP_FEATURES := exceptions
# LOCAL_CPPFLAGS := -std=c++11

include $(BUILD_SHARED_LIBRARY)
