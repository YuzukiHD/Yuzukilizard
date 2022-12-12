LOCAL_PATH := $(call my-dir)

#######################
include $(CLEAR_VARS)
CEDARM_PATH=$(LOCAL_PATH)/..
include $(CEDARM_PATH)/config.mk
CONF_ANDROID_MAJOR_VERSION = $(shell echo $(PLATFORM_VERSION) | cut -c 1)

product = $(TARGET_BOARD_PLATFORM)

ifeq ($(product),)
    $(error No TARGET_BOARD_PLATFORM value found!)
endif

LOCAL_SRC_FILES := $(product)_cedarx.conf

ifeq ($(CONF_CMCC), yes)
    $(warning You have selected $(product) cmcc cedarx.conf)
    LOCAL_SRC_FILES := $(product)_cmcc_cedarx.conf
else
    $(warning You have selected $(product) cedarx.conf)
endif

LOCAL_MODULE := cedarx.conf
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_PATH := $(TARGET_OUT)/etc

LOCAL_MODULE_CLASS := FAKE
include $(BUILD_PREBUILT)