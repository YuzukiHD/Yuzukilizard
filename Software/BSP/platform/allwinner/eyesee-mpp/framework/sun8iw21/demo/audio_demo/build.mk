TARGET_PATH :=$(call my-dir)

#########################################
include $(ENV_CLEAR)

TARGET_SRC := \
	audio_demo.c

TARGET_INC := \
    $(TARGET_TOP)/system/include \
    $(TARGET_TOP)/middleware/include \
    $(TARGET_TOP)/middleware/include/utils \
    $(TARGET_TOP)/middleware/include/media \
    $(TARGET_TOP)/framework/include \

TARGET_SHARED_LIB := \
	liblog \
	libpthread \
	libmedia_utils \
	libmedia_mpp \
	libmpp_vi \
	libmpp_vo \
	libmpp_isp \
	libmpp_component \
	libcustomaw_media_utils \

TARGET_CPPFLAGS += -fPIC -Wall

TARGET_MODULE := audio_demo

include $(BUILD_BIN)
#########################################
