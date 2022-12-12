TARGET_PATH :=$(call my-dir)

#########################################
include $(ENV_CLEAR)

TARGET_SRC := \
	audio_hwdrc_app.c

TARGET_INC := \
    $(TARGET_TOP)/system/include \
    $(TARGET_TOP)/framework/include \

TARGET_SHARED_LIB := \
	liblog \
	libpthread \

TARGET_CPPFLAGS += -fPIC -Wall

TARGET_MODULE := audio_hwdrc_app

include $(BUILD_BIN)
#########################################
