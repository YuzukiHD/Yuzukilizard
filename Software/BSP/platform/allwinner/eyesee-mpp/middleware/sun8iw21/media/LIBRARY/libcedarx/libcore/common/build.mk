TARGET_PATH :=$(call my-dir)

include $(ENV_CLEAR)

include $(TARGET_PATH)/../../config.mk

TARGET_SRC := \
    ./iniparser/dictionary.c   \
    ./iniparser/iniparserapi.c \
    ./iniparser/iniparser.c    \
    ./plugin/cdx_plugin.c

TARGET_INC := \
    $(TARGET_PATH)/../base/include   \
    $(TARGET_PATH)/iniparser         \
    $(TARGET_PATH)/plugin

TARGET_SHARED_LIB := \
    libcdx_base

TARGET_STATIC_LIB := \


TARGET_CFLAGS += -fPIC -Wall
#TARGET_CPPFLAGS +=

TARGET_MODULE := libcdx_common

include $(BUILD_SHARED_LIB)

