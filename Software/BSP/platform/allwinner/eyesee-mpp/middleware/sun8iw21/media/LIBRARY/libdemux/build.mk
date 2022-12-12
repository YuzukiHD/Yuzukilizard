TARGET_PATH :=$(call my-dir)
include $(ENV_CLEAR)

include $(TARGET_TOP)/middleware/media/LIBRARY/libcedarx/config.mk

TARGET_SRC := \
	cedarx_demux.c \
	aw_demux.cpp \
	DemuxTypeMap.c

TARGET_INC := \
            $(TARGET_TOP)/middleware/include \
            $(TARGET_TOP)/middleware/include/media \
            $(TARGET_TOP)/middleware/include/utils \
            $(TARGET_TOP)/middleware/media/include \
            $(TARGET_TOP)/middleware/media/include/component \
            $(TARGET_TOP)/middleware/media/LIBRARY/libdemux \
            $(TARGET_TOP)/middleware/media/LIBRARY/include_stream \
            $(TARGET_TOP)/middleware/media/LIBRARY/include_demux \
            $(TARGET_TOP)/middleware/media/LIBRARY/include_utils \
            $(TARGET_TOP)/middleware/media/LIBRARY/libcedarx \
            $(TARGET_TOP)/middleware/media/LIBRARY/libcedarx/libcore/include \
            $(TARGET_TOP)/middleware/media/LIBRARY/libcedarx/libcore/parser/include \
            $(TARGET_TOP)/middleware/media/LIBRARY/libcedarx/libcore/stream/include \
            $(TARGET_TOP)/middleware/media/LIBRARY/libcedarx/libcore/base/include \
            $(TARGET_TOP)/middleware/media/LIBRARY/libcedarc/include \
            $(TARGET_TOP)/middleware/media/LIBRARY/libcedarc/base/include \
            $(TARGET_TOP)/middleware/media/LIBRARY/AudioLib/osal \
            $(TARGET_TOP)/middleware/media/LIBRARY/AudioLib/midware/decoding/include \

TARGET_SHARED_LIB := \
    liblog \
    libmedia_utils \
    libcedarxstream

TARGET_CFLAGS += -fPIC -Wall -Wno-unused-but-set-variable -D__OS_LINUX
TARGET_CPPFLAGS += -fPIC -Wall -Wno-unused-but-set-variable -D__OS_LINUX

TARGET_MODULE := libcedarxdemuxer

include $(BUILD_STATIC_LIB)

#$(TARGET_TOP)/middleware/media/include/utils
