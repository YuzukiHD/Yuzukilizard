TARGET_PATH :=$(call my-dir)
TOP_PATH := $(TARGET_PATH)

#########################################
include $(ENV_CLEAR)

include $(TARGET_TOP)/middleware/config/mpp_config.mk

TARGET_SRC := \
    CallbackNotifier.cpp \
    PreviewWindow.cpp \
    VIChannel.cpp \
    VIDevice.cpp \
    CameraParameters.cpp \
    CameraJpegEncoder.cpp \
    EyeseeCamera.cpp

TARGET_INC := \
    $(TARGET_TOP)/framework/include \
    $(TARGET_TOP)/framework/include/media \
    $(TARGET_TOP)/framework/include/media/camera \
    $(TARGET_TOP)/framework/include/utils \
    $(TARGET_TOP)/middleware/media/vi_api \
    $(TARGET_TOP)/middleware/config \
    $(TARGET_TOP)/middleware/include \
    $(TARGET_TOP)/middleware/include/utils \
    $(TARGET_TOP)/middleware/include/media \
    $(TARGET_TOP)/middleware/include/media/utils \
    $(TARGET_TOP)/middleware/media/include \
    $(TARGET_TOP)/middleware/media/include/utils \
    $(TARGET_TOP)/middleware/media/include/component \
    $(TARGET_TOP)/middleware/media/LIBRARY/libcedarc/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/libisp/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/libisp/include/V4l2Camera \
    $(TARGET_TOP)/middleware/media/LIBRARY/libisp/isp_tuning \
    $(TARGET_TOP)/middleware/media/LIBRARY/include_ai_common \
	$(TARGET_TOP)/middleware/media/LIBRARY/include_eve_common \
    $(TARGET_TOP)/middleware/media/LIBRARY/libaiMOD/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/libeveface/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/libVLPR/include \
    $(TARGET_TOP)/system/public/include/vo \
    $(TARGET_TOP)/system/public/include/kernel-headers \
    $(TARGET_TOP)/system/public/libion/include

#TARGET_SHARED_LIB := \
#    liblog \
#    libcustomaw_media_utils \
#    libhwdisplay

#TARGET_STATIC_LIB := \
#    libmedia_mpp

TARGET_CPPFLAGS += $(CEDARX_EXT_CFLAGS)
TARGET_CFLAGS += $(CEDARX_EXT_CFLAGS)

TARGET_CPPFLAGS += -fPIC -Wall

ifeq ($(strip $(TARGET_PRODUCT)), V5SDV)
	TARGET_CPPFLAGS += -DPRODUCT_V5SDV
endif

ifeq ($(strip $(TARGET_PRODUCT)), V5SDVTP)
	TARGET_CPPFLAGS += -DPRODUCT_V5SDV
endif

#TARGET_LDFLAGS += -static

TARGET_MODULE := libcamera

include $(BUILD_STATIC_LIB)
#########################################
