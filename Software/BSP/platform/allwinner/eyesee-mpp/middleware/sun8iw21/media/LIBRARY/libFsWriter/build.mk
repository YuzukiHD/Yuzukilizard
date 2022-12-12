TARGET_PATH :=$(call my-dir)
include $(ENV_CLEAR)

TARGET_SRC := \
	FsWriter.c \
	FsWriteDirect.c \
	FsSimpleCache.c \
	FsCache.c

TARGET_INC := \
            $(TARGET_TOP)/system/public/include \
            $(TARGET_TOP)/middleware/include \
            $(TARGET_TOP)/middleware/include/utils \
            $(TARGET_TOP)/middleware/include/media \
            $(TARGET_TOP)/middleware/media/include/utils \
            $(TARGET_TOP)/middleware/media/LIBRARY/include_stream \
            $(TARGET_TOP)/middleware/media/LIBRARY/include_FsWriter

TARGET_SHARED_LIB := \
    liblog \
    libmedia_utils \
    libcedarxstream

TARGET_CPPFLAGS += -fPIC -Wall
TARGET_CFLAGS += -fPIC -Wall

TARGET_MODULE := libFsWriter

include $(BUILD_STATIC_LIB)

