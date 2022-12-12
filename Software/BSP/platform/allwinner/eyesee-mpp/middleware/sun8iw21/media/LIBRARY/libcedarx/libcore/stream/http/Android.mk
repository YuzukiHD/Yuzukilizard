LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LIB_ROOT=$(LOCAL_PATH)/../..
include $(LIB_ROOT)/../config.mk
include $(LIB_ROOT)/stream/config.mk

LOCAL_SRC_FILES = \
		$(notdir $(wildcard $(LOCAL_PATH)/*.c))

LOCAL_C_INCLUDES:= \
    $(LIB_ROOT)/base/include \
    $(LIB_ROOT)/stream/include \
    $(LIB_ROOT)/include \
    $(TOP)/external/zlib \
    $(LIB_ROOT)/../     \

LOCAL_CFLAGS += $(CDX_CFLAGS)
LOCAL_MODULE_TAGS := optional
LOCAL_PRELINK_MODULE := false

LOCAL_MODULE:= libcdx_http_stream

ifeq ($(TARGET_ARCH),arm)
    LOCAL_CFLAGS += -Wno-psabi
endif

include $(BUILD_STATIC_LIBRARY)

