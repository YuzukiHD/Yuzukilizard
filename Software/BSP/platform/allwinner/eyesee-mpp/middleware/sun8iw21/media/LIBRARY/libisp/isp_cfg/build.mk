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

SRC_TAGS := ./
TARGET_SRC := $(call all-c-files-under, $(SRC_TAGS))
TARGET_INC := \
	$(TARGET_PATH)/SENSOR_H \
	$(TARGET_PATH)/../iniparser/src/ \
	$(TARGET_PATH)/../include/ \
	$(TARGET_PATH)/../isp_dev/
TARGET_CFLAGS := -fPIC -Wall

ifeq ($(strip $(SENSOR_NAME)), imx317)
	TARGET_CFLAGS += -DSENSOR_IMX317=1
endif

ifeq ($(strip $(SENSOR_NAME)), imx278)
	TARGET_CFLAGS += -DSENSOR_IMX278=1
endif

ifeq ($(strip $(SENSOR_NAME)), imx386)
	TARGET_CFLAGS += -DSENSOR_IMX386=1
endif

TARGET_MODULE := libisp_ini
include $(BUILD_STATIC_LIB)


ifeq ($(MPPCFG_COMPILE_DYNAMIC_LIB), Y)

include $(ENV_CLEAR)

SRC_TAGS := ./
TARGET_SRC := $(call all-c-files-under, $(SRC_TAGS))
TARGET_INC := \
	$(TARGET_PATH)/SENSOR_H \
	$(TARGET_PATH)/../iniparser/src/ \
	$(TARGET_PATH)/../include/ \
	$(TARGET_PATH)/../isp_dev/
TARGET_CFLAGS := -fPIC -Wall

ifeq ($(strip $(SENSOR_NAME)), imx317)
	TARGET_CFLAGS += -DSENSOR_IMX317=1
endif

ifeq ($(strip $(SENSOR_NAME)), imx278)
	TARGET_CFLAGS += -DSENSOR_IMX278=1
endif

ifeq ($(strip $(SENSOR_NAME)), imx386)
	TARGET_CFLAGS += -DSENSOR_IMX386=1
endif

TARGET_MODULE := libisp_ini
include $(BUILD_SHARED_LIB)

endif

