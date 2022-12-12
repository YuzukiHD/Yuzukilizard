LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

include $(LOCAL_PATH)/../../config.mk

LOCAL_SRC_FILES = \
        ./iniparser/dictionary.c   \
        ./iniparser/iniparserapi.c \
        ./iniparser/iniparser.c    \
        ./plugin/cdx_plugin.c

LOCAL_C_INCLUDES:=  $(LOCAL_PATH)/../../             \
                    $(LOCAL_PATH)/../include         \
                    $(LOCAL_PATH)/.                  \
                    $(LOCAL_PATH)/../base/include/   \
                    $(LOCAL_PATH)/iniparser          \
                    $(LOCAL_PATH)/plugin

LOCAL_CFLAGS += $(CDX_CFLAGS)

LOCAL_MODULE_TAGS := optional
LOCAL_PRELINK_MODULE := false

LOCAL_MODULE:= libcdx_common

LOCAL_SHARED_LIBRARIES := \
            libcutils     \
            libutils      \
            libcdx_base   \
            libdl \
	    liblog

ifeq ($(TARGET_ARCH),arm)
    LOCAL_CFLAGS += -Wno-psabi
endif

include $(BUILD_SHARED_LIBRARY)

