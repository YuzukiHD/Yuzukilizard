TARGET_PATH:= $(call my-dir)

include $(ENV_CLEAR)

LIB_ROOT=$(TARGET_PATH)/../..
include $(LIB_ROOT)/../config.mk
include $(LIB_ROOT)/stream/config.mk

TARGET_SRC = \
		$(notdir $(wildcard $(TARGET_PATH)/*.c))

TARGET_INC:= \
    $(LIB_ROOT)/base/include \
    $(LIB_ROOT)/stream/include \
    $(LIB_ROOT)/include     \
	$(LIB_ROOT)/..     \

TARGET_CFLAGS += $(CDX_CFLAGS) -fPIC -Wno-psabi

TARGET_MODULE:= libcdx_file_stream

include $(BUILD_STATIC_LIB)

