TARGET_PATH :=$(call my-dir)
TOP_PATH := $(TARGET_PATH)

#########################################
include $(ENV_CLEAR)

TARGET_SRC := \
	sample_VideoResizer.cpp

TARGET_INC := \
    $(TARGET_TOP)/framework/include \
    $(TARGET_TOP)/framework/include/media \
    $(TARGET_TOP)/framework/include/media/videoresizer \
    $(TARGET_TOP)/framework/include/utils \
    $(TARGET_TOP)/middleware/include \
    $(TARGET_TOP)/middleware/include/utils \
    $(TARGET_TOP)/middleware/include/media \
    $(TARGET_TOP)/middleware/media/include/utils \
    $(TARGET_TOP)/middleware/media/include/component \
    $(TARGET_TOP)/middleware/media/LIBRARY/include_stream \
    $(TARGET_TOP)/middleware/media/LIBRARY/include_FsWriter \
    $(TARGET_TOP)/middleware/media/LIBRARY/libcedarc/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/libcedarx/libcore/common/iniparser \
    $(TARGET_TOP)/middleware/sample/configfileparser  \

TARGET_SHARED_LIB := \
	liblog \
	libpthread \
   	libhwdisplay \
   	libcdx_common \
   	libsample_confparser \
   	libcedarxrender \
   	libmpp_vi \
   	libmpp_isp \
   	libmpp_vo \
   	libmpp_component \
   	libmedia_utils \
	libmpp_ise \
   	libmedia_mpp \
   	libISP \
   	libMemAdapter \
   	libvencoder \
   	libcustomaw_media_utils \
	libmpp_component \


TARGET_STATIC_LIB := \
    libvideoresizer

TARGET_CPPFLAGS += -fPIC -Wall -Wno-unused-but-set-variable
TARGET_CFLAGS += -fPIC -Wall -Wno-unused-but-set-variable

TARGET_MODULE := sample_videoresizer

include $(BUILD_BIN)
#########################################
