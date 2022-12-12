TARGET_PATH :=$(call my-dir)
TOP_PATH := $(TARGET_PATH)

#########################################
include $(ENV_CLEAR)

include $(TARGET_TOP)/middleware/config/mpp_config.mk

TARGET_SRC := \
	sample_RecorderSegment.cpp

TARGET_INC := \
	$(TARGET_TOP)/framework/include \
    $(TARGET_TOP)/framework/include/media \
    $(TARGET_TOP)/framework/include/media/camera \
    $(TARGET_TOP)/framework/include/media/ise \
    $(TARGET_TOP)/framework/include/media/recorder \
    $(TARGET_TOP)/framework/include/utils \
    $(TARGET_TOP)/custom_aw/include \
    $(TARGET_TOP)/custom_aw/include/media \
    $(TARGET_TOP)/custom_aw/include/media/camera \
    $(TARGET_TOP)/custom_aw/include/media/ise \
    $(TARGET_TOP)/custom_aw/include/media/recorder \
    $(TARGET_TOP)/custom_aw/include/utils \
    $(TARGET_TOP)/system/public/include \
    $(TARGET_TOP)/system/public/include/vo \
    $(TARGET_TOP)/middleware/include \
    $(TARGET_TOP)/middleware/include/utils \
    $(TARGET_TOP)/middleware/include/media \
    $(TARGET_TOP)/middleware/media/include/utils \
    $(TARGET_TOP)/middleware/media/include/component \
    $(TARGET_TOP)/middleware/media/LIBRARY/libisp/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/libisp/include/V4l2Camera \
    $(TARGET_TOP)/middleware/media/LIBRARY/libisp/isp_tuning \
    $(TARGET_TOP)/middleware/media/LIBRARY/libISE \
	$(TARGET_TOP)/middleware/media/LIBRARY/include_ai_common \
	$(TARGET_TOP)/middleware/media/LIBRARY/include_eve_common \
    $(TARGET_TOP)/middleware/media/LIBRARY/libaiMOD/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/libVLPR/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/libeveface/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/include_stream \
    $(TARGET_TOP)/middleware/media/LIBRARY/include_FsWriter \
    $(TARGET_TOP)/middleware/media/LIBRARY/include_muxer \
    $(TARGET_TOP)/middleware/media/LIBRARY/libcedarc/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/libcedarx/libcore/common/iniparser \
    $(TARGET_TOP)/middleware/media/LIBRARY/include_eve_common \
    $(TARGET_TOP)/middleware/sample/configfileparser

ifeq ($(MPPCFG_COMPILE_DYNAMIC_LIB), Y)
TARGET_SHARED_LIB := \
	liblog \
	libpthread \
   	libhwdisplay \
   	libcdx_common \
   	libsample_confparser \
   	libcedarxrender \
   	libmpp_vi \
   	libmpp_isp \
   	libmpp_vo \
   	libmpp_component \
   	libmedia_utils \
	libmpp_ise \
    libmpp_uvc \
   	libmedia_mpp \
   	libISP \
   	libMemAdapter \
   	libvencoder \
   	libcustomaw_media_utils \

ifeq ($(MPPCFG_MOD),Y)
TARGET_SHARED_LIB += libai_MOD
endif
ifeq ($(MPPCFG_EVEFACE),Y)
TARGET_SHARED_LIB += libeve_event
endif
ifeq ($(MPPCFG_VLPR),Y)
TARGET_SHARED_LIB += libai_VLPR
endif

ifneq ($(MPPCFG_MOD)_$(MPPCFG_VLPR),N_N)
TARGET_SHARED_LIB += libCVEMiddleInterface
endif

TARGET_STATIC_LIB := \
    libcamera \
    librecorder

ifeq ($(MPPCFG_ISE),Y)
TARGET_STATIC_LIB += \
    libise
endif

else ifeq ($(MPPCFG_COMPILE_STATIC_LIB), Y)
TARGET_SHARED_LIB := \
    libdl \
    libpthread \
    liblog \
    libsample_confparser \
    libcdx_base \
    libcdx_parser \
    libcdx_stream \
    libcdx_common \
    libasound

ifeq ($(MPPCFG_ISE),Y)
TARGET_SHARED_LIB += \
    lib_ise_bi \
    lib_ise_bi_soft \
    lib_ise_mo \
    lib_ise_sti
endif

ifeq ($(MPPCFG_EVEFACE),Y)
TARGET_SHARED_LIB += \
    libeve_event
endif

TARGET_STATIC_LIB := \
    librecorder \
    libcamera \
    libcustomaw_media_utils \
    libaw_mpp \
    libcedarxdemuxer \
    libmuxers \
    libmp4_muxer \
    libraw_muxer \
    libmpeg2ts_muxer \
    libaac_muxer \
    libmp3_muxer \
    libffavutil \
    libFsWriter \
    libcedarxstream \
    libadecoder \
    libcedarx_aencoder \
    libaacenc \
    libadpcmenc \
    libg711enc \
    libg726enc \
    libmp3enc \
    libvdecoder \
    libvideoengine \
    libawh264 \
    libawh265 \
    libawmjpegplus \
    libvencoder \
    libvenc_codec \
    libvenc_base \
    libMemAdapter \
    libVE \
    libcdc_base \
    libISP \
    libisp_ae \
    libisp_af \
    libisp_afs \
    libisp_awb \
    libisp_base \
    libisp_gtm \
    libisp_iso \
    libisp_math \
    libisp_md \
    libisp_pltm \
    libisp_rolloff \
    libmatrix \
    libiniparser \
    libisp_ini \
    libisp_dev \
    libmedia_utils \
    libhwdisplay \
    libion \
    libcutils

ifeq ($(MPPCFG_ISE),Y)
TARGET_STATIC_LIB += \
    libise
endif

ifeq ($(MPPCFG_USE_KFC),Y)
TARGET_STATIC_LIB += lib_hal
endif

endif

TARGET_CPPFLAGS += $(CEDARX_EXT_CFLAGS)
TARGET_CFLAGS += $(CEDARX_EXT_CFLAGS)

TARGET_CPPFLAGS += -fPIC -Wall -Wno-unused-but-set-variable
TARGET_CFLAGS += -fPIC -Wall -Wno-unused-but-set-variable

TARGET_MODULE := sample_RecorderSegment

include $(BUILD_BIN)
#########################################
