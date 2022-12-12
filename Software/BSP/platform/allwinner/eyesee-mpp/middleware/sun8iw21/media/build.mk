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
    mpi_sys.c \
    mpi_aenc.c \
    mpi_tenc.c \
    mpi_adec.c \
    mpi_ai.c \
    mpi_ao.c \
    mpi_venc.c \
    mpi_mux.c \
    mpi_demux.c \
    mpi_vdec.c \
    mpi_clock.c \
	mpi_region.c \
	videoIn/uvcInput.c \
    component/AIChannel_Component.c \
    component/AOChannel_Component.c \
    component/RecRenderSink.c \
    component/RecRender_cache.c \
    component/RecRender_Component.c \
    component/AudioEnc_Component.c \
    component/TextEnc_Component.c \
    component/VideoEnc_Component.c \
	component/Demux_Component.c \
	component/VideoDec_Component.c \
	component/VideoDec_InputThread.c \
	component/AudioDec_Component.c \
	component/Clock_Component.c \
	component/RecAVSync.c \
    audio/alsa_interface.c \
    audio/audio_hw.c \
    audio/pcmBufferManager.c \

#ChannelRegionInfo.c \
#mpi_vi.c \
#component/VideoVirVi_Component.c \
#videoIn/videoInputHw.c \
#videoIn/videoInputBufferManager.c \
#component/ComponentsRegistryTable.c \
#component/mm_component.c \
#mpi_videoformat_conversion.c \
#media_common.c \
#LIBRARY/libisp/isp.c \
#LIBRARY/libisp/isp_tuning/isp_tuning.c \
#LIBRARY/libisp/isp_manage/isp_manage.c \
#LIBRARY/libisp/isp_events/events.c \
#mpi_isp.c \
#mpi_vo.c \
#component/VideoRender_Component.c \


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
	$(TARGET_TOP)/middleware/media/component \
    $(TARGET_TOP)/middleware/media/include/include_render \
    $(TARGET_TOP)/middleware/media/include/videoIn \
    $(TARGET_TOP)/middleware/media/include/audio \
    $(TARGET_TOP)/middleware/media/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/libcedarc/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/AudioLib/osal \
    $(TARGET_TOP)/middleware/media/LIBRARY/AudioLib/midware/encoding/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/AudioLib/midware/decoding/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/audioEffectLib/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/textEncLib/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/include_demux \
    $(TARGET_TOP)/middleware/media/LIBRARY/include_muxer \
    $(TARGET_TOP)/middleware/media/LIBRARY/include_FsWriter \
    $(TARGET_TOP)/middleware/media/LIBRARY/include_stream \
    $(TARGET_TOP)/middleware/media/LIBRARY/libmuxer/mp4_muxer \
    $(TARGET_TOP)/middleware/media/LIBRARY/libcedarx/libcore/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/libcedarx/libcore/base/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/libcedarx/libcore/common/iniparser \
    $(TARGET_TOP)/middleware/media/LIBRARY/libcedarx/libcore/common/plugin \
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
    $(TARGET_TOP)/middleware/media/LIBRARY/libVideoStabilization \
    $(TARGET_TOP)/middleware/media/LIBRARY/libkfc/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/lib_aw_ai_core/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/lib_aw_ai_mt/include 

#TARGET_SHARED_LIB := \
#    libpthread \
#    librt

TARGET_SHARED_LIB := \
    librt \
    libpthread \
    libion \
    liblog \
    libmedia_utils \
    libcedarxstream \
    libMemAdapter \
    libvencoder \
    libvenc_base \
    libvenc_codec \
    libcdx_common \
    libcdx_parser \
    libcdx_stream \
    libvdecoder \
    libadecoder \
    libnormal_audio \
    libhwdisplay \
    libcutils \
    libcedarx_aencoder \
    libcedarx_tencoder \
    libasound \
    libmpp_vi \
    libmpp_isp \
    libmpp_vo \
    libmpp_component

ifeq ($(MPPCFG_ISE),Y)
TARGET_SHARED_LIB += \
    libmpp_ise
endif

ifeq ($(MPPCFG_EIS),Y)
TARGET_SHARED_LIB += \
    libmpp_eis
endif

ifeq ($(MPPCFG_AUDIO_EFFECT_EQ),Y)
    TARGET_SHARED_LIB += libAudioEq
endif
ifeq ($(MPPCFG_AUDIO_EFFECT_GAIN),Y)
    TARGET_SHARED_LIB += libAudioGain
endif
ifeq ($(MPPCFG_AUDIO_EFFECT_RESAMPLE),Y)
    TARGET_SHARED_LIB += libAudioResample
endif
ifeq ($(MPPCFG_AUDIO_EFFECT_AEC),Y)
    TARGET_SHARED_LIB += libAudioAec
endif
ifeq ($(MPPCFG_AUDIO_EFFECT_DRC),Y)
    TARGET_SHARED_LIB += libAudioDrc
endif
ifeq ($(MPPCFG_AUDIO_EFFECT_RNR),Y)
    TARGET_SHARED_LIB += libAudioNosc
endif
ifeq ($(MPPCFG_UVC),Y)
    TARGET_SHARED_LIB += libmpp_uvc
endif

TARGET_STATIC_LIB := \
    libmuxers \
    libmp3_muxer \
    libaac_muxer \
    libmp4_muxer \
    libmpeg2ts_muxer \
    libffavutil \
    libraw_muxer \
    libFsWriter \
    libcedarxdemuxer \

ifeq ($(MPPCFG_USE_KFC),Y)
    TARGET_STATIC_LIB += lib_hal
endif

#libisp_dev \
#libisp_base \
#libisp_math \
#libisp_ae \
#libisp_af \
#libisp_afs \
#libisp_awb \
#libisp_md \
#libisp_iso \
#libisp_gtm \
#libisp_ini \
#libiniparser \
#libisp_rolloff \
#libisp_pltm \
#libmatrix \
#libcedarxrender \


TARGET_CFLAGS += -fPIC -Wall -Wno-unused-function -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-label -D__OS_LINUX

#TARGET_LDFLAGS += -static
#	-L $(TARGET_PATH)/LIBRARY/libisp/out \
#	-L $(TARGET_PATH)/LIBRARY/libisp \
#	-lisp_dev -lisp_base -lisp_math -lisp_ae -lisp_af -lisp_afs \
#	-lisp_awb -lisp_md -lisp_iso -lisp_gtm -lisp_ini -liniparser \
#	-lpthread -lrt

TARGET_MODULE := libmedia_mpp

include $(BUILD_SHARED_LIB)

endif
#########################################


#########################################
#include $(ENV_CLEAR)
#include $(TARGET_PATH)/LIBRARY/libisp/build.mk

