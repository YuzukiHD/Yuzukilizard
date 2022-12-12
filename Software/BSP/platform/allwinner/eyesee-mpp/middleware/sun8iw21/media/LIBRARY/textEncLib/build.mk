TARGET_PATH:= $(call my-dir)

include $(ENV_CLEAR)
include $(TARGET_TOP)/middleware/config/mpp_config.mk
	
ifeq ($(MPPCFG_COMPILE_DYNAMIC_LIB), Y)

TARGET_SRC := \
	TextEncApi.c 

TARGET_INC := \
        $(TARGET_TOP)/system/include \
        $(TARGET_TOP)/middleware/include/utils \
        $(TARGET_TOP)/middleware/include/media \
        $(TARGET_TOP)/middleware/include \
        $(TARGET_TOP)/middleware/media/include/utils \
        $(TARGET_TOP)/middleware/media/LIBRARY/textEncLib/include \

TARGET_CFLAGS += -fPIC -Wall

TARGET_STATIC_LIB := 

TARGET_SHARED_LIB := \
	liblog

TARGET_MODULE:= libcedarx_tencoder

include $(BUILD_SHARED_LIB)

endif


ifeq ($(MPPCFG_COMPILE_STATIC_LIB), Y)

include $(ENV_CLEAR)

TARGET_SRC := \
	TextEncApi.c 

TARGET_INC := \
        $(TARGET_TOP)/system/include \
        $(TARGET_TOP)/middleware/include/utils \
        $(TARGET_TOP)/middleware/include/media \
        $(TARGET_TOP)/middleware/include \
        $(TARGET_TOP)/middleware/media/include/utils \
        $(TARGET_TOP)/middleware/media/LIBRARY/textEncLib/include \

TARGET_CFLAGS += -fPIC -Wall

TARGET_SHARED_LIB := \
	liblog

TARGET_MODULE:= libcedarx_tencoder

include $(BUILD_STATIC_LIB)

endif

