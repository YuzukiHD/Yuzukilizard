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

#########################################
include $(ENV_CLEAR)
include $(TARGET_TOP)/middleware/config/mpp_config.mk

TARGET_CPPFLAGS += -fPIC $(CEDARX_EXT_CFLAGS)
TARGET_CFLAGS += -fPIC $(CEDARX_EXT_CFLAGS)

TARGET_SRC := \
   utils/generic_buffer.c \
   utils/iio_utils.c \
   utils/ring_buffer.c

TARGET_INC := \
    $(TARGET_TOP)/middleware/include/ \
    $(TARGET_TOP)/middleware/include/utils \
    $(TARGET_PATH)/ \

TARGET_SHARED_LIB := \

TARGET_STATIC_LIB := \

TARGET_CFLAGS += -fPIC -Wall -Wno-unused-function -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-label \

TARGET_MODULE := libEIS

include $(BUILD_STATIC_LIB)

#########################################
include $(ENV_CLEAR)
include $(TARGET_TOP)/middleware/config/mpp_config.mk

TARGET_MODULE := lib_eis

TARGET_PREBUILT_LIBS :=

TARGET_PREBUILT_LIBS += $(TARGET_PATH)/algo/lib_eis.a

include $(BUILD_MULTI_PREBUILT)

