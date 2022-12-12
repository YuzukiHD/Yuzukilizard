LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

CEDARX_ROOT=$(LOCAL_PATH)/../../../
include $(CEDARX_ROOT)/config.mk

LOCAL_SRC_FILES = \
		$(notdir $(wildcard $(LOCAL_PATH)/*.c)) \

LOCAL_C_INCLUDES:= \
    $(CEDARX_ROOT)/ \
    $(CEDARX_ROOT)/libcore \
    $(CEDARX_ROOT)/libcore/base/include \
    $(CEDARX_ROOT)/libcore/parser/include \
    $(CEDARX_ROOT)/libcore/parser/mov/ \
    $(CEDARX_ROOT)/libcore/stream/include \
    $(CEDARX_ROOT)/external/include/adecoder \
    $(TOP)/frameworks/av/media/libcedarc/include \
    $(TOP)/frameworks/av/media/libcedarc/vdecoder/include \
    $(TOP)/frameworks/av/media/libcedarc/sdecoder/include \
    $(TOP)/external/libxml2/include 

ifeq ($(CONF_ANDROID_VERSION), 4.4)
LOCAL_C_INCLUDES += $(TOP)/external/icu4c/common
else
LOCAL_C_INCLUDES += $(TOP)/external/icu/icu4c/source/common
endif

LOCAL_STATIC_LIBRARIES = libxml2
LOCAL_CFLAGS += $(CDX_CFLAGS)

LOCAL_MODULE_TAGS := optional
LOCAL_PRELINK_MODULE := false

LOCAL_MODULE := libcdx_dash_parser

ifeq ($(TARGET_ARCH),arm)
    LOCAL_CFLAGS += -Wno-psabi
endif

include $(BUILD_STATIC_LIBRARY)

