LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

SCLIB_TOP=${LOCAL_PATH}/..
include ${SCLIB_TOP}/config.mk

current_path := $(LOCAL_PATH)

libvdecoder_src_common   :=  fbm.c
libvdecoder_src_common   +=  pixel_format.c
libvdecoder_src_common   +=  sbm/sbmStream.c
libvdecoder_src_common   +=  sbm/sbmFrameH264.c
libvdecoder_src_common   +=  sbm/sbmFrameH265.c
libvdecoder_src_common   +=  sbm/sbmFrameBase.c
libvdecoder_src_common   +=  vdecoder.c

libvdecoder_inc_common := 	$(current_path) \
                    		$(SCLIB_TOP)/ve/include \
		                    $(SCLIB_TOP)/include \
		                    $(SCLIB_TOP)/base/include \
                                    $(SCLIB_TOP)/base/inclued/gralloc_metadata \
		                    $(LOCAL_PATH)/include \
		                    $(LOCAL_PATH) \
		                    $(LOCAL_PATH)/videoengine/ \

LOCAL_SRC_FILES := $(libvdecoder_src_common)
LOCAL_C_INCLUDES := $(libvdecoder_inc_common)
#LOCAL_CFLAGS :=
LOCAL_LDFLAGS :=


LOCAL_MODULE_TAGS := optional

## add libaw* for eng/user rebuild
LOCAL_SHARED_LIBRARIES := \
	libcutils \
	libutils \
	liblog \
	libui       \
	libdl       
	
$(warning "CONFIG_COMPILE_STATIC_LIB: $(CONFIG_COMPILE_STATIC_LIB)")
ifeq ($(CONFIG_COMPILE_STATIC_LIB), y)
    LOCAL_STATIC_LIBRARIES += libvideoengine
    LOCAL_STATIC_LIBRARIES += libawh264
    LOCAL_STATIC_LIBRARIES += libawh265
    LOCAL_STATIC_LIBRARIES += libawavs
   #LOCAL_STATIC_LIBRARIES += libawh265soft
    LOCAL_STATIC_LIBRARIES += libawmjpeg
    LOCAL_STATIC_LIBRARIES += libawmjpegplus
    LOCAL_STATIC_LIBRARIES += libawmpeg2
    LOCAL_STATIC_LIBRARIES += libawmpeg4dx
    LOCAL_STATIC_LIBRARIES += libawmpeg4h263
    LOCAL_STATIC_LIBRARIES += libawmpeg4normal
    LOCAL_STATIC_LIBRARIES += libawmpeg4vp6
    LOCAL_STATIC_LIBRARIES += libawmpeg4base
    LOCAL_STATIC_LIBRARIES += librv
    LOCAL_STATIC_LIBRARIES += libawvp6soft
    LOCAL_STATIC_LIBRARIES += libawvp8
    LOCAL_STATIC_LIBRARIES += libawvp9Hw
    LOCAL_STATIC_LIBRARIES += libawvp9soft
   #LOCAL_STATIC_LIBRARIES += libawwmv12soft
    LOCAL_STATIC_LIBRARIES += libawwmv3
    LOCAL_STATIC_LIBRARIES += libVE libcdc_base libMemAdapter
else
    LOCAL_SHARED_LIBRARIES += libvideoengine libVE libcdc_base libMemAdapter
endif	

LOCAL_MODULE := libvdecoder
include $(BUILD_SHARED_LIBRARY)

##########################################################################
ifeq ($(PIE_AND_NEWER), yes)
include $(CLEAR_VARS)

include ${SCLIB_TOP}/config.mk

current_path := $(LOCAL_PATH)

libvdecoder_src_common   :=  fbm.c
libvdecoder_src_common   +=  pixel_format.c
libvdecoder_src_common   +=  sbm/sbmStream.c
libvdecoder_src_common   +=  sbm/sbmFrameH264.c
libvdecoder_src_common   +=  sbm/sbmFrameH265.c
libvdecoder_src_common   +=  sbm/sbmFrameBase.c
libvdecoder_src_common   +=  vdecoder.c

libvdecoder_inc_common := 	$(current_path) \
                    		$(SCLIB_TOP)/ve/include \
		                    $(SCLIB_TOP)/include \
		                    $(SCLIB_TOP)/base/include \
                            $(SCLIB_TOP)/base/inclued/gralloc_metadata \
		                    $(LOCAL_PATH)/include \
		                    $(LOCAL_PATH) \
		                    $(LOCAL_PATH)/videoengine/ \

LOCAL_SRC_FILES := $(libvdecoder_src_common)
LOCAL_C_INCLUDES := $(libvdecoder_inc_common)
#LOCAL_CFLAGS :=
LOCAL_LDFLAGS :=

LOCAL_MODULE_TAGS := optional

## add libaw* for eng/user rebuild
LOCAL_SHARED_LIBRARIES := \
	libcutils \
	libutils \
	liblog \
	libui       \
	libdl       \
	libVE.vendor       \
	libcdc_base.vendor   \
	libvideoengine.vendor \
	libMemAdapter.vendor

LOCAL_MODULE := libvdecoder.vendor
LOCAL_USE_VNDK := true
LOCAL_INSTALLED_MODULE_STEM := libvdecoder.so
LOCAL_PROPRIETARY_MODULE := true
include $(BUILD_SHARED_LIBRARY)

endif

include $(call all-makefiles-under,$(LOCAL_PATH))
