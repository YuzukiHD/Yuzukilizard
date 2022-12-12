TARGET_PATH :=$(call my-dir)
include $(ENV_CLEAR)

include $(TARGET_PATH)/../../../config/mpp_config.mk

ifeq ($(MPPCFG_COMPILE_DYNAMIC_LIB), Y)

TARGET_SRC := \
	cedarx_stream.c \
	cedarx_outstream.c \
	cedarx_outstream_external.c \
	cedarx_stream_file.c \

TARGET_INC := \
            $(TARGET_TOP)/system/public/include \
            $(TARGET_TOP)/middleware/config \
            $(TARGET_TOP)/middleware/include \
            $(TARGET_TOP)/middleware/include/utils \
            $(TARGET_TOP)/middleware/include/media \
            $(TARGET_TOP)/middleware/media/include/utils \
            $(TARGET_TOP)/middleware/media/LIBRARY \
            $(TARGET_TOP)/middleware/media/LIBRARY/include_stream

TARGET_SHARED_LIB := \
    liblog \
    libmedia_utils

TARGET_CPPFLAGS += -fPIC -Wall -Wno-unused-function
TARGET_CFLAGS += -fPIC -Wall -Wno-unused-function

TARGET_CPPFLAGS += $(CEDARX_EXT_CFLAGS)
TARGET_CFLAGS += $(CEDARX_EXT_CFLAGS)

TARGET_MODULE := libcedarxstream

include $(BUILD_SHARED_LIB)

endif


ifeq ($(MPPCFG_COMPILE_STATIC_LIB), Y)

include $(ENV_CLEAR)

TARGET_SRC := \
	cedarx_stream.c \
	cedarx_outstream.c \
	cedarx_outstream_external.c \
	cedarx_stream_file.c \

TARGET_INC := \
            $(TARGET_TOP)/system/public/include \
            $(TARGET_TOP)/middleware/config \
            $(TARGET_TOP)/middleware/include \
            $(TARGET_TOP)/middleware/include/utils \
            $(TARGET_TOP)/middleware/include/media \
            $(TARGET_TOP)/middleware/media/include/utils \
            $(TARGET_TOP)/middleware/media/LIBRARY \
            $(TARGET_TOP)/middleware/media/LIBRARY/include_stream

TARGET_SHARED_LIB := \
    liblog \
    libmedia_utils

TARGET_CPPFLAGS += -fPIC -Wall -Wno-unused-function
TARGET_CFLAGS += -fPIC -Wall -Wno-unused-function

TARGET_CPPFLAGS += $(CEDARX_EXT_CFLAGS)
TARGET_CFLAGS += $(CEDARX_EXT_CFLAGS)

TARGET_MODULE := libcedarxstream

include $(BUILD_STATIC_LIB)

endif

