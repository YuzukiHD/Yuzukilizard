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
TARGET_SRC := hwdisplay.c
TARGET_INC :=
TARGET_SHARED_LIB :=
TARGET_CFLAGS += \
	-fPIC
TARGET_LDFLAGS += \
	-lpthread \
	-llog

ifeq ("$(strip $(TARGET_PRODUCT))", "V5SDV")
	TARGET_CFLAGS += -DSDV_PRODUCT
endif

ifeq ("$(strip $(TARGET_PRODUCT))", "V5SDVTP")
	TARGET_CFLAGS += -DSDV_PRODUCT
endif

TARGET_MODULE := libhwdisplay
include $(BUILD_STATIC_LIB)

include $(ENV_CLEAR)
TARGET_SRC := hwdisplay.c
TARGET_INC :=
TARGET_SHARED_LIB :=
TARGET_CFLAGS += \
	-fPIC
TARGET_LDFLAGS += \
	-lpthread \
	-llog

ifeq ("$(strip $(TARGET_PRODUCT))", "V5SDV")
	TARGET_CFLAGS += -DSDV_PRODUCT
endif

ifeq ("$(strip $(TARGET_PRODUCT))", "V5SDVTP")
	TARGET_CFLAGS += -DSDV_PRODUCT
endif

TARGET_MODULE := libhwdisplay
include $(BUILD_SHARED_LIB)
