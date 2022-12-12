TARGET_PATH :=$(call my-dir)
include $(ENV_CLEAR)

include $(TARGET_TOP)/middleware/config/mpp_config.mk

TARGET_MODULE :=
TARGET_PREBUILT_LIBS := $(wildcard $(TARGET_PATH)/lib/*.so)

ifeq ($(MPPCFG_COMPILE_STATIC_LIB), Y)
TARGET_PREBUILT_LIBS := $(wildcard $(TARGET_PATH)/lib/*.a)
endif

include $(BUILD_MULTI_PREBUILT)
