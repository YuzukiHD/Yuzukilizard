TARGET_PATH:= $(call my-dir)

include $(ENV_CLEAR)

include $(TARGET_TOP)/middleware/config/mpp_config.mk

ifeq ($(MPPCFG_COMPILE_DYNAMIC_LIB), Y)

include $(ENV_CLEAR)
#include $(TARGET_PATH)/../libcedarx/config.mk

TARGET_SRC := \
    adecoder.c \
    src/AudioDec_Decode.c \
    src/cedar_abs_packet_hdr.c

TARGET_INC := \
        $(TARGET_PATH)/ \
        $(TARGET_PATH)/include \
        $(TARGET_TOP)/middleware/include/utils \
        $(TARGET_TOP)/system/public/include/cutils \
        $(TARGET_TOP)/middleware/media/LIBRARY/AudioLib/osal \

TARGET_CFLAGS += -fPIC -Wall -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-function -D__OS_LINUX

TARGET_SHARED_LIB := \
	liblog

TARGET_MODULE:= libadecoder

include $(BUILD_SHARED_LIB)

endif

#####################compile static lib#########################
ifeq ($(MPPCFG_COMPILE_STATIC_LIB), Y)

include $(ENV_CLEAR)
#include $(TARGET_PATH)/../libcedarx/config.mk

TARGET_SRC := \
    adecoder.c \
    src/AudioDec_Decode.c \
    src/cedar_abs_packet_hdr.c

TARGET_INC := \
        $(TARGET_PATH)/ \
        $(TARGET_PATH)/include \
        $(TARGET_TOP)/middleware/include/utils \
        $(TARGET_TOP)/system/public/include/cutils \
        $(TARGET_TOP)/middleware/media/LIBRARY/AudioLib/osal \

TARGET_CFLAGS += -fPIC -Wall -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-function -D__OS_LINUX

TARGET_SHARED_LIB := \
	liblog

TARGET_MODULE:= libadecoder

include $(BUILD_STATIC_LIB)

endif
