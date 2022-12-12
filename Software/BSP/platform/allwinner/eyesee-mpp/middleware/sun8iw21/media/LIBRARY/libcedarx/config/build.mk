TARGET_PATH:= $(call my-dir)

#######################
include $(ENV_CLEAR)
CEDARM_PATH=$(TARGET_PATH)/..
include $(CEDARM_PATH)/config.mk

product = $(TARGET_BOARD_PLATFORM)

ifeq ($(product),)
    $(error No TARGET_BOARD_PLATFORM value found!)
endif

TARGET_SRC := $(product)_cedarx.conf

ifeq ($(CONF_CMCC), yes)
    $(warning You have selected $(product) cmcc cedarx.conf)
    TARGET_SRC := $(product)_cmcc_cedarx.conf
else
    $(warning You have selected $(product) cedarx.conf)
endif

TARGET_MODULE := cedarx.conf
TARGET_MODULE_PATH := $(TARGET_OUT)/target/etc


$(info ---------copy config file-----------)
$(call copy-one-file, \
	$(TARGET_PATH)/$(TARGET_SRC), \
	$(TARGET_MODULE_PATH)/$(TARGET_MODULE) \
)
