#
# 1. Set the path and clear environment
# 	TARGET_PATH := $(call my-dir)
# 	include $(ENV_CLEAR)
#
# 2. Set the source files and headers files
#	TARGET_SRC := xxx_1.c xxx_2.c
#	TARGET_INC := xxx_1.h xxx_2.h
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

TARGET_PATH := $(call my-dir)

#########################################
include $(ENV_CLEAR)

include $(TARGET_TOP)/middleware/config/mpp_config.mk

ifeq ($(MPPCFG_COMPILE_DYNAMIC_LIB), Y)
include $(TARGET_PATH)/../config.mk

TARGET_SRC := \
	$(notdir $(wildcard $(TARGET_PATH)/*.c))

TARGET_INC := \
	$(TARGET_PATH)/include \
	$(TARGET_PATH)/../include \
	$(TARGET_TOP)/system/public/libion/include \
	$(TARGET_TOP)/system/public/libion/kernel-headers \
	
TARGET_CFLAGS += -fPIC -Wall

TARGET_LDFLAGS := -shared -ldl

TARGET_MODULE := libcdc_base

include $(BUILD_SHARED_LIB)

endif


ifeq ($(MPPCFG_COMPILE_STATIC_LIB), Y)

include $(ENV_CLEAR)
include $(TARGET_PATH)/../config.mk

TARGET_SRC := \
	$(notdir $(wildcard $(TARGET_PATH)/*.c))

TARGET_INC := \
	$(TARGET_PATH)/include \
	$(TARGET_PATH)/../include \
	$(TARGET_TOP)/system/public/libion/include \
	$(TARGET_TOP)/system/public/libion/kernel-headers \
	
TARGET_CFLAGS += -fPIC -Wall

TARGET_MODULE := libcdc_base

include $(BUILD_STATIC_LIB)

endif

