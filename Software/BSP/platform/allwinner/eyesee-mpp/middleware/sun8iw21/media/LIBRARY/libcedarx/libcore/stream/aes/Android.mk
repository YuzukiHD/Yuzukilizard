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
	$(LIB_ROOT)/include/     \
    $(LIB_ROOT)/../

ifeq ($(CONF_ANDROID_VERSION), 4.4)
    LOCAL_C_INCLUDES += $(TOP)/external/openssl/include
else ifeq ($(CONF_ANDROID_VERSION), 5.0)
    LOCAL_C_INCLUDES += $(TOP)/external/openssl/include
else ifeq ($(CONF_ANDROID_VERSION), 5.1)
    LOCAL_C_INCLUDES += $(TOP)/external/openssl/include
else
    LOCAL_C_INCLUDES += $(TOP)/external/boringssl/src/include
endif

LOCAL_CFLAGS += $(CDX_CFLAGS)

LOCAL_MODULE_TAGS := optional
LOCAL_PRELINK_MODULE := false

LOCAL_MODULE:= libcdx_aes_stream

ifeq ($(TARGET_ARCH),arm)
    LOCAL_CFLAGS += -Wno-psabi
endif

include $(BUILD_STATIC_LIBRARY)
