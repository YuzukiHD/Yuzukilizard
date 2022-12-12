#
# 1. Set the path and clear environment
# 	TARGET_PATH := $(call my-dir)
# 	include $(ENV_CLEAR)
#
# 2. Set the source files and headers files
#	TARGET_SRC := xxx_1.c xxx_2.c
#	TARGET_INc := xxx_1.h xxx_2.h
#
# 3. Set the output target
#	TARGET_MODULE := xxx
#
# 4. Include the main makefile
#	include $(BUILD_BIN)
#
# Before include the build makefile, you can set the compilaion
# flags, e.g. TARGET_ASFLAGS TARGET_CFLAGS TARGET_CPPFLAGS
#

TARGET_PATH :=$(call my-dir)
TOP_PATH := $(TARGET_PATH)

# ============================================================================
# start gcc sample_virvi2venc.c
#########################################
include $(ENV_CLEAR)

include $(TARGET_TOP)/middleware/config/mpp_config.mk

TARGET_SRC := sample_virvi2venc.c
TARGET_INC := \
    $(TARGET_TOP)/system/include \
    $(TARGET_TOP)/middleware/include/utils \
    $(TARGET_TOP)/middleware/include/media \
    $(TARGET_TOP)/middleware/include \
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
    libion \
    libhwdisplay \
    libasound \
    libsample_confparser \
    libcdx_base \
    libcdx_parser \
    libcdx_stream \
    libcdx_common \
    libcutils

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

ifeq ($(MPPCFG_ISE),Y)
TARGET_STATIC_LIB += \
    libise \
    lib_ise_mo
endif

ifeq ($(MPPCFG_EIS),Y)
TARGET_STATIC_LIB += \
    libEIS \
    lib_eis
endif

ifeq ($(MPPCFG_USE_KFC),Y)
TARGET_STATIC_LIB += lib_hal
endif

TARGET_CFLAGS += -fPIC -Wall -Wno-unused-but-set-variable -ldl

TARGET_MODULE := sample_virvi2venc

include $(BUILD_BIN)
