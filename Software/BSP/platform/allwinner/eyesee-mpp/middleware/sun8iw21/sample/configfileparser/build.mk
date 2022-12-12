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
include $(TARGET_TOP)/middleware/config/mpp_config.mk

include $(ENV_CLEAR)
TARGET_SRC := confparser.c
  
TARGET_INC :=  \
		$(TARGET_TOP) \
		$(TARGET_TOP)/middleware/include/utils \
		$(TARGET_TOP)/middleware/media/LIBRARY/libcedarx/libcore/common/iniparser

TARGET_SHARED_LIB := \
    liblog \
    libcdx_common \

#TARGET_CPPFLAGS += -fPIC -Wall
TARGET_CFLAGS += -fPIC -Wall
TARGET_LDFLAGS +=

TARGET_MODULE := libsample_confparser
include $(BUILD_SHARED_LIB)
#########################################

