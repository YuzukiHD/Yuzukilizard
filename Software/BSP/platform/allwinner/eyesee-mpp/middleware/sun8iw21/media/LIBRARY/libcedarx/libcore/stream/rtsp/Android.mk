LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LIB_ROOT=$(LOCAL_PATH)/../..
include $(LIB_ROOT)/../config.mk

include $(LIB_ROOT)/stream/config.mk

LOCAL_SRC_FILES = \
		$(notdir $(wildcard $(LOCAL_PATH)/*.cpp)) \
		$(notdir $(wildcard $(LOCAL_PATH)/*.c))

LOCAL_C_INCLUDES:= \
    $(LIB_ROOT)/external/include/live \
	$(TOP)/external/openssl/include \
	$(LIB_ROOT)/base/include \
	$(LIB_ROOT)/stream/include \
    $(LIB_ROOT)/parser/include \
	$(LIB_ROOT)/../external/include/adecoder \
	$(LIB_ROOT)/../external/include/live \
	$(LIB_ROOT)/../../libcedarc/include \
	$(LIB_ROOT)/include \
	$(LIB_ROOT)/../ \
	$(TOP)/frameworks/av/media/libcedarc/include \


LOCAL_CFLAGS += $(CDX_CFLAGS) -Wno-unused

LOCAL_MODULE_TAGS := optional
LOCAL_PRELINK_MODULE := false

LOCAL_MODULE:= libcdx_rtsp_stream

ifeq ($(TARGET_ARCH),arm)
    LOCAL_CFLAGS += -Wno-psabi
endif

include $(BUILD_STATIC_LIBRARY)

