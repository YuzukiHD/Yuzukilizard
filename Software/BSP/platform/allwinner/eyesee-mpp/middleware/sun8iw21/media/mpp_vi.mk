#
# 1. Set the path and clear environment
# 	TARGET_PATH := $(call my-dir)
# 	include $(ENV_CLEAR)
#
# 2. Set the source files and headers files
#	TARGET_SRC := xxx_1.c xxx_2.c
#	TARGET_INC := xxx_1.h xxx_2.h
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
object :=

#########################################
include $(ENV_CLEAR)

include $(TARGET_TOP)/middleware/config/mpp_config.mk

ifeq ($(MPPCFG_COMPILE_DYNAMIC_LIB), Y)
include $(TARGET_TOP)/middleware/media/LIBRARY/libcedarx/config.mk

TARGET_CPPFLAGS += -fPIC $(CEDARX_EXT_CFLAGS)
TARGET_CFLAGS += -fPIC $(CEDARX_EXT_CFLAGS)

TARGET_SRC := \
    ChannelRegionInfo.c \
    mpi_vi.c \
    component/VideoVirVi_Component.c \
    videoIn/videoInputHw.c \
    videoIn/VIPPDrawOSD_V5.c \

ifeq ($(MPPCFG_VIDEOSTABILIZATION),Y)
	TARGET_SRC += videoIn/anti-shake.c
	TARGET_SRC += videoIn/generic_buffer.c
	TARGET_SRC += videoIn/iio_utils.c
endif

TARGET_INC := \
    $(TARGET_TOP)/system/public/include \
    $(TARGET_TOP)/system/public/include/vo \
    $(TARGET_TOP)/system/public/include/kernel-headers \
    $(TARGET_TOP)/system/public/libion/include \
    $(TARGET_TOP)/system/public/libion/kernel-headers \
    $(TARGET_TOP)/middleware/config \
    $(TARGET_TOP)/middleware/include/utils \
    $(TARGET_TOP)/middleware/include/media \
    $(TARGET_TOP)/middleware/include/media/utils \
    $(TARGET_TOP)/middleware/include \
    $(TARGET_TOP)/middleware/media/include/utils \
    $(TARGET_TOP)/middleware/media/include/component \
    $(TARGET_TOP)/middleware/media/include/include_render \
    $(TARGET_TOP)/middleware/media/include/videoIn \
    $(TARGET_TOP)/middleware/media/include/audio \
    $(TARGET_TOP)/middleware/media/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/libVideoStabilization \
    $(TARGET_TOP)/middleware/media/LIBRARY/libcedarc/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/AudioLib/osal \
    $(TARGET_TOP)/middleware/media/LIBRARY/AudioLib/midware/encoding/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/AudioLib/midware/decoding/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/audioEffectLib/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/include_demux \
    $(TARGET_TOP)/middleware/media/LIBRARY/include_muxer \
    $(TARGET_TOP)/middleware/media/LIBRARY/include_FsWriter \
    $(TARGET_TOP)/middleware/media/LIBRARY/include_stream \
    $(TARGET_TOP)/middleware/media/LIBRARY/libmuxer/mp4_muxer \
    $(TARGET_TOP)/middleware/media/LIBRARY/libcedarx/libcore/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/libcedarx/libcore/base/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/libcedarx/libcore/common/iniparser \
    $(TARGET_TOP)/middleware/media/LIBRARY/libcedarx/libcore/parser/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/libcedarx/libcore/stream/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/libcedarx \
    $(TARGET_TOP)/middleware/media/LIBRARY/include_ai_common \
    $(TARGET_TOP)/middleware/media/LIBRARY/libISE \
    $(TARGET_TOP)/middleware/media/LIBRARY/libVLPR/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/libisp/include/V4l2Camera \
    $(TARGET_TOP)/middleware/media/LIBRARY/libisp/include/device \
    $(TARGET_TOP)/middleware/media/LIBRARY/libisp/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/libisp/isp_dev \
    $(TARGET_TOP)/middleware/media/LIBRARY/libisp/isp_tuning \
    $(TARGET_TOP)/middleware/media/LIBRARY/libisp \
    $(TARGET_TOP)/middleware/media/LIBRARY/libkfc/include \

TARGET_SHARED_LIB := \
    libISP \


ifeq ($(MPPCFG_VIDEOSTABILIZATION),Y)
    TARGET_SHARED_LIB += libIRIDALABS_ViSta
endif

TARGET_STATIC_LIB := \

TARGET_CFLAGS += -fPIC -Wall -Wno-unused-function -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-label \

#TARGET_LDFLAGS += -static
#	-L $(TARGET_PATH)/LIBRARY/libisp/out \
#	-L $(TARGET_PATH)/LIBRARY/libisp \
#	-lisp_dev -lisp_base -lisp_math -lisp_ae -lisp_af -lisp_afs \
#	-lisp_awb -lisp_md -lisp_iso -lisp_gtm -lisp_ini -liniparser \
#	-lpthread -lrt

TARGET_MODULE := libmpp_vi

include $(BUILD_SHARED_LIB)

endif

