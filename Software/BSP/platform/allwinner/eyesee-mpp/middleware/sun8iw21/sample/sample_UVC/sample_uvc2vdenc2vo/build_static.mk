TARGET_PATH :=$(call my-dir)
TOP_PATH := $(TARGET_PATH)

#########################################
include $(ENV_CLEAR)

include $(TARGET_TOP)/middleware/config/mpp_config.mk

ifeq ($(MPPCFG_COMPILE_STATIC_LIB), Y)
TARGET_SRC := \
    sample_uvc2vdenc2vo.c

TARGET_INC := \
    $(TARGET_TOP)/system/public/include \
    $(TARGET_TOP)/middleware/include \
    $(TARGET_TOP)/middleware/include/utils \
    $(TARGET_TOP)/middleware/include/media \
    $(TARGET_TOP)/middleware/media/include \
    $(TARGET_TOP)/middleware/media/include/utils \
    $(TARGET_TOP)/middleware/media/include/component \
    $(TARGET_TOP)/middleware/media/LIBRARY/libisp/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/libisp/include/V4l2Camera \
    $(TARGET_TOP)/middleware/media/LIBRARY/libisp/isp_tuning \
    $(TARGET_TOP)/middleware/media/LIBRARY/libAIE_Vda/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/include_stream \
    $(TARGET_TOP)/middleware/media/LIBRARY/include_FsWriter \
    $(TARGET_TOP)/middleware/media/LIBRARY/libcedarc/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/libcedarx/libcore/common/iniparser \
    $(TARGET_TOP)/middleware/sample/configfileparser \

TARGET_SHARED_LIB := \
    librt \
    libpthread \
    liblog \
    libasound \
    libcdx_parser \
    libcdx_common \
    libcdx_base \
    libsample_confparser

TARGET_STATIC_LIB := \
    libaw_mpp \
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
    libmuxers \
    libmp4_muxer \
    libraw_muxer \
    libmpeg2ts_muxer \
    libaac_muxer \
    libmp3_muxer \
    libffavutil \
    libFsWriter \
    libcedarxstream \
    libcedarxdemuxer \
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
    libion \
    libhwdisplay \
    libcutils

ifeq ($(MPPCFG_USE_KFC),Y)
TARGET_STATIC_LIB += lib_hal
endif

ifeq ($(MPPCFG_ISE),Y)
TARGET_STATIC_LIB += \
    libise \
    lib_ise_mo
endif
#lib_ise_bi \
#lib_ise_sti

TARGET_CFLAGS += -fPIC -Wall -Wno-unused-but-set-variable -ldl

TARGET_MODULE := sample_uvc2vdenc2vo

include $(BUILD_BIN)

else
    $(warning "warning: static compile, but MPPCFG_COMPILE_STATIC_LIB = $(MPPCFG_COMPILE_STATIC_LIB)")
endif

#########################################
