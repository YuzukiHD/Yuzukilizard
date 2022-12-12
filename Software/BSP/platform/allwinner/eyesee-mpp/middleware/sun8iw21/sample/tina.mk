# Makefile for eyesee-mpp/middleware/sample
CUR_PATH := .
PACKAGE_TOP := ..
EYESEE_MPP_INCLUDE:=$(STAGING_DIR)/usr/include/eyesee-mpp
EYESEE_MPP_LIBDIR:=$(STAGING_DIR)/usr/lib/eyesee-mpp
# STAGING_DIR is exported in rules.mk, so it can be used directly here.
# STAGING_DIR:=.../tina-v316/out/v316-perfnor/staging_dir/target

# used to store all the generated sample bin files
MPP_SAMPLES_BIN_DIR = bin

include $(PACKAGE_TOP)/config/mpp_config.mk

# based on different samples, specify the corresponding configuration
ifeq ($(TARGET), sample_vencRecreate)
SRCCS := sample_vencRecreate/sample_vencRecreate.c
LOCAL_TARGET_BIN := sample_vencRecreate/sample_vencRecreate
endif
ifeq ($(TARGET), sample_takePicture)
SRCCS := sample_takePicture/sample_takePicture.c
LOCAL_TARGET_BIN := sample_takePicture/sample_takePicture
endif
ifeq ($(TARGET), sample_vencGdcZoom)
SRCCS := sample_vencGdcZoom/sample_vencGdcZoom.c
LOCAL_TARGET_BIN := sample_vencGdcZoom/sample_vencGdcZoom
endif

ifeq ($(TARGET), sample_OnlineVenc)
SRCCS := sample_OnlineVenc/sample_OnlineVenc.c
LOCAL_TARGET_BIN := sample_OnlineVenc/sample_OnlineVenc
endif
ifeq ($(TARGET), sample_vencQpMap)
SRCCS := sample_vencQpMap/sample_vencQpMap.c
LOCAL_TARGET_BIN := sample_vencQpMap/sample_vencQpMap
endif
ifeq ($(TARGET), sample_virvi2vo_zoom)
SRCCS := sample_virvi2vo_zoom/sample_virvi2vo_zoom.c
LOCAL_TARGET_BIN := sample_virvi2vo_zoom/sample_virvi2vo_zoom
endif
ifeq ($(TARGET), sample_CodecParallel)
SRCCS := sample_CodecParallel/sample_CodecParallel.c
LOCAL_TARGET_BIN := sample_CodecParallel/sample_CodecParallel
endif

ifeq ($(TARGET), sample_adec)
SRCCS := sample_adec/sample_adec.c
LOCAL_TARGET_BIN := sample_adec/sample_adec
endif
ifeq ($(TARGET), sample_adec2ao)
SRCCS := sample_adec2ao/sample_adec2ao.c
LOCAL_TARGET_BIN := sample_adec2ao/sample_adec2ao
endif
ifeq ($(TARGET), sample_aec)
SRCCS := sample_aec/sample_aec.c
LOCAL_TARGET_BIN := sample_aec/sample_aec
endif
ifeq ($(TARGET), sample_aenc)
SRCCS := sample_aenc/sample_aenc.c
LOCAL_TARGET_BIN := sample_aenc/sample_aenc
endif
ifeq ($(TARGET), sample_ai)
SRCCS := sample_ai/sample_ai.c
LOCAL_TARGET_BIN := sample_ai/sample_ai
endif
ifeq ($(TARGET), sample_ai2aenc)
SRCCS := sample_ai2aenc/sample_ai2aenc.c
LOCAL_TARGET_BIN := sample_ai2aenc/sample_ai2aenc
endif

ifeq ($(TARGET), sample_ai2aenc2muxer)
SRCCS := sample_ai2aenc2muxer/sample_ai2aenc2muxer.c
LOCAL_TARGET_BIN := sample_ai2aenc2muxer/sample_ai2aenc2muxer
endif
ifeq ($(TARGET), sample_ao)
SRCCS := sample_ao/sample_ao.c
LOCAL_TARGET_BIN := sample_ao/sample_ao
endif
ifeq ($(TARGET), sample_ao_resample_mixer)
SRCCS := sample_ao_resample_mixer/sample_ao_resample_mixer.c
LOCAL_TARGET_BIN := sample_ao_resample_mixer/sample_ao_resample_mixer
endif
ifeq ($(TARGET), sample_ai2ao)
SRCCS := sample_ai2ao/sample_ai2ao.c
LOCAL_TARGET_BIN := sample_ai2ao/sample_ai2ao
endif

ifeq ($(TARGET), sample_ao2ai_aec)
SRCCS := sample_ao2ai_aec/sample_ao2ai.c
LOCAL_TARGET_BIN := sample_ao2ai_aec/sample_ao2ai_aec
endif
ifeq ($(TARGET), sample_ao2ai_aec_rate_mixer)
SRCCS := sample_ao2ai_aec_rate_mixer/sample_ao2ai_aec_rate_mixer.c
LOCAL_TARGET_BIN := sample_ao2ai_aec_rate_mixer/sample_ao2ai_aec_rate_mixer
endif
ifeq ($(TARGET), sample_aoSync)
SRCCS := sample_aoSync/sample_aoSync.c
LOCAL_TARGET_BIN := sample_aoSync/sample_aoSync
endif
ifeq ($(TARGET), sample_demux)
SRCCS := sample_demux/sample_demux.c
LOCAL_TARGET_BIN := sample_demux/sample_demux
endif
ifeq ($(TARGET), sample_demux2adec)
SRCCS := sample_demux2adec/sample_demux2adec.c
LOCAL_TARGET_BIN := sample_demux2adec/sample_demux2adec
endif
ifeq ($(TARGET), sample_demux2adec2ao)
SRCCS := sample_demux2adec2ao/sample_demux2adec2ao.c
LOCAL_TARGET_BIN := sample_demux2adec2ao/sample_demux2adec2ao
endif

ifeq ($(TARGET), sample_demux2vdec)
SRCCS := sample_demux2vdec/sample_demux2vdec.c
LOCAL_TARGET_BIN := sample_demux2vdec/sample_demux2vdec
endif
ifeq ($(TARGET), sample_demux2vdec_saveFrame)
SRCCS := sample_demux2vdec_saveFrame/sample_demux2vdec_saveFrame.c
LOCAL_TARGET_BIN := sample_demux2vdec_saveFrame/sample_demux2vdec_saveFrame
endif
ifeq ($(TARGET), sample_demux2vdec2vo)
SRCCS := sample_demux2vdec2vo/sample_demux2vdec2vo.c
LOCAL_TARGET_BIN := sample_demux2vdec2vo/sample_demux2vdec2vo
endif
ifeq ($(TARGET), sample_directIORead)
SRCCS := sample_directIORead/sample_directIORead.c
LOCAL_TARGET_BIN := sample_directIORead/sample_directIORead
endif
ifeq ($(TARGET), sample_driverVipp)
SRCCS := sample_driverVipp/sample_driverVipp.c
LOCAL_TARGET_BIN := sample_driverVipp/sample_driverVipp
endif

ifeq ($(TARGET), sample_file_repair)
SRCCS := sample_file_repair/sample_file_repair.c
LOCAL_TARGET_BIN := sample_file_repair/sample_file_repair
endif
ifeq ($(TARGET), sample_fish)
SRCCS := sample_fish/sample_fish.c
LOCAL_TARGET_BIN := sample_fish/sample_fish
endif
ifeq ($(TARGET), sample_g2d)
SRCCS := \
    sample_g2d/sample_g2d.c \
    sample_g2d/sample_g2d_mem.c
LOCAL_TARGET_BIN := sample_g2d/sample_g2d
endif
ifeq ($(TARGET), sample_glog)
SRCCS := sample_glog/sample_glog.cpp
LOCAL_TARGET_BIN := sample_glog/sample_glog
endif
ifeq ($(TARGET), sample_hello)
SRCCS := sample_hello/sample_hello.c
LOCAL_TARGET_BIN := sample_hello/sample_hello
endif

# librgb_ctrl, and librgb_ctrl depends on liblz4
# eyesee-mpp\system\public\rgb_ctrl
# eyesee-mpp\external\lz4-1.7.5
ifeq ($(TARGET), sample_isposd)
SRCCS := sample_isposd/sample_isposd.c
LOCAL_TARGET_BIN := sample_isposd/sample_isposd
endif
ifeq ($(TARGET), sample_MotionDetect)
SRCCS := sample_MotionDetect/sample_MotionDetect.c
LOCAL_TARGET_BIN := sample_MotionDetect/sample_MotionDetect
endif
ifeq ($(TARGET), sample_motor)
SRCCS := \
    sample_motor/sample_motor.c \
	sample_motor/fan_control.c
LOCAL_TARGET_BIN := sample_motor/sample_motor
endif
ifeq ($(TARGET), sample_multi_vi2venc2muxer)
SRCCS := sample_multi_vi2venc2muxer/sample_multi_vi2venc2muxer.c
LOCAL_TARGET_BIN := sample_multi_vi2venc2muxer/sample_multi_vi2venc2muxer
endif
ifeq ($(TARGET), sample_PersonDetect)
SRCCS := sample_PersonDetect/sample_PersonDetect.c
LOCAL_TARGET_BIN := sample_PersonDetect/sample_PersonDetect
endif
ifeq ($(TARGET), sample_pthread_cancel)
SRCCS := sample_pthread_cancel/sample_pthread_cancel.c
LOCAL_TARGET_BIN := sample_pthread_cancel/sample_pthread_cancel
endif
ifeq ($(TARGET), sample_region)
SRCCS := sample_region/sample_region.c
LOCAL_TARGET_BIN := sample_region/sample_region
endif
ifeq ($(TARGET), sample_RegionDetect)
SRCCS := \
    sample_RegionDetect/service.c \
    sample_RegionDetect/MotionDetect.c \
    sample_RegionDetect/MppHelper.c
LOCAL_TARGET_BIN := sample_RegionDetect/sample_RegionDetect
endif

# libTinyServer
ifeq ($(TARGET), sample_rtsp)
SRCCS := \
	sample_rtsp/sample_rtsp.c \
	sample_rtsp/rtsp_server.cpp
LOCAL_TARGET_BIN := sample_rtsp/sample_rtsp
endif
ifeq ($(TARGET), sample_smartIPC_demo)
SRCCS := \
	sample_smartIPC_demo/sample_smartIPC_demo.c \
	sample_smartIPC_demo/rtsp_server.cpp \
	sample_smartIPC_demo/sdcard_manager.c \
	sample_smartIPC_demo/record.c \
	sample_smartIPC_demo/aiservice.c \
	sample_smartIPC_demo/aiservice_detect.c \
	sample_smartIPC_demo/aiservice_hw_scale.c \
	sample_smartIPC_demo/aiservice_mpp_helper.c
LOCAL_TARGET_BIN := sample_smartIPC_demo/sample_smartIPC_demo
endif
ifeq ($(TARGET), sample_smartPreview_demo)
SRCCS := \
	sample_smartPreview_demo/sample_smartPreview_demo.c \
	sample_smartPreview_demo/aiservice.c \
	sample_smartPreview_demo/aiservice_detect.c \
	sample_smartPreview_demo/aiservice_hw_scale.c \
	sample_smartPreview_demo/aiservice_mpp_helper.c
LOCAL_TARGET_BIN := sample_smartPreview_demo/sample_smartPreview_demo
endif
ifeq ($(TARGET), sample_select)
SRCCS := sample_select/sample_select.c
LOCAL_TARGET_BIN := sample_select/sample_select
endif
# libactivate.a or libtxzEngineUSB.so
ifeq ($(TARGET), sample_sound_controler)
SRCCS := sample_sound_controler/sample_sound_controler.c
LOCAL_TARGET_BIN := sample_sound_controler/sample_sound_controler
endif
ifeq ($(TARGET), sample_timelapse)
SRCCS := sample_timelapse/sample_timelapse.c
LOCAL_TARGET_BIN := sample_timelapse/sample_timelapse
endif
# libssl, libcrypto
ifeq ($(TARGET), sample_twinchn_virvi2venc2ce)
SRCCS := sample_twinchn_virvi2venc2ce/sample_twinchn_virvi2venc2ce.c
LOCAL_TARGET_BIN := sample_twinchn_virvi2venc2ce/sample_twinchn_virvi2venc2ce
endif

ifeq ($(TARGET), sample_UILayer)
SRCCS := sample_UILayer/sample_UILayer.c
LOCAL_TARGET_BIN := sample_UILayer/sample_UILayer
endif
ifeq ($(TARGET), sample_uvc_vo)
SRCCS := sample_UVC/sample_uvc_vo/sample_uvc_vo.c
LOCAL_TARGET_BIN := sample_UVC/sample_uvc_vo/sample_uvc_vo
endif
ifeq ($(TARGET), sample_uvc2vdec_vo)
SRCCS := sample_UVC/sample_uvc2vdec_vo/sample_uvc2vdec_vo.c
LOCAL_TARGET_BIN := sample_UVC/sample_uvc2vdec_vo/sample_uvc2vdec_vo
endif
ifeq ($(TARGET), sample_uvc2vdenc2vo)
SRCCS := sample_UVC/sample_uvc2vdenc2vo/sample_uvc2vdenc2vo.c
LOCAL_TARGET_BIN := sample_UVC/sample_uvc2vdenc2vo/sample_uvc2vdenc2vo
endif
ifeq ($(TARGET), sample_uvc2vo)
SRCCS := sample_UVC/sample_uvc2vo/sample_uvc2vo.c
LOCAL_TARGET_BIN := sample_UVC/sample_uvc2vo/sample_uvc2vo
endif

ifeq ($(TARGET), sample_uac)
SRCCS := sample_uac/sample_uac.c
LOCAL_TARGET_BIN := sample_uac/sample_uac
endif
ifeq ($(TARGET), sample_uvcout)
SRCCS := sample_uvcout/sample_uvcout.c
LOCAL_TARGET_BIN := sample_uvcout/sample_uvcout
endif
ifeq ($(TARGET), sample_usbcamera)
SRCCS := sample_usbcamera/sample_usbcamera.c
LOCAL_TARGET_BIN := sample_usbcamera/sample_usbcamera
endif
ifeq ($(TARGET), sample_vdec)
SRCCS := sample_vdec/sample_vdec.c
LOCAL_TARGET_BIN := sample_vdec/sample_vdec
endif
ifeq ($(TARGET), sample_venc)
SRCCS := \
    sample_venc/sample_venc_mem.c \
    sample_venc/sample_venc.c
LOCAL_TARGET_BIN := sample_venc/sample_venc
endif
ifeq ($(TARGET), sample_venc2muxer)
SRCCS := sample_venc2muxer/sample_venc2muxer.c
LOCAL_TARGET_BIN := sample_venc2muxer/sample_venc2muxer
endif
ifeq ($(TARGET), sample_vi_g2d)
SRCCS := sample_vi_g2d/sample_vi_g2d.c
LOCAL_TARGET_BIN := sample_vi_g2d/sample_vi_g2d
endif

ifeq ($(TARGET), sample_vi_reset)
SRCCS := sample_vi_reset/sample_vi_reset.c
LOCAL_TARGET_BIN := sample_vi_reset/sample_vi_reset
endif
ifeq ($(TARGET), sample_vin_isp_test)
SRCCS := sample_vin_isp_test/sample_vin_isp_test.c
LOCAL_TARGET_BIN := sample_vin_isp_test/sample_vin_isp_test
endif
ifeq ($(TARGET), sample_virvi)
SRCCS := sample_virvi/sample_virvi.c
LOCAL_TARGET_BIN := sample_virvi/sample_virvi
endif
ifeq ($(TARGET), sample_virvi2eis2venc)
SRCCS := sample_virvi2eis2venc/sample_virvi2eis2venc.c
LOCAL_TARGET_BIN := sample_virvi2eis2venc/sample_virvi2eis2venc
endif
ifeq ($(TARGET), sample_virvi2fish2venc)
SRCCS := sample_virvi2fish2venc/sample_virvi2fish2venc.c
LOCAL_TARGET_BIN := sample_virvi2fish2venc/sample_virvi2fish2venc
endif

ifeq ($(TARGET), sample_virvi2fish2vo)
SRCCS := sample_virvi2fish2vo/sample_virvi2fish2vo.c
LOCAL_TARGET_BIN := sample_virvi2fish2vo/sample_virvi2fish2vo
endif
ifeq ($(TARGET), sample_virvi2venc)
SRCCS := sample_virvi2venc/sample_virvi2venc.c
LOCAL_TARGET_BIN := sample_virvi2venc/sample_virvi2venc
endif
# libssl, libcrypto
ifeq ($(TARGET), sample_virvi2venc2ce)
SRCCS := sample_virvi2venc2ce/sample_virvi2venc2ce.c
LOCAL_TARGET_BIN := sample_virvi2venc2ce/sample_virvi2venc2ce
endif
ifeq ($(TARGET), sample_virvi2venc2muxer)
SRCCS := sample_virvi2venc2muxer/sample_vi2venc2muxer.c
LOCAL_TARGET_BIN := sample_virvi2venc2muxer/sample_vi2venc2muxer
endif
ifeq ($(TARGET), sample_virvi2vo)
SRCCS := sample_virvi2vo/sample_virvi2vo.c
LOCAL_TARGET_BIN := sample_virvi2vo/sample_virvi2vo
endif
ifeq ($(TARGET), sample_odet_demo)
SRCCS := sample_odet_demo/sample_odet_demo.c
SRCCS += sample_odet_demo/yolov3-tiny_nbg_viplite/main.c
SRCCS += sample_odet_demo/yolov3-tiny_nbg_viplite/vnn_pre_process.c
SRCCS += sample_odet_demo/yolov3-tiny_nbg_viplite/vnn_post_process.c
SRCCS += sample_odet_demo/post/box.cpp
SRCCS += sample_odet_demo/post/post_process.cpp
SRCCS += sample_odet_demo/post/yolo_layer.cpp
LOCAL_TARGET_BIN := sample_odet_demo/sample_odet_demo
endif

ifeq ($(TARGET), sample_vo)
SRCCS := sample_vo/sample_vo.c
LOCAL_TARGET_BIN := sample_vo/sample_vo
endif
ifeq ($(TARGET), sample_recorder)
SRCCS := sample_recorder/sample_recorder.c
LOCAL_TARGET_BIN := sample_recorder/sample_recorder
endif

LOCAL_SHARED_LIBS :=
LOCAL_STATIC_LIBS :=

ifeq ($(MPPCFG_TOOLCHAIN_LIBC), glibc)
LOCAL_SHARED_LIBS += \
	libdl \
	librt \
	libpthread
endif


ifeq ($(MPPCFG_COMPILE_DYNAMIC_LIB), Y)
######################## dynamic lib mode ########################
# Public dynamic library
LOCAL_SHARED_LIBS += \
    libglog \
    liblog \
	libion \
    libcdx_common \
    libsample_confparser \
    libmedia_mpp \
    libmpp_component \
    libmedia_utils

# These are the libraries corresponding to MPP components
ifeq ($(MPPCFG_VI),Y)
LOCAL_SHARED_LIBS += \
    libmpp_vi \
    libmpp_isp \
    libISP
endif
ifeq ($(MPPCFG_VO),Y)
LOCAL_SHARED_LIBS += \
    libmpp_vo
endif
ifeq ($(MPPCFG_ISE),Y)
LOCAL_SHARED_LIBS += \
    libmpp_ise
endif
ifeq ($(MPPCFG_ADEC), Y)
LOCAL_SHARED_LIBS += \
    libadecoder \
    libaw_g726dec
endif
ifeq ($(MPPCFG_EIS),Y)
LOCAL_SHARED_LIBS += \
    libmpp_eis \
    lib_eis
endif
ifeq ($(MPPCFG_UVC),Y)
LOCAL_SHARED_LIBS += \
    libmpp_uvc
endif

# These libraries are only used by the corresponding samples
ifneq (,$(filter $(TARGET),sample_rtsp sample_smartIPC_demo))
LOCAL_SHARED_LIBS += \
    libTinyServer
endif
ifeq ($(TARGET), sample_isposd)
LOCAL_SHARED_LIBS += \
	librgb_ctrl \
	liblz4
endif
ifeq ($(TARGET), sample_sound_controler)
LOCAL_SHARED_LIBS += \
	libtxzEngineUSB
endif
ifneq (,$(filter $(TARGET),sample_twinchn_virvi2venc2ce sample_virvi2venc2ce))
LOCAL_SHARED_LIBS += \
	libssl \
	libcrypto
endif
ifeq ($(TARGET), sample_file_repair)
LOCAL_SHARED_LIBS += \
    libfilerepair
endif
ifneq (,$(filter $(TARGET),sample_odet_demo sample_RegionDetect sample_PersonDetect sample_smartIPC_demo sample_smartPreview_demo))
LOCAL_SHARED_LIBS += \
    libVIPlite \
    libVIPuser
endif
ifneq (,$(filter $(TARGET),sample_RegionDetect sample_PersonDetect sample_smartIPC_demo sample_smartPreview_demo))
LOCAL_SHARED_LIBS += \
    libawnn_det
endif

else
######################## static lib mode ########################
# These only provide dynamic libraries
LOCAL_SHARED_LIBS += \
    libasound \
    libglog

# Public static library
LOCAL_STATIC_LIBS += \
    libz \
    liblog \
    libion \
	libaw_mpp \
    libmedia_utils \
    libMemAdapter \
    libVE \
    libcdc_base \
    libcedarxstream \
    libsample_confparser\
    libcdx_common \
    libcdx_base \
    libResample

# These are the libraries corresponding to MPP components
ifeq ($(MPPCFG_VI),Y)
LOCAL_STATIC_LIBS += \
    libISP \
    libisp_dev \
    libisp_ini \
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
    libisp_rolloff
endif
ifeq ($(MPPCFG_VO),Y)
LOCAL_STATIC_LIBS += \
    libcedarxrender
ifeq ($(MPPCFG_HW_DISPLAY),Y)
LOCAL_STATIC_LIBS += \
	libhwdisplay
endif
endif
ifeq ($(MPPCFG_TEXTENC),Y)
LOCAL_STATIC_LIBS += \
    libcedarx_tencoder
endif
ifeq ($(MPPCFG_VENC),Y)
LOCAL_STATIC_LIBS += \
    libvencoder \
    libvenc_codec \
    libvenc_base
endif
ifeq ($(MPPCFG_VDEC),Y)
LOCAL_STATIC_LIBS += \
    libvdecoder \
    libvideoengine \
    libawh264 \
    libawh265 \
    libawmjpeg
endif
ifeq ($(MPPCFG_AENC),Y)
LOCAL_STATIC_LIBS += \
    libcedarx_aencoder
endif
ifeq ($(MPPCFG_AENC_AAC),Y)
LOCAL_STATIC_LIBS += \
    libaacenc
endif
ifeq ($(MPPCFG_AENC_MP3),Y)
LOCAL_STATIC_LIBS += \
    libmp3enc
endif
ifeq ($(MPPCFG_ADEC), Y)
LOCAL_STATIC_LIBS += \
    libadecoder \
    libaac \
    libmp3 \
    libwav \
    libaw_g726dec
endif
ifeq ($(MPPCFG_MUXER),Y)
LOCAL_STATIC_LIBS += \
    libmuxers \
    libmp4_muxer \
    libraw_muxer \
    libmpeg2ts_muxer \
    libaac_muxer \
    libmp3_muxer \
    libffavutil \
    libFsWriter
endif
ifeq ($(MPPCFG_DEMUXER),Y)
LOCAL_STATIC_LIBS += \
    libcedarxdemuxer \
    libcdx_aac_parser \
    libcdx_id3v2_parser \
    libcdx_mp3_parser \
    libcdx_mov_parser \
    libcdx_mpg_parser \
    libcdx_ts_parser \
    libcdx_wav_parser \
    libcdx_parser \
    libcdx_file_stream \
    libcdx_stream
endif
ifeq ($(MPPCFG_AEC),Y)
LOCAL_STATIC_LIBS += \
    libAec
endif
ifeq ($(MPPCFG_SOFTDRC),Y)
LOCAL_STATIC_LIBS += \
    libDrc
endif
ifneq ($(filter Y, $(MPPCFG_AI_AGC) $(MPPCFG_AGC)),)
LOCAL_STATIC_LIBS += \
    libAgc
endif
ifeq ($(MPPCFG_ANS),Y)
LOCAL_STATIC_LIBS += \
    libAns
endif
ifeq ($(MPPCFG_EIS),Y)
LOCAL_STATIC_LIBS += \
    libEIS \
    lib_eis
endif
ifeq ($(MPPCFG_ISE_MO),Y)
LOCAL_STATIC_LIBS += \
	lib_ise_mo
endif
ifeq ($(MPPCFG_ISE_GDC),Y)
LOCAL_STATIC_LIBS += \
	lib_ise_gdc
endif

# These libraries are only used by the corresponding samples
ifneq (,$(filter $(TARGET),sample_odet_demo sample_RegionDetect sample_PersonDetect sample_smartIPC_demo sample_smartPreview_demo))
LOCAL_SHARED_LIBS += \
    libVIPlite \
    libVIPuser
endif
ifneq (,$(filter $(TARGET),sample_RegionDetect sample_PersonDetect sample_smartIPC_demo sample_smartPreview_demo))
LOCAL_STATIC_LIBS += \
    libawnn_det
endif

ifneq (,$(filter $(TARGET),sample_rtsp sample_smartIPC_demo))
LOCAL_STATIC_LIBS += \
    libTinyServer
endif

ifeq ($(TARGET), sample_isposd)
LOCAL_STATIC_LIBS += \
	librgb_ctrl \
	liblz4
endif
ifeq ($(TARGET), sample_sound_controler)
LOCAL_STATIC_LIBS += \
	libactivate
endif
ifneq (,$(filter $(TARGET),sample_twinchn_virvi2venc2ce sample_virvi2venc2ce))
LOCAL_STATIC_LIBS += \
	libssl \
	libcrypto
endif
ifeq ($(TARGET), sample_file_repair)
LOCAL_STATIC_LIBS += \
    libfilerepair
endif

endif


#include directories
INCLUDE_DIRS := \
    $(CUR_PATH) \
    $(EYESEE_MPP_INCLUDE)/system/public/include \
	$(EYESEE_MPP_INCLUDE)/system/public/include/utils \
    $(EYESEE_MPP_INCLUDE)/system/public/include/vo \
    $(EYESEE_MPP_INCLUDE)/system/public/include/openssl \
	$(EYESEE_MPP_INCLUDE)/system/public/include/crypto \
    $(EYESEE_MPP_INCLUDE)/system/public/rgb_ctrl \
    $(EYESEE_MPP_INCLUDE)/system/public/libion/include \
    $(EYESEE_MPP_INCLUDE)/system/private/rtsp/IPCProgram/interface \
    $(EYESEE_MPP_INCLUDE)/external/sound_controler \
    $(EYESEE_MPP_INCLUDE)/external/sound_controler/include \
    $(PACKAGE_TOP)/include/utils \
    $(PACKAGE_TOP)/include/media \
    $(PACKAGE_TOP)/include/media/utils \
    $(PACKAGE_TOP)/include \
    $(PACKAGE_TOP)/config \
    $(PACKAGE_TOP)/media/include \
    $(PACKAGE_TOP)/media/include/utils \
    $(PACKAGE_TOP)/media/include/component \
    $(PACKAGE_TOP)/media/LIBRARY/libISE \
    $(PACKAGE_TOP)/media/LIBRARY/libISE/include \
    $(PACKAGE_TOP)/media/LIBRARY/libisp \
    $(PACKAGE_TOP)/media/LIBRARY/libisp/include \
	$(PACKAGE_TOP)/media/LIBRARY/libisp/include/device \
    $(PACKAGE_TOP)/media/LIBRARY/libisp/include/V4l2Camera \
    $(PACKAGE_TOP)/media/LIBRARY/libisp/isp_tuning \
    $(PACKAGE_TOP)/media/LIBRARY/libAIE_Vda/include \
    $(PACKAGE_TOP)/media/LIBRARY/include_ai_common \
    $(PACKAGE_TOP)/media/LIBRARY/include_eve_common \
    $(PACKAGE_TOP)/media/LIBRARY/libeveface/include \
    $(PACKAGE_TOP)/media/LIBRARY/include_stream \
    $(PACKAGE_TOP)/media/LIBRARY/include_FsWriter \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarc/include \
	$(PACKAGE_TOP)/media/LIBRARY/AudioLib/osal \
	$(PACKAGE_TOP)/media/LIBRARY/AudioLib/midware/encoding \
    $(PACKAGE_TOP)/media/LIBRARY/AudioLib/midware/encoding/include \
    $(PACKAGE_TOP)/media/LIBRARY/AudioLib/midware/decoding/include \
    $(PACKAGE_TOP)/media/LIBRARY/agc_lib/include \
    $(PACKAGE_TOP)/media/LIBRARY/include_muxer \
    $(PACKAGE_TOP)/media/LIBRARY/libfilerepair/include \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarx/libcore/common/iniparser \
    $(PACKAGE_TOP)/sample/configfileparser \
    $(LINUX_USER_HEADERS)/include

ifeq ($(TARGET), sample_odet_demo)
INCLUDE_DIRS += $(PACKAGE_TOP)/sample/sample_odet_demo/sdk/include
endif

#generate include directory flags for gcc.
inc_paths := $(foreach inc,$(filter-out -I%,$(INCLUDE_DIRS)),$(addprefix -I, $(inc))) \
                $(filter -I%, $(INCLUDE_DIRS))
#Extra flags to give to the C compiler
LOCAL_CFLAGS := $(CFLAGS) $(CEDARX_EXT_CFLAGS) $(inc_paths) -fPIC -Wall -Wno-unused-but-set-variable -Wno-unused-variable -Wno-unused-label -Wno-unused-function
#Extra flags to give to the C++ compiler
LOCAL_CXXFLAGS := $(CXXFLAGS) $(CEDARX_EXT_CFLAGS) $(inc_paths) -fPIC -Wall -Wno-unused-but-set-variable -Wno-unused-variable -Wno-unused-label -Wno-unused-function
#Extra flags to give to the C preprocessor and programs that use it (the C and Fortran compilers).
LOCAL_CPPFLAGS := $(CPPFLAGS)
#target device arch: x86, arm
LOCAL_TARGET_ARCH := $(ARCH)
#Extra flags to give to compilers when they are supposed to invoke the linker,‘ld’.
LOCAL_LDFLAGS := $(LDFLAGS)

LIB_SEARCH_PATHS := \
    $(EYESEE_MPP_LIBDIR) \
    $(PACKAGE_TOP)/sample/configfileparser \
    $(PACKAGE_TOP)/media/utils \
    $(PACKAGE_TOP)/media \
    $(PACKAGE_TOP)/media/component \
    $(PACKAGE_TOP)/media/LIBRARY/libstream \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarx/libcore/base \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarx/libcore/common \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarx/libcore/parser/aac \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarx/libcore/parser/id3v2 \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarx/libcore/parser/mov \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarx/libcore/parser/mp3 \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarx/libcore/parser/mpg \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarx/libcore/parser/ts \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarx/libcore/parser/wav \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarx/libcore/parser/base \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarx/libcore/stream/base \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarx/libcore/stream/file \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarc/base \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarc/ve \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarc/memory \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarc/vencoder \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarc/vencoder/base \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarc/vencoder/libcodec \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarc/vdecoder \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarc/vdecoder/videoengine \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarc/vdecoder/videoengine/h264 \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarc/vdecoder/videoengine/h265 \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarc/vdecoder/videoengine/mjpeg \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarc/library/out \
    $(PACKAGE_TOP)/media/LIBRARY/libdemux \
    $(PACKAGE_TOP)/media/LIBRARY/libfilerepair \
    $(PACKAGE_TOP)/media/LIBRARY/libmuxer/muxers \
    $(PACKAGE_TOP)/media/LIBRARY/libmuxer/mp4_muxer \
    $(PACKAGE_TOP)/media/LIBRARY/libmuxer/raw_muxer \
    $(PACKAGE_TOP)/media/LIBRARY/libmuxer/mpeg2ts_muxer \
    $(PACKAGE_TOP)/media/LIBRARY/libmuxer/aac_muxer \
    $(PACKAGE_TOP)/media/LIBRARY/libmuxer/mp3_muxer \
    $(PACKAGE_TOP)/media/LIBRARY/libmuxer/common/libavutil \
    $(PACKAGE_TOP)/media/LIBRARY/libFsWriter \
    $(PACKAGE_TOP)/media/LIBRARY/AudioLib/midware/decoding \
    $(PACKAGE_TOP)/media/LIBRARY/AudioLib/midware/encoding \
    $(PACKAGE_TOP)/media/LIBRARY/AudioLib/lib/out \
    $(PACKAGE_TOP)/media/LIBRARY/audioEffectLib/lib \
    $(PACKAGE_TOP)/media/LIBRARY/textEncLib \
    $(PACKAGE_TOP)/media/LIBRARY/aec_lib/out \
    $(PACKAGE_TOP)/media/LIBRARY/drc_lib/out \
    $(PACKAGE_TOP)/media/LIBRARY/agc_lib/out \
    $(PACKAGE_TOP)/media/LIBRARY/ans_lib/out \
    $(PACKAGE_TOP)/media/LIBRARY/libResample \
    $(PACKAGE_TOP)/media/LIBRARY/libisp \
    $(PACKAGE_TOP)/media/LIBRARY/libisp/out/out \
    $(PACKAGE_TOP)/media/LIBRARY/libisp/isp_cfg \
    $(PACKAGE_TOP)/media/LIBRARY/libisp/isp_dev \
    $(PACKAGE_TOP)/media/LIBRARY/libisp/iniparser \
    $(PACKAGE_TOP)/media/LIBRARY/libISE/out \
    $(PACKAGE_TOP)/media/LIBRARY/libVideoStabilization \
    $(PACKAGE_TOP)/media/LIBRARY/libVideoStabilization/out \
    $(PACKAGE_TOP)/media/librender

ifneq (,$(filter $(TARGET),sample_odet_demo sample_PersonDetect sample_RegionDetect sample_smartIPC_demo sample_smartPreview_demo))
LIB_SEARCH_PATHS += $(STAGING_DIR)/usr/lib
endif

empty:=
space:= $(empty) $(empty)

LOCAL_BIN_LDFLAGS := $(LOCAL_LDFLAGS) \
    $(patsubst %,-L%,$(LIB_SEARCH_PATHS)) \
    -Wl,-rpath-link=$(subst $(space),:,$(strip $(LIB_SEARCH_PATHS))) \
    -Wl,-Bstatic \
    -Wl,--whole-archive \
    -Wl,--start-group $(foreach n, $(LOCAL_STATIC_LIBS), -l$(patsubst lib%,%,$(patsubst %.a,%,$(notdir $(n))))) -Wl,--end-group \
    -Wl,--no-whole-archive \
    -Wl,-Bdynamic \
    $(foreach y, $(LOCAL_SHARED_LIBS), -l$(patsubst lib%,%,$(patsubst %.so,%,$(notdir $(y)))))

#generate object files
OBJS := $(SRCCS:%=%.o) #OBJS=$(patsubst %,%.o,$(SRCCS))
DEPEND_LIBS := $(wildcard $(foreach p, $(patsubst %/,%,$(LIB_SEARCH_PATHS)), \
                            $(addprefix $(p)/, \
                              $(foreach y,$(LOCAL_SHARED_LIBS),$(patsubst %,%.so,$(patsubst %.so,%,$(notdir $(y))))) \
                              $(foreach n,$(LOCAL_STATIC_LIBS),$(patsubst %,%.a,$(patsubst %.a,%,$(notdir $(n))))) \
                            ) \
                          ) \
               )

#generate exe file.
.PHONY: all
all: $(LOCAL_TARGET_BIN)
	@echo ===================================
	@echo build eyesee-mpp-middleware-sample-$(LOCAL_TARGET_BIN) done
	@echo ===================================

$(LOCAL_TARGET_BIN): $(OBJS) $(DEPEND_LIBS)
	$(CXX) $(OBJS) $(LOCAL_BIN_LDFLAGS) -o $@
	-mkdir -p $(MPP_SAMPLES_BIN_DIR)
	-cp -f $@ $(MPP_SAMPLES_BIN_DIR)
	-cp -f $(wildcard *.conf $(TARGET)/*.conf) $(MPP_SAMPLES_BIN_DIR)
	@echo ----------------------------
	@echo "finish target: $@"
#	@echo "object files:  $+"
#	@echo "source files:  $(SRCCS)"
	@echo ----------------------------

#patten rules to generate local object files
$(filter %.cpp.o %.cc.o, $(OBJS)): %.o: %
	$(CXX) $(LOCAL_CXXFLAGS) $(LOCAL_CPPFLAGS) -MD -MP -MF $(@:%=%.d) -c -o $@ $<
$(filter %.c.o, $(OBJS)): %.o: %
	$(CC) $(LOCAL_CFLAGS) $(LOCAL_CPPFLAGS) -MD -MP -MF $(@:%=%.d) -c -o $@ $<

# clean all
.PHONY: clean
clean:
	-rm -rf $(OBJS) $(OBJS:%=%.d) $(LOCAL_TARGET_BIN) $(MPP_SAMPLES_BIN_DIR)

#add *.h prerequisites
-include $(OBJS:%=%.d)
