#
# 1. Set the path and clear environment
#   TARGET_PATH := $(call my-dir)
#   include $(ENV_CLEAR)
#
# 2. Set the source files and headers files
#   TARGET_SRC := xxx_1.c xxx_2.c
#   TARGET_INc := xxx_1.h xxx_2.h
#
# 3. Set the output target
#   TARGET_MODULE := xxx
#
# 4. Include the main makefile
#   include $(BUILD_BIN)
#
# Before include the build makefile, you can set the compilaion
# flags, e.g. TARGET_ASFLAGS TARGET_CFLAGS TARGET_CPPFLAGS
#

TARGET_PATH :=$(call my-dir)
TOP_PATH := $(TARGET_PATH)

# ============================================================================
# start gcc sample_fish.c
#########################################
include $(ENV_CLEAR)

include $(TARGET_TOP)/middleware/config/mpp_config.mk

TARGET_SRC := sample_fish.c
TARGET_INC := \
    $(TARGET_TOP)/middleware/media/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/libcedarc/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/libisp/include/V4l2Camera \
    $(TARGET_TOP)/middleware/media/LIBRARY/libisp/include/device \
    $(TARGET_TOP)/middleware/media/LIBRARY/libisp/isp_dev \
    $(TARGET_TOP)/middleware/include/utils \
    $(TARGET_TOP)/middleware/include/media/ \
    $(TARGET_TOP)/middleware/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/libISE \
    $(TARGET_TOP)/system/include \
    $(TARGET_TOP)/system/public/include \
    $(TARGET_TOP)/system/public/include/vo \
    $(TARGET_TOP)/system/public/libion/include \
    $(TARGET_TOP)/system/public/libion/kernel-headers \
    $(TARGET_TOP)/middleware/include/utils \
    $(TARGET_TOP)/middleware/include/media \
    $(TARGET_TOP)/middleware/include \
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
	libsample_confparser \
    librt \
    libpthread \
    liblog \
    libion \
    libhwdisplay \
    libasound \
    libcedarxstream \
    libnormal_audio \
    libcutils \
    libcdx_common \
    libcdx_base \
    libcdx_parser \

ifeq ($(MPPCFG_VIDEOSTABILIZATION),Y)
TARGET_SHARED_LIB += libIRIDALABS_ViSta
endif

TARGET_STATIC_LIB := \
    libaw_mpp \
    libmedia_utils \
    libcedarx_aencoder \
    libaacenc \
    libadpcmenc \
    libg711enc \
    libg726enc \
    libmp3enc \
    libadecoder \
    libcedarxdemuxer \
    libAwNosc \
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
    libisp_dev \
    libisp_ini \
    lib_ise_mo \
    libiniparser \
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
    libiniparser \
    libisp_ini \
    libisp_dev \
    libmuxers \
    libmp4_muxer \
    libraw_muxer \
    libmpeg2ts_muxer \
    libaac_muxer \
    libmp3_muxer \
    libffavutil \
    libFsWriter \
    libcedarxstream \
    libion \

ifeq ($(MPPCFG_USE_KFC),Y)
TARGET_STATIC_LIB += lib_hal
endif

ifeq ($(MPPCFG_EIS),Y)
TARGET_STATIC_LIB += libEIS \
    lib_eis
endif

ifeq ($(MPPCFG_ISE_BI),Y)
TARGET_STATIC_LIB += lib_ise_bi
endif

ifeq ($(MPPCFG_ISE_MO),Y)
TARGET_STATIC_LIB += lib_ise_mo
endif

ifeq ($(MPPCFG_ISE_STI),Y)
TARGET_STATIC_LIB += lib_ise_sti
endif

TARGET_CFLAGS += -fPIC -Wall -Wno-unused-but-set-variable -ldl



TARGET_CFLAGS += -fPIC -Wall -Wno-unused-but-set-variable -ldl

#TARGET_LDFLAGS += \
#   -L $(TARGET_PATH)/LIBRARY/libisp/out \
#   -L $(TARGET_PATH)/LIBRARY/libisp \
#   -lisp_dev -lisp_base -lisp_math -lisp_ae -lisp_af -lisp_afs \
#   -lisp_awb -lisp_md -lisp_iso -lisp_gtm -lisp_ini -liniparser \
#   -lpthread -lrt -static
    TARGET_MODULE := sample_fish
    object += $(object) $(TARGET_MODULE)
    include $(BUILD_BIN)

#########################################
