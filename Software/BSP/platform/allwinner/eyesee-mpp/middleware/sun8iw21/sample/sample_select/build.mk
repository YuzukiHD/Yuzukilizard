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

TARGET_PATH :=$(call my-dir)
TOP_PATH := $(TARGET_PATH)
#########################################

#########################################
include $(ENV_CLEAR)
TARGET_SRC := \
    sample_select.c

TARGET_INC :=  \
    $(TARGET_TOP)/system/include \
    $(TARGET_TOP)/middleware/include \
    $(TARGET_TOP)/middleware/include/utils \
    $(TARGET_TOP)/middleware/include/media \
    $(TARGET_TOP)/middleware/media/include/utils \
    $(TARGET_TOP)/middleware/media/LIBRARY/libcedarx/libcore/common/iniparser \
    $(TARGET_TOP)/middleware/media/LIBRARY/libcedarc/include \
    $(TARGET_TOP)/middleware/sample/configfileparser \

TARGET_SHARED_LIB := \
    liblog \
    libpthread \
    libmedia_utils \
    libmedia_mpp \
    libmpp_vi \
    libmpp_vo \
    libmpp_isp \
    libmpp_ise \
    libmpp_component \
    libsample_confparser \

TARGET_LDFLAGS += \
    -lpthread \
    -lrt \
    -ldl

TARGET_CPPFLAGS += -fPIC -Wall

TARGET_MODULE := sample_select

include $(BUILD_BIN)

#########################################
$(call copy-files-under, \
    $(TARGET_PATH)/sample_select.conf, \
    $(TARGET_OUT)/target/etc \
)
#########################################
