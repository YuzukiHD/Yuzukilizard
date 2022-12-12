TARGET_PATH :=$(call my-dir)
TOP_PATH := $(TARGET_PATH)

#########################################
include $(ENV_CLEAR)

TARGET_SRC := \
	sample_EyeseeIse.cpp

TARGET_INC := \
    $(TARGET_TOP)/framework/include \
    $(TARGET_TOP)/framework/include/media \
    $(TARGET_TOP)/framework/include/media/camera \
    $(TARGET_TOP)/framework/include/media/ise \
    $(TARGET_TOP)/framework/include/media/recorder \
    $(TARGET_TOP)/framework/include/utils \
    $(TARGET_TOP)/system/public/libion/include \
    $(TARGET_TOP)/system/public/include \
    $(TARGET_TOP)/system/public/include/vo \
    $(TARGET_TOP)/middleware/include \
    $(TARGET_TOP)/middleware/include/utils \
    $(TARGET_TOP)/middleware/include/media \
    $(TARGET_TOP)/middleware/media/include/utils \
    $(TARGET_TOP)/middleware/media/include/component \
    $(TARGET_TOP)/middleware/media/LIBRARY/libisp/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/libisp/include/V4l2Camera \
    $(TARGET_TOP)/middleware/media/LIBRARY/libisp/isp_tuning \
    $(TARGET_TOP)/middleware/media/LIBRARY/libAIE_Vda/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/include_stream \
    $(TARGET_TOP)/middleware/media/LIBRARY/include_FsWriter \
    $(TARGET_TOP)/middleware/media/LIBRARY/include_muxer \
    $(TARGET_TOP)/middleware/media/LIBRARY/libcedarc/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/libcedarx/libcore/common/iniparser \
    $(TARGET_TOP)/middleware/media/LIBRARY/libaiMOD/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/include_ai_common \
    $(TARGET_TOP)/middleware/media/LIBRARY/include_eve_common \
    $(TARGET_TOP)/middleware/media/LIBRARY/libVLPR/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/libeveface/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/libISE \
    $(TARGET_TOP)/middleware/sample/configfileparser

TARGET_SHARED_LIB := \
	libion \
	liblog \
	libpthread \
   	libhwdisplay \
   	libcdx_common \
   	libsample_confparser \
   	libmedia_utils \
   	libmedia_mpp \
	libmpp_vi \
	libmpp_vo \
	libmpp_isp \
	libmpp_ise \
	libmpp_uvc \
	libmpp_component \
   	libMemAdapter \
   	libvencoder \
   	libcustomaw_media_utils \

TARGET_STATIC_LIB := \
    libcamera \
    librecorder \
    libise

TARGET_CPPFLAGS += -fPIC -Wall -Wno-unused-but-set-variable
TARGET_CFLAGS += -fPIC -Wall -Wno-unused-but-set-variable

TARGET_MODULE := sample_EyeseeIse

include $(BUILD_BIN)
#########################################

$(call copy-files-under, $(TARGET_PATH)/sample_EyeseeIse.conf, $(TARGET_OUT)/target/etc)
