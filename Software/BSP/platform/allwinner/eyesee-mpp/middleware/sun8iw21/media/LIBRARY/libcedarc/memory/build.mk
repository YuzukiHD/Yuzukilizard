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
	memoryAdapter.c \
	ionMemory/ionAlloc.c

TARGET_INC := \
	$(TARGET_PATH) \
	$(TARGET_PATH)/ionMemory \
	$(TARGET_PATH)/../include \
	$(TARGET_PATH)/../ve/include \
	$(TARGET_PATH)/../base/include
	
TARGET_CFLAGS += -fPIC -Wall -Wno-unused-variable -Wno-unused-but-set-variable

SHARED_LIBS_PATHS := -L$(TARGET_PATH)/../ve/
TARGET_LDFLAGS := -shared -ldl -lVE $(SHARED_LIBS_PATHS)

TARGET_MODULE := libMemAdapter

include $(BUILD_SHARED_LIB)

endif


ifeq ($(MPPCFG_COMPILE_STATIC_LIB), Y)

include $(ENV_CLEAR)
include $(TARGET_PATH)/../config.mk

TARGET_SRC := \
	memoryAdapter.c \
	ionMemory/ionAlloc.c

TARGET_INC := \
	$(TARGET_PATH) \
	$(TARGET_PATH)/ionMemory \
	$(TARGET_PATH)/../include \
	$(TARGET_PATH)/../ve/include \
	$(TARGET_PATH)/../base/include
	
TARGET_CFLAGS += -fPIC -Wall -Wno-unused-variable -Wno-unused-but-set-variable

TARGET_MODULE := libMemAdapter

include $(BUILD_STATIC_LIB)

endif

