TARGET_PATH:= $(call my-dir)
include $(ENV_CLEAR)

TARGET_SRC := \
	avc.c \
	ByteIOContext.c \
	hevc.c \
	Mp4Muxer.c \
	Mp4MuxerDrv.c \
	sa_config.c \
	
TARGET_INC := \
    $(TARGET_TOP)/system/public/include \
	$(TARGET_TOP)/middleware/include \
	$(TARGET_TOP)/middleware/include/utils \
	$(TARGET_TOP)/middleware/media/LIBRARY/libcedarc/include \
	$(TARGET_TOP)/middleware/media/LIBRARY/AudioLib/midware/encoding/include \
	$(TARGET_TOP)/middleware/media/LIBRARY/include_stream \
	$(TARGET_TOP)/middleware/media/LIBRARY/include_FsWriter \
	$(TARGET_TOP)/middleware/media/LIBRARY/include_muxer \
	$(TARGET_TOP)/middleware/media/LIBRARY/libmuxer \
	$(TARGET_TOP)/middleware/media/LIBRARY/libmuxer/include \
	$(TARGET_TOP)/middleware/media/LIBRARY/libmuxer/common

TARGET_CPPFLAGS += -fPIC -Wall -Wno-unused-function -Wno-unused-but-set-variable -DHAVE_AV_CONFIG_H
TARGET_CFLAGS += -fPIC -Wall -Wno-unused-function -Wno-unused-but-set-variable -DHAVE_AV_CONFIG_H

TARGET_MODULE:= libmp4_muxer

include $(BUILD_STATIC_LIB)

