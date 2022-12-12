TARGET_PATH :=$(call my-dir)
TOP_PATH := $(TARGET_PATH)

#########################################
include $(ENV_CLEAR)

TARGET_SRC := \
	sample_LargeFile.cpp

TARGET_INC := \
    $(TARGET_TOP)/framework/include \
    $(TARGET_TOP)/framework/include/utils \
    $(TARGET_TOP)/middleware/sample/configfileparser \
    $(TARGET_TOP)/middleware/media/LIBRARY/libcedarx/libcore/common/iniparser \
    $(TARGET_TOP)/middleware/include \
    $(TARGET_TOP)/middleware/include/utils \
    $(TARGET_TOP)/middleware/media/include \
    $(TARGET_TOP)/middleware/media/include/utils \

TARGET_SHARED_LIB := \
    liblog \
    libpthread \
    libdl \
    libcdx_common \
    libsample_confparser \
    libmedia_utils \
    libcustomaw_media_utils \

TARGET_STATIC_LIB := \

TARGET_CPPFLAGS += -fPIC -Wall -Wno-unused-but-set-variable
TARGET_CFLAGS += -fPIC -Wall -Wno-unused-but-set-variable

TARGET_MODULE := sample_LargeFile

include $(BUILD_BIN)
#########################################
