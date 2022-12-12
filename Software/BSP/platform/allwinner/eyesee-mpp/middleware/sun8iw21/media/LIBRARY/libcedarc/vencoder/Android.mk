LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

SCLIB_TOP=${LOCAL_PATH}/..
include ${SCLIB_TOP}/config.mk

LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES:= vencoder.c

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/base/include \
	$(LOCAL_PATH)/libcodec \
	${SCLIB_TOP}/ve/include \
	${SCLIB_TOP}/include \
	${SCLIB_TOP}/base/include

LOCAL_SHARED_LIBRARIES := \
	libcutils \
	libutils


ifeq ($(CONFIG_COMPILE_STATIC_LIB), y)
    LOCAL_STATIC_LIBRARIES += libVE libcdc_base libMemAdapter libvenc_base libvenc_codec liblog
else
    LOCAL_SHARED_LIBRARIES += libVE libcdc_base libMemAdapter libvenc_base libvenc_codec liblog
endif

LOCAL_MODULE:= libvencoder

include $(BUILD_SHARED_LIBRARY)
#include $(BUILD_EXECUTABLE)





##########################
### compile for vendor ###
##########################

ifeq ($(PIE_AND_NEWER), yes)

include $(CLEAR_VARS)

SCLIB_TOP=${LOCAL_PATH}/..
include ${SCLIB_TOP}/config.mk

LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES:= vencoder.c

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/base/include \
	${SCLIB_TOP}/ve/include \
	${SCLIB_TOP}/include \
	${SCLIB_TOP}/base/include

LOCAL_SHARED_LIBRARIES := \
	libcutils \
	libutils


ifeq ($(CONFIG_COMPILE_STATIC_LIB), y)
    LOCAL_STATIC_LIBRARIES += libVE libcdc_base libMemAdapter libvenc_base libvenc_codec liblog
else
    LOCAL_SHARED_LIBRARIES += libVE.vendor libcdc_base.vendor libMemAdapter.vendor libvenc_base.vendor libvenc_codec.vendor liblog
endif

LOCAL_MODULE := libvencoder.vendor
LOCAL_USE_VNDK := true
LOCAL_INSTALLED_MODULE_STEM := libvencoder.so
LOCAL_PROPRIETARY_MODULE := true

include $(BUILD_SHARED_LIBRARY)

endif

include $(call all-makefiles-under,$(LOCAL_PATH))