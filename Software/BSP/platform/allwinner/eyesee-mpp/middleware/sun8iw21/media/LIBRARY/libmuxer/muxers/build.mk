TARGET_PATH:= $(call my-dir)
include $(ENV_CLEAR)

TARGET_SRC := \
	record_writer.c
	
TARGET_INC := \
    $(TARGET_TOP)/system/public/include \
	$(TARGET_TOP)/middleware/include \
	$(TARGET_TOP)/middleware/media/LIBRARY/libcedarc/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/AudioLib/osal \
	$(TARGET_TOP)/middleware/media/LIBRARY/AudioLib/midware/encoding/include \
	$(TARGET_TOP)/middleware/media/LIBRARY/include_stream \
	$(TARGET_TOP)/middleware/media/LIBRARY/include_FsWriter \
	$(TARGET_TOP)/middleware/media/LIBRARY/include_muxer \
	$(TARGET_TOP)/middleware/media/LIBRARY/libmuxer/include

TARGET_SHARED_LIB :=
TARGET_STATIC_LIB :=

TARGET_CPPFLAGS += -fPIC -Wall
TARGET_CFLAGS += -fPIC -Wall

TARGET_MODULE:= libmuxers

include $(BUILD_STATIC_LIB)

