TARGET_PATH :=$(call my-dir)
TOP_PATH := $(TARGET_PATH)

#########################################
include $(ENV_CLEAR)

include $(TARGET_TOP)/middleware/config/mpp_config.mk

ifeq ($(MPPCFG_COMPILE_DYNAMIC_LIB), Y)

TARGET_SRC := \
	sample_USBCamera.cpp

TARGET_INC := \
    $(TARGET_TOP)/framework/include \
    $(TARGET_TOP)/framework/include/media \
    $(TARGET_TOP)/framework/include/media/camera \
    $(TARGET_TOP)/framework/include/media/ise \
    $(TARGET_TOP)/framework/include/media/recorder \
	$(TARGET_TOP)/framework/include/media/usbcamera \
	$(TARGET_TOP)/framework/media/camera \
    $(TARGET_TOP)/framework/include/utils \
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
	$(TARGET_TOP)/middleware/media/LIBRARY/include_ai_common \
    $(TARGET_TOP)/middleware/media/LIBRARY/libaiMOD/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/libVLPR/include \
	$(TARGET_TOP)/middleware/media/LIBRARY/libISE \
	$(TARGET_TOP)/middleware/media/LIBRARY/include_eve_common \
    $(TARGET_TOP)/middleware/media/LIBRARY/libeveface/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/include_stream \
    $(TARGET_TOP)/middleware/media/LIBRARY/include_FsWriter \
    $(TARGET_TOP)/middleware/media/LIBRARY/libcedarc/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/libcedarx/libcore/common/iniparser \
    $(TARGET_TOP)/middleware/sample/configfileparser

TARGET_SHARED_LIB := \
	liblog \
	libpthread \
   	libhwdisplay \
   	libcdx_common \
   	libsample_confparser \
   	libcedarxrender \
   	libmpp_vi \
	libmpp_uvc \
   	libmpp_isp \
   	libmpp_ise \
   	libmpp_vo \
   	libmpp_component \
   	libmedia_utils \
   	libmedia_mpp \
   	libISP \
   	libMemAdapter \
   	libvencoder \
   	libcustomaw_media_utils \

ifeq ($(MPPCFG_MOD),Y)
TARGET_SHARED_LIB += libai_MOD
endif
ifeq ($(MPPCFG_EVEFACE),Y)
TARGET_SHARED_LIB += libeve_event
endif
ifeq ($(MPPCFG_VLPR),Y)
TARGET_SHARED_LIB += libai_VLPR
endif

ifneq ($(MPPCFG_MOD)_$(MPPCFG_VLPR),N_N)
TARGET_SHARED_LIB += libCVEMiddleInterface
endif

TARGET_STATIC_LIB := \
    libusbcamera \
	librecorder  \
	libise \

TARGET_CPPFLAGS += $(CEDARX_EXT_CFLAGS)
TARGET_CFLAGS += $(CEDARX_EXT_CFLAGS)

TARGET_CPPFLAGS += -fPIC -Wall -Wno-unused-but-set-variable
TARGET_CFLAGS += -fPIC -Wall -Wno-unused-but-set-variable

TARGET_MODULE := sample_USBCamera

include $(BUILD_BIN)

else
    $(warning "warning: dynamic compile, but MPPCFG_COMPILE_DYNAMIC_LIB = $(MPPCFG_COMPILE_DYNAMIC_LIB)")
endif

#########################################
