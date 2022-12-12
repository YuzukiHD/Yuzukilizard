TARGET_PATH:= $(call my-dir)
include $(ENV_CLEAR)

TARGET_SRC := \
	intmath.c \
	mem.c
	
TARGET_INC := \
	$(TARGET_TOP)/middleware/include/utils \
	$(TARGET_TOP)/middleware/media/LIBRARY/libmuxer/common \
	$(TARGET_TOP)/middleware/media/LIBRARY/libmuxer/common/libavutil

TARGET_CPPFLAGS += -fPIC -Wall -Wno-unused-function -Wno-unused-but-set-variable -DHAVE_AV_CONFIG_H
TARGET_CFLAGS += -fPIC -Wall -Wno-unused-function -Wno-unused-but-set-variable -DHAVE_AV_CONFIG_H

TARGET_MODULE:= libffavutil

include $(BUILD_STATIC_LIB)

