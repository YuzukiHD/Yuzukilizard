TARGET_PATH :=$(call my-dir)

#########################################
include $(ENV_CLEAR)

TARGET_SRC := \
    EyeseeVideoResizer.cpp \
    VREncBuffer.cpp \ 

TARGET_INC := \
    $(TARGET_TOP)/system/public/include \
    $(TARGET_TOP)/middleware/include \
    $(TARGET_TOP)/middleware/include/utils \
    $(TARGET_TOP)/middleware/include/media \
    $(TARGET_TOP)/middleware/media/include \
    $(TARGET_TOP)/middleware/media/include/component \
    $(TARGET_TOP)/middleware/media/LIBRARY/libcedarc/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/include_stream \
    $(TARGET_TOP)/middleware/media/LIBRARY/include_FsWriter \
    $(TARGET_TOP)/middleware/media/LIBRARY/include_muxer \
    $(TARGET_TOP)/framework/include \
    $(TARGET_TOP)/framework/include/media \
    $(TARGET_TOP)/framework/include/media/camera \
    $(TARGET_TOP)/framework/include/media/ise \
    $(TARGET_TOP)/framework/include/media/player \
    $(TARGET_TOP)/framework/include/media/thumbretriever \
    $(TARGET_TOP)/framework/include/media/videoresizer \
    $(TARGET_TOP)/framework/include/utils \

#TARGET_SHARED_LIB := \
#    liblog \
#    libmedia_utils \
#    libmedia_mpp

#TARGET_STATIC_LIB := \
#    libcamera \
#    libise

TARGET_CPPFLAGS += -fPIC -Wall
TARGET_CFLAGS += -fPIC -Wall

TARGET_LDFLAGS += -static

TARGET_MODULE := libvideoresizer
include $(BUILD_STATIC_LIB)

#########################################
