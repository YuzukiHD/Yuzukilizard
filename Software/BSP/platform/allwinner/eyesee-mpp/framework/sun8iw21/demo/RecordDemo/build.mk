#
# 1. Set the path and clear environment
# 	TARGET_PATH := $(call my-dir)
# 	include $(ENV_CLEAR)
#
# 2. Set the source files and headers files
#	TARGET_SRC := xxx_1.c xxx_2.c
#	TARGET_INc := xxx_1.h xxx_2.h
#
# 3. Set the output target
#	TARGET_MODULE := xxx
#
# 4. Include the main makefile
#	include $(BUILD_BIN)
#
# Before include the build makefile, you can set the compilaion
# flags, e.g. TARGET_ASFLAGS TARGET_CFLAGS TARGET_CPPFLAGS
#

TARGET_PATH:= $(call my-dir)

########################################
include $(ENV_CLEAR)

include $(TARGET_TOP)/middleware/config/mpp_config.mk

TARGET_SRC := RecordDemo.cpp

TARGET_INC := \
    $(TARGET_TOP)/system/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/libisp/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/libisp/include/V4l2Camera \
    $(TARGET_TOP)/middleware/media/LIBRARY/libisp/isp_tuning \
	$(TARGET_TOP)/middleware/media/LIBRARY/include_ai_common \
    $(TARGET_TOP)/middleware/media/LIBRARY/include_eve_common \
    $(TARGET_TOP)/middleware/media/LIBRARY/libaiMOD/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/include_stream \
    $(TARGET_TOP)/middleware/media/LIBRARY/include_FsWriter \
    $(TARGET_TOP)/middleware/media/LIBRARY/libcedarc/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/libVLPR/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/libeveface/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/libISE \
    $(TARGET_TOP)/middleware/include/utils \
    $(TARGET_TOP)/middleware/include/media \
    $(TARGET_TOP)/middleware/media/include/component \
    $(TARGET_TOP)/middleware/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/include_muxer \
    $(TARGET_TOP)/framework/include \
    $(TARGET_TOP)/framework/include/media \
    $(TARGET_TOP)/framework/include/media/camera \
    $(TARGET_TOP)/framework/include/media/ise \
    $(TARGET_TOP)/framework/include/media/recorder \
    $(TARGET_TOP)/framework/include/utils

TARGET_SHARED_LIB := \
    librt \
    libpthread \
    liblog \
    libhwdisplay \
    libmedia_utils \
    libmedia_mpp \
    libmpp_vi \
    libmpp_vo \
    libmpp_isp \
    libmpp_ise \
    libmpp_component \
    libmpp_uvc \
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
    libcamera \
    librecorder \
    libise
    
TARGET_CPPFLAGS += $(CEDARX_EXT_CFLAGS)
TARGET_CFLAGS += $(CEDARX_EXT_CFLAGS)

TARGET_CPPFLAGS += -fPIC -Wall -Wno-unused-but-set-variable
TARGET_CFLAGS += -fPIC -Wall -Wno-unused-but-set-variable

#TARGET_LDFLAGS += -static

TARGET_MODULE := RecordDemo

include $(BUILD_BIN)
#include $(BUILD_SHARED_LIB)

