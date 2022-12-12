TARGET_PATH := $(call my-dir)

include $(ENV_CLEAR)
TARGET_SRC := ion.c  ion_memmanager.c
TARGET_MODULE := libion
TARGET_SHARED_LIB := liblog
#TARGET_INC := $(TARGET_PATH)/include $(TARGET_PATH)/kernel-headers
TARGET_INC := $(TARGET_TOP)/system/public/include/kernel-headers \
			  $(TARGET_TOP)/middleware/include/utils
TARGET_CFLAGS := -fPIC -Wall
include $(BUILD_SHARED_LIB)

include $(ENV_CLEAR)
TARGET_SRC := ion.c  ion_memmanager.c
TARGET_MODULE := libion
TARGET_SHARED_LIB := liblog
#TARGET_INC := $(TARGET_PATH)/include $(TARGET_PATH)/kernel-headers
TARGET_INC := $(TARGET_TOP)/system/public/include/kernel-headers \
			  $(TARGET_TOP)/middleware/include/utils
TARGET_CFLAGS := -fPIC -Wall
include $(BUILD_STATIC_LIB)

