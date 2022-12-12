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
#include $(ENV_CLEAR)
#include $(TARGET_PATH)/LIBRARY/libisp/build.mk
#########################################
include $(ENV_CLEAR)
TARGET_SRC := sever.c isp_tuning_cfg_api.c \ 

TARGET_INC := \
	$(TARGET_PATH)/include \
	$(TARGET_PATH)/../include/utils \
	$(TARGET_PATH)/../include \
	$(TARGET_TOP)/middleware/include \
	$(TARGET_TOP)/middleware/include/utils \
	$(TARGET_TOP)/middleware/include/media \
	$(TARGET_TOP)/middleware/media/LIBRARY \
	$(TARGET_TOP)/middleware/media/LIBRARY/libisp \
	$(TARGET_TOP)/middleware/media/LIBRARY/libisp/include \
	$(TARGET_TOP)/middleware/media/LIBRARY/libisp/isp_tuning \
	$(TARGET_TOP)/middleware/media/LIBRARY/libisp/include/V4l2Camera \
	$(TARGET_TOP)/middleware/media/LIBRARY/libcedarc/include \
	$(TARGET_TOP)/middleware/media/isp_tool/include \
  $(TARGET_TOP)/middleware/media/include/component \

TARGET_SHARED_LIB := \
    libpthread \
    liblog \
    libmedia_utils \
    libmedia_mpp \
		libMemAdapter \
		libvencoder \

TARGET_STATIC_LIB := \
#    libisp_dev \
#    libisp_base \
#    libisp_math \
#    libisp_ae \
#    libisp_af \
#    libisp_afs \
#    libisp_awb \
#    libisp_md \
#    libisp_iso \
#    libisp_gtm \
#    libisp_ini \
#    libisp_rolloff \

TARGET_CFLAGS := -O2 -fPIC -Wall
#TARGET_LDFLAGS += \
#	-L $(TARGET_PATH)/LIBRARY/libisp/out \
#	-L $(TARGET_PATH)/LIBRARY/libisp \
#	-lisp_dev -lisp_base -lisp_math -lisp_ae -lisp_af -lisp_afs \
#	-lisp_awb -lisp_md -lisp_iso -lisp_gtm -lisp_ini -liniparser \
#	-lpthread -lrt -static
TARGET_MODULE := isp_tool
object += $(object) $(TARGET_MODULE)

#include $(BUILD_BIN)
