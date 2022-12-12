TARGET_PATH :=$(call my-dir)
TOP_PATH := $(TARGET_PATH)

#########################################
include $(ENV_CLEAR)

TARGET_SRC := \
	sample_Motion.cpp

TARGET_INC := \
	$(TARGET_TOP)/framework/include \
    $(TARGET_TOP)/framework/include/media \
	$(TARGET_TOP)/framework/include/media/player \
    $(TARGET_TOP)/framework/include/media/camera \
    $(TARGET_TOP)/framework/include/media/ise \
    $(TARGET_TOP)/framework/include/media/recorder \
    $(TARGET_TOP)/framework/include/utils \
    $(TARGET_TOP)/custom_aw/include \
    $(TARGET_TOP)/custom_aw/include/media \
    $(TARGET_TOP)/custom_aw/include/media/player \
    $(TARGET_TOP)/custom_aw/include/utils \
    $(TARGET_TOP)/system/public/include \
    $(TARGET_TOP)/system/public/include/vo \
	$(TARGET_TOP)/system/public/libion/include \
    $(TARGET_TOP)/framework/include \
    $(TARGET_TOP)/framework/include/media \
    $(TARGET_TOP)/framework/include/media/bdii \
    $(TARGET_TOP)/framework/include/media/camera \
    $(TARGET_TOP)/framework/include/media/ise \
    $(TARGET_TOP)/framework/include/media/player \
    $(TARGET_TOP)/framework/include/media/recorder \
	$(TARGET_TOP)/framework/include/media/thumbretriever \
	$(TARGET_TOP)/framework/include/media/motion \
    $(TARGET_TOP)/framework/include/utils \
    $(TARGET_TOP)/middleware/include \
    $(TARGET_TOP)/middleware/include/utils \
    $(TARGET_TOP)/middleware/include/media \
    $(TARGET_TOP)/middleware/media/include/utils \
    $(TARGET_TOP)/middleware/media/include/component \
    $(TARGET_TOP)/middleware/media/LIBRARY/libisp/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/libisp/include/V4l2Camera \
    $(TARGET_TOP)/middleware/media/LIBRARY/libisp/isp_tuning \
    $(TARGET_TOP)/middleware/media/LIBRARY/include_ai_common \
    $(TARGET_TOP)/middleware/media/LIBRARY/libaiMOD/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/include_stream \
    $(TARGET_TOP)/middleware/media/LIBRARY/include_FsWriter \
    $(TARGET_TOP)/middleware/media/LIBRARY/libcedarc/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/libcedarx/libcore/common/iniparser \
	$(TARGET_TOP)/middleware/media/LIBRARY/lib_aw_ai_core/include \
	$(TARGET_TOP)/middleware/media/LIBRARY/lib_aw_ai_mt/include \
    $(TARGET_TOP)/middleware/sample/configfileparser

TARGET_SHARED_LIB := \
   	libcdx_common \
   	libsample_confparser \
   	libcedarxrender \
   	libmpp_component \
   	libmedia_utils \
   	libmedia_mpp \
   	libMemAdapter \
   	libcustomaw_media_utils \
	libmpp_component \
	lib_aw_ai_mt \
	lib_aw_ai_core \
	libion \
	liblog \
	libpthread \

TARGET_STATIC_LIB := \
    libmotion \

TARGET_CPPFLAGS += -fPIC -Wall -Wno-unused-but-set-variable
TARGET_CFLAGS += -fPIC -Wall -Wno-unused-but-set-variable

TARGET_MODULE := sample_Motion

include $(BUILD_BIN)
#########################################
