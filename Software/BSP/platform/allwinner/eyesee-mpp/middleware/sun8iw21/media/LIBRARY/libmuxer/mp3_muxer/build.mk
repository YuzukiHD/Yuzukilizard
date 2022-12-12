TARGET_PATH:= $(call my-dir)
include $(ENV_CLEAR)

TARGET_SRC := \
    mp3.c \
    Mp3Muxer.c
	
TARGET_INC := \
    $(TARGET_TOP)/system/public/include \
	$(TARGET_TOP)/middleware/include \
	$(TARGET_TOP)/middleware/media/LIBRARY/include_stream \
	$(TARGET_TOP)/middleware/media/LIBRARY/include_FsWriter \
	$(TARGET_TOP)/middleware/media/LIBRARY/include_muxer \
	$(TARGET_TOP)/middleware/media/LIBRARY/libmuxer/include

TARGET_CPPFLAGS += -fPIC -Wall
TARGET_CFLAGS += -fPIC -Wall

TARGET_MODULE:= libmp3_muxer

include $(BUILD_STATIC_LIB)

