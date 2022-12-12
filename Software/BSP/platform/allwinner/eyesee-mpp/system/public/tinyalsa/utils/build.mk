TARGET_PATH :=$(call my-dir)

#########################################
include $(ENV_CLEAR)
TARGET_SRC := tinyplay.c
TARGET_INC := $(TARGET_TOP)/custom_aw/include $(TARGET_PATH)/../include
TARGET_STATIC_LIB := libtinyalsa
TARGET_CFLAGS += \
	-fPIC
TARGET_LDFLAGS += \
	-lpthread \

TARGET_MODULE := tinyplay
include $(BUILD_BIN)

#########################################
include $(ENV_CLEAR)
TARGET_SRC := tinycap.c
TARGET_INC := $(TARGET_TOP)/custom_aw/include $(TARGET_PATH)/../include
TARGET_STATIC_LIB := libtinyalsa
TARGET_CFLAGS += \
	-fPIC
TARGET_LDFLAGS += \
	-lpthread \

TARGET_MODULE := tinycap
include $(BUILD_BIN)


#########################################
include $(ENV_CLEAR)
TARGET_SRC := tinymix.c
TARGET_INC := $(TARGET_TOP)/custom_aw/include $(TARGET_PATH)/../include
TARGET_STATIC_LIB := libtinyalsa
TARGET_CFLAGS += \
	-fPIC
TARGET_LDFLAGS += \
	-lpthread \

TARGET_MODULE := tinymix
include $(BUILD_BIN)

