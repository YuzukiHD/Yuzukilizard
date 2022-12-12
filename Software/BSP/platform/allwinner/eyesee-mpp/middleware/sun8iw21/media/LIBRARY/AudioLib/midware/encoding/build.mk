TARGET_PATH:= $(call my-dir)

include $(ENV_CLEAR)
include $(TARGET_TOP)/middleware/config/mpp_config.mk
	
ifeq ($(MPPCFG_COMPILE_DYNAMIC_LIB), Y)

TARGET_SRC := \
    aencoder.c \
    pcm_enc.c \

TARGET_INC := \
        $(TARGET_TOP)/system/include \
        $(TARGET_TOP)/middleware/include/utils \
        $(TARGET_TOP)/middleware/include/media \
        $(TARGET_TOP)/middleware/include \
        $(TARGET_TOP)/middleware/media/include/utils \
        $(TARGET_TOP)/middleware/media/LIBRARY/AudioLib/osal \
        $(TARGET_TOP)/middleware/media/LIBRARY/AudioLib/midware/encoding \
        $(TARGET_TOP)/middleware/media/LIBRARY/AudioLib/midware/encoding/include \

TARGET_CFLAGS += -fPIC -Wall -D__OS_LINUX

TARGET_STATIC_LIB := \
	libaacenc \
	libmp3enc \
	libadpcmenc \
	libg711enc \
	libg726enc \

TARGET_SHARED_LIB := \
	liblog

TARGET_MODULE:= libcedarx_aencoder

include $(BUILD_SHARED_LIB)

endif


ifeq ($(MPPCFG_COMPILE_STATIC_LIB), Y)

include $(ENV_CLEAR)

TARGET_SRC := \
    aencoder.c \
    pcm_enc.c \

TARGET_INC := \
        $(TARGET_TOP)/system/include \
        $(TARGET_TOP)/middleware/include/utils \
        $(TARGET_TOP)/middleware/include/media \
        $(TARGET_TOP)/middleware/include \
        $(TARGET_TOP)/middleware/media/include/utils \
        $(TARGET_TOP)/middleware/media/LIBRARY/AudioLib/osal \
        $(TARGET_TOP)/middleware/media/LIBRARY/AudioLib/midware/encoding \
        $(TARGET_TOP)/middleware/media/LIBRARY/AudioLib/midware/encoding/include \

TARGET_CFLAGS += -fPIC -Wall -D__OS_LINUX

TARGET_SHARED_LIB := \
	liblog

TARGET_MODULE:= libcedarx_aencoder

include $(BUILD_STATIC_LIB)

endif

