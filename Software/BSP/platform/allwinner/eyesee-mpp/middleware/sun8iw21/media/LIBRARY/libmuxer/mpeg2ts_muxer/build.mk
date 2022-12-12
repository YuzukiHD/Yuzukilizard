TARGET_PATH:= $(call my-dir)
include $(ENV_CLEAR)

TARGET_SRC := \
	Mpeg2tsMuxerDrv.c \
	Mpeg2tsMuxer.c
	
TARGET_INC := \
    $(TARGET_TOP)/system/public/include \
	$(TARGET_TOP)/middleware/include \
	$(TARGET_TOP)/middleware/media/LIBRARY/libcedarc/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/AudioLib/osal \
	$(TARGET_TOP)/middleware/media/LIBRARY/AudioLib/midware/encoding/include \
	$(TARGET_TOP)/middleware/media/LIBRARY/textEncLib/include \
	$(TARGET_TOP)/middleware/media/LIBRARY/include_stream \
	$(TARGET_TOP)/middleware/media/LIBRARY/include_FsWriter \
	$(TARGET_TOP)/middleware/media/LIBRARY/include_muxer \
	$(TARGET_TOP)/middleware/media/LIBRARY/libmuxer/include

TARGET_CPPFLAGS += -fPIC -Wall -Wno-unused-function
TARGET_CFLAGS += -fPIC -Wall -Wno-unused-function

TARGET_MODULE:= libmpeg2ts_muxer

include $(BUILD_STATIC_LIB)

