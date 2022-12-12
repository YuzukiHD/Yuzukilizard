TARGET_PATH:= $(call my-dir)

include $(ENV_CLEAR)

include $(TARGET_PATH)/../../config.mk
include $(TARGET_PATH)/config.mk

TARGET_SRC = \
		$(notdir $(wildcard $(TARGET_PATH)/*.c)) \

TARGET_INC:= $(TARGET_PATH)/include    \
                   $(TARGET_PATH)/../../     \
                   $(TARGET_PATH)/../include/     \

TARGET_CFLAGS += $(CDX_CFLAGS) -fPIC -Wno-psabi

TARGET_MODULE:= libcdx_base

#TARGET_SHARED_LIB:= libcutils
#                         libutils \
#                         liblog

#ifeq ($(CONF_ANDROID_VERSION),4.4)
#TARGET_SHARED_LIB += libcorkscrew
#endif

#ifeq ($(TARGET_ARCH),arm)
#    LOCAL_CFLAGS += -Wno-psabi
#endif

include $(BUILD_SHARED_LIB)

