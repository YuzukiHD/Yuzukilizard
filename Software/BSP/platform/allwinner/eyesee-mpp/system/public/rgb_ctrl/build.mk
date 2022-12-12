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
TARGET_SRC := rgb_ctrl.c lz4f_decompress.c
TARGET_INC := \
    $(TARGET_TOP)/external/lz4-1.7.5/lib

TARGET_STATIC_LIB := liblz4
TARGET_CFLAGS += \
        -fPIC
TARGET_LDFLAGS += \
        -lpthread \
        -llog

TARGET_MODULE := librgb_ctrl
include $(BUILD_SHARED_LIB)

include $(ENV_CLEAR)
TARGET_SRC := rgb_ctrl.c lz4f_decompress.c
TARGET_INC := \
    $(TARGET_TOP)/external/lz4-1.7.5/lib

TARGET_STATIC_LIB := liblz4
TARGET_CFLAGS += \
        -fPIC
TARGET_LDFLAGS += \
        -lpthread \
        -llog

TARGET_MODULE := librgb_ctrl
include $(BUILD_STATIC_LIB)

# compress font file
#
$(call copy-files-under, \
	$(TARGET_PATH)/fonts/*, \
	$(TARGET_OUT)/target/usr/share/osd/fonts \
)

#$(shell cp $(TARGET_PATH)/rgb_ctrl.h $(TARGET_PATH)/../include/rgb_ctrl)
