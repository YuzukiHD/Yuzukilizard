LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE    := setup
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := main_linux.c easy_setup_linux.c easy_setup/easy_setup.c easy_setup/scan.c proto/bcast.c proto/neeze.c proto/akiss.c proto/changhong.c proto/jingdong.c proto/xiaoyi.c
LOCAL_SRC_FILES += proto/jd.c
LOCAL_CFLAGS := -I$(LOCAL_PATH)/proto -I$(LOCAL_PATH)/easy_setup
LOCAL_LDFLAGS := -static
LOCAL_STATIC_LIBRARIES := libc
LOCAL_FORCE_STATIC_EXECUTABLE := true

#include $(BUILD_SHARED_LIBRARY)
include $(BUILD_EXECUTABLE)
