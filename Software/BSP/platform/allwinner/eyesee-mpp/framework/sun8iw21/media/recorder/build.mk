TARGET_PATH :=$(call my-dir)

#########################################
include $(ENV_CLEAR)

TARGET_SRC := \
    CameraFrameManager.cpp \
    EyeseeRecorder.cpp \
    MediaCallbackDispatcher.cpp \
    DynamicBitRateControl.cpp

TARGET_INC := \
    $(TARGET_TOP)/system/public/include \
    $(TARGET_TOP)/middleware/include \
    $(TARGET_TOP)/middleware/include/utils \
    $(TARGET_TOP)/middleware/include/media \
	$(TARGET_TOP)/middleware/media/include \
	$(TARGET_TOP)/middleware/media/include/utils \
    $(TARGET_TOP)/middleware/media/include/component \
    $(TARGET_TOP)/middleware/media/LIBRARY/libisp/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/libisp/include/V4l2Camera \
    $(TARGET_TOP)/middleware/media/LIBRARY/libisp/isp_tuning \
	$(TARGET_TOP)/middleware/media/LIBRARY/include_ai_common \
    $(TARGET_TOP)/middleware/media/LIBRARY/include_eve_common \
    $(TARGET_TOP)/middleware/media/LIBRARY/libaiMOD/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/libeveface/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/libVLPR/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/libcedarc/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/AudioLib/osal \
	$(TARGET_TOP)/middleware/media/LIBRARY/AudioLib/midware/encoding \
    $(TARGET_TOP)/middleware/media/LIBRARY/AudioLib/midware/encoding/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/AudioLib/midware/decoding/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/textEncLib/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/include_stream \
    $(TARGET_TOP)/middleware/media/LIBRARY/include_FsWriter \
    $(TARGET_TOP)/middleware/media/LIBRARY/include_muxer \
    $(TARGET_TOP)/middleware/media/LIBRARY/libISE \
    $(TARGET_TOP)/framework/include \
    $(TARGET_TOP)/framework/include/media \
    $(TARGET_TOP)/framework/include/media/camera \
    $(TARGET_TOP)/framework/include/media/ise \
    $(TARGET_TOP)/framework/include/media/recorder \
    $(TARGET_TOP)/framework/include/utils

#TARGET_SHARED_LIB := \
#    liblog \
#    libmedia_utils \
#    libmedia_mpp

#TARGET_STATIC_LIB := \
#    libcamera \
#    libise

TARGET_CPPFLAGS += -fPIC -Wall -D__OS_LINUX
TARGET_CFLAGS += -fPIC -Wall -D__OS_LINUX

#TARGET_LDFLAGS += -static

TARGET_MODULE := librecorder

include $(BUILD_STATIC_LIB)
#########################################
