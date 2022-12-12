LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

include $(LOCAL_PATH)/../config.mk

LOCAL_SRC_FILES = \
		$(notdir $(wildcard $(LOCAL_PATH)/*.c)) \

LOCAL_SRC_FILES += \
        ./cdcIniparser/cdcDictionary.c   \
        ./cdcIniparser/cdcIniparserapi.c \
        ./cdcIniparser/cdcIniparser.c

LOCAL_C_INCLUDES:= $(LOCAL_PATH)/include    \
                   $(LOCAL_PATH)/../../     \
                   $(LOCAL_PATH)/../include/     \
		   $(LOCAL_PATH)/../memory/ionMemory/     \
                   $(TOP)/frameworks/av/                               \
		   $(TOP)/system/core/libion/include/ \
		   $(TOP)/system/core/include/ \
           $(TOP)/system/core/libion/kernel-headers/ \

LOCAL_CFLAGS += $(CDX_CFLAGS)

LOCAL_MODULE_TAGS := optional
LOCAL_PRELINK_MODULE := false

LOCAL_MODULE:= libcdc_base

LOCAL_SHARED_LIBRARIES:= libcutils liblog

ifeq ($(LOLLIPOP_AND_NEWER), no)
LOCAL_SHARED_LIBRARIES += libcorkscrew
endif

include $(BUILD_SHARED_LIBRARY)

######################################################
ifeq ($(PIE_AND_NEWER), yes)
include $(CLEAR_VARS)

include $(LOCAL_PATH)/../config.mk

LOCAL_SRC_FILES = \
		$(notdir $(wildcard $(LOCAL_PATH)/*.c)) \

LOCAL_SRC_FILES += \
        ./cdcIniparser/cdcDictionary.c   \
        ./cdcIniparser/cdcIniparserapi.c \
        ./cdcIniparser/cdcIniparser.c		

LOCAL_C_INCLUDES:= $(LOCAL_PATH)/include    \
                   $(LOCAL_PATH)/../../     \
                   $(LOCAL_PATH)/../include/     \
		   $(LOCAL_PATH)/../memory/ionMemory/     \
                   $(TOP)/frameworks/av/                               \
		   $(TOP)/system/core/libion/include/ \
		   $(TOP)/system/core/include/ \
           $(TOP)/system/core/libion/kernel-headers/ \

LOCAL_CFLAGS += $(CDX_CFLAGS)
LOCAL_PROPRIETARY_MODULE := true
#LOCAL_MODULE_RELATIVE_PATH := vndk-28
LOCAL_USE_VNDK := true
LOCAL_MODULE_TAGS := optional
LOCAL_PRELINK_MODULE := false

LOCAL_MODULE:= libcdc_base.vendor
LOCAL_INSTALLED_MODULE_STEM := libcdc_base.so

LOCAL_SHARED_LIBRARIES:= libcutils liblog

ifeq ($(LOLLIPOP_AND_NEWER), no)
LOCAL_SHARED_LIBRARIES += libcorkscrew
endif

ifeq ($(CONFIG_COMPILE_STATIC_LIB), y)
    include $(BUILD_STATIC_LIBRARY)
else
    include $(BUILD_SHARED_LIBRARY)
endif

endif
