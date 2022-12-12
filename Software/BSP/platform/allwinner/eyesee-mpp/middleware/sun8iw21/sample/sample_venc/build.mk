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
    sample_venc_mem.c \
    sample_venc.c

TARGET_INC :=  \
    $(TARGET_TOP)/system/include \
    $(TARGET_TOP)/system/public/rgb_ctrl \
    $(TARGET_TOP)/middleware/include \
    $(TARGET_TOP)/middleware/include/utils \
    $(TARGET_TOP)/middleware/include/media \
    $(TARGET_TOP)/middleware/media/include \
	$(TARGET_TOP)/system/public/libion/include \
    $(TARGET_TOP)/middleware/media/include/utils \
    $(TARGET_TOP)/middleware/media/include/component \
    $(TARGET_TOP)/middleware/media/LIBRARY/libcedarc/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/libcedarx/libcore/common/iniparser \
    $(TARGET_TOP)/middleware/sample/configfileparser

TARGET_SHARED_LIB := \
    liblog \
    libion \
    libvencoder \
    libcdx_common \
    libsample_confparser \
    libcedarxrender \
   	libmpp_vi \
   	libmpp_isp \
   	libmpp_ise \
   	libmpp_vo \
   	libmpp_component \
    libmedia_mpp \
    libISP \
    libMemAdapter \
    libmedia_utils

#librgb_ctrl \

TARGET_LDFLAGS += \
    -lpthread \
    -lrt \
    -ldl \

TARGET_MODULE := sample_venc
include $(BUILD_BIN)

$(call copy-files-under, \
        $(TARGET_PATH)/*.conf, \
        $(TARGET_OUT)/target/etc \
)
#########################################
