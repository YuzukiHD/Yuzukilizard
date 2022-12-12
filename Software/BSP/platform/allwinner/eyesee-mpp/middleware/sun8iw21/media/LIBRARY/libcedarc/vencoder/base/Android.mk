LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

SCLIB_TOP=${LOCAL_PATH}/../../
include ${SCLIB_TOP}/config.mk

LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES:= FrameBufferManager.c \
                  BitstreamManager.c \
                  EncAdapter.c

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/include \
	${SCLIB_TOP}/ve/include \
	${SCLIB_TOP}/include \
	${SCLIB_TOP}/base/include

LOCAL_SHARED_LIBRARIES := \
	libcutils \
	libutils


ifeq ($(CONFIG_COMPILE_STATIC_LIB), y)
    LOCAL_STATIC_LIBRARIES += libVE libcdc_base libMemAdapter liblog
else
    LOCAL_SHARED_LIBRARIES += libVE libcdc_base libMemAdapter  liblog
endif

LOCAL_MODULE:= libvenc_base

include $(BUILD_SHARED_LIBRARY)





##########################
### compile for vendor ###
##########################

ifeq ($(PIE_AND_NEWER), yes)

include $(CLEAR_VARS)

SCLIB_TOP=${LOCAL_PATH}/../../
include ${SCLIB_TOP}/config.mk

LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES:= FrameBufferManager.c \
                  BitstreamManager.c \
                  EncAdapter.c

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/include \
	${SCLIB_TOP}/ve/include \
	${SCLIB_TOP}/include \
	${SCLIB_TOP}/base/include

LOCAL_SHARED_LIBRARIES := \
	libcutils \
	libutils


ifeq ($(CONFIG_COMPILE_STATIC_LIB), y)
    LOCAL_STATIC_LIBRARIES += libVE libcdc_base libMemAdapter liblog
else
    LOCAL_SHARED_LIBRARIES += libVE.vendor libcdc_base.vendor libMemAdapter.vendor  liblog
endif

LOCAL_MODULE := libvenc_base.vendor
LOCAL_USE_VNDK := true
LOCAL_INSTALLED_MODULE_STEM := libvenc_base.so
LOCAL_PROPRIETARY_MODULE := true

include $(BUILD_SHARED_LIBRARY)

endif