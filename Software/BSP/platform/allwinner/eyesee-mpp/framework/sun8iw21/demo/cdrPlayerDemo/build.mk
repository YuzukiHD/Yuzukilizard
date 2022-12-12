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

TARGET_PATH:= $(call my-dir)

########################################
include $(ENV_CLEAR)
TARGET_SRC := cdrPlayerDemo.cpp	\
				LannisterView.cpp		\
				LannisterViewMessageHandler.cpp	\
				StarkModel.cpp			\
				StarkModelMessageHandler.cpp	\
				TargaryenControl.cpp		\
				TargaryenControlMessageHandler.cpp	\
				utils/stringOperate.cpp

TARGET_INC := \
    $(TARGET_TOP)/system/public/include/vo \
	$(TARGET_TOP)/middleware/include/utils \
    $(TARGET_TOP)/middleware/include/media \
    $(TARGET_TOP)/middleware/include \
    $(TARGET_TOP)/framework/include/media \
    $(TARGET_TOP)/framework/include/media/camera \
    $(TARGET_TOP)/framework/include/media/ise \
    $(TARGET_TOP)/framework/include/media/recorder \
    $(TARGET_TOP)/framework/include/media/player \
    $(TARGET_TOP)/framework/include/utils \
    $(TARGET_TOP)/framework/include \
	$(TARGET_PATH)/utils \

TARGET_MODULE := cdrPlayerDemo
TARGET_SHARED_LIB := \
    liblog \
    libpthread \
    libhwdisplay \
    libmedia_mpp \
    libmpp_vo \
    libmpp_vi \
    libmpp_isp \
    libmpp_ise \
    libmpp_component \
    libcustomaw_media_utils \
    

TARGET_STATIC_LIB := \
    libcamera \
    libplayer

include $(BUILD_BIN)

