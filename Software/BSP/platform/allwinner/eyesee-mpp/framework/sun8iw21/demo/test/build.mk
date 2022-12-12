TARGET_PATH :=$(call my-dir)

#########################################
include $(ENV_CLEAR)

TARGET_SRC := demo.cpp
TARGET_INC := $(TARGET_PATH)/include

TARGET_CPPFLAGS +=
TARGET_LDFLAGS +=

TARGET_MODULE := libdemo

include $(BUILD_STATIC_LIB)
#########################################

include $(ENV_CLEAR)

TARGET_SRC := demo.cpp
TARGET_INC := $(TARGET_PATH)/include

TARGET_CPPFLAGS +=
TARGET_LDFLAGS +=

TARGET_MODULE := libdemo

include $(BUILD_SHARED_LIB)
########################################

include $(ENV_CLEAR)

TARGET_SRC := main.cpp
TARGET_INC := $(TARGET_PATH)/include

TARGET_CPPFLAGS += -v
TARGET_LDFLAGS += -ldemo -static

TARGET_MODULE := main

include $(BUILD_BIN)
