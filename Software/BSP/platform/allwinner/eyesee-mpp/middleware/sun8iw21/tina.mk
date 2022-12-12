############################################################################################
# 			eyesee-mpp-middleware for tina(OpenWrt) Linux
#
#	eyesee-mpp is designed for CDR/SDV product, focus on video/audio capturing and encoding, 
# it also can support video/audio decode.
#   eyesee-mpp-middleware provides basic libraries of mpp.
# libDisplay, etc.
#
# Version: v1.0
# Date   : 2019-1-23
# Author : PDC-PD5
############################################################################################
CUR_PATH := $(shell pwd)
include $(CUR_PATH)/config/mpp_config.mk

all:
	@echo ==================================================
	@echo build eyesee-mpp-middleware
	@echo ==================================================
	make -C media/LIBRARY/libcedarx/config                  -f tina.mk  all
	make -C media/LIBRARY/libcedarx/libcore/base            -f tina.mk  all
	make -C media/LIBRARY/libcedarx/libcore/common          -f tina.mk  all
	make -C media/utils -f tina.mk                                 all
	make -C media/LIBRARY/libstream -f tina.mk                    all
ifeq ($(MPPCFG_MUXER),Y)
	make -C media/LIBRARY/libFsWriter -f tina.mk                  all
	make -C media/LIBRARY/libmuxer/common/libavutil -f tina.mk    all
	make -C media/LIBRARY/libmuxer/mp3_muxer -f tina.mk           all
	make -C media/LIBRARY/libmuxer/aac_muxer -f tina.mk           all
	make -C media/LIBRARY/libmuxer/mp4_muxer -f tina.mk           all
	make -C media/LIBRARY/libmuxer/mpeg2ts_muxer -f tina.mk       all
	make -C media/LIBRARY/libmuxer/raw_muxer -f tina.mk           all
	make -C media/LIBRARY/libmuxer/muxers -f tina.mk              all
endif

	make -C media/LIBRARY/libfilerepair/ -f tina.mk           all
ifeq ($(MPPCFG_ISE),Y)
	make -C media/LIBRARY/libISE -f tina.mk                       all
endif
	PACKAGE_TOP=$(CUR_PATH) make -C media/LIBRARY/libcedarc -f tina.mk  all

ifeq ($(MPPCFG_VI),Y)
	PACKAGE_TOP=$(CUR_PATH) make -C media/LIBRARY/libisp    -f tina.mk  all
endif

ifeq ($(MPPCFG_DEMUXER),Y)
	make -C media/LIBRARY/libcedarx/libcore/stream/file     -f tina.mk  all
	make -C media/LIBRARY/libcedarx/libcore/stream/base     -f tina.mk  all
	make -C media/LIBRARY/libcedarx/libcore/parser/aac      -f tina.mk  all
	make -C media/LIBRARY/libcedarx/libcore/parser/id3v2    -f tina.mk  all
	make -C media/LIBRARY/libcedarx/libcore/parser/mp3      -f tina.mk  all
	make -C media/LIBRARY/libcedarx/libcore/parser/mov      -f tina.mk  all
	make -C media/LIBRARY/libcedarx/libcore/parser/mpg      -f tina.mk  all
	make -C media/LIBRARY/libcedarx/libcore/parser/ts       -f tina.mk  all
	make -C media/LIBRARY/libcedarx/libcore/parser/wav      -f tina.mk  all
	make -C media/LIBRARY/libcedarx/libcore/parser/base     -f tina.mk  all
	make -C media/LIBRARY/libdemux -f tina.mk                     all
endif
	make -C media/LIBRARY/AudioLib/lib -f tina.mk       all
ifeq ($(MPPCFG_AENC),Y)
	make -C media/LIBRARY/AudioLib/midware/encoding -f tina.mk all
endif
ifeq ($(MPPCFG_ADEC),Y)
	make -C media/LIBRARY/AudioLib/midware/decoding -f tina.mk all
endif
ifeq ($(MPPCFG_TEXTENC),Y)
	make -C media/LIBRARY/textEncLib -f tina.mk all
endif
ifeq ($(MPPCFG_AEC),Y)
	make -C media/LIBRARY/aec_lib -f tina.mk all
endif
ifeq ($(MPPCFG_AGC),Y)
	make -C media/LIBRARY/agc_lib -f tina.mk all
else ifeq ($(MPPCFG_AI_AGC),Y)
	make -C media/LIBRARY/agc_lib -f tina.mk all
endif
ifeq ($(MPPCFG_ANS),Y)
	make -C media/LIBRARY/ans_lib -f tina.mk all
endif
ifeq ($(MPPCFG_SOFTDRC),Y)
	make -C media/LIBRARY/drc_lib -f tina.mk all
endif
	make -C media/LIBRARY/libResample -f tina.mk all

	make -C media/LIBRARY/libMODSoft -f tina.mk                     all
ifneq ($(filter Y, $(MPPCFG_ADAS_DETECT) $(MPPCFG_ADAS_DETECT_V2)),)
	make -C media/LIBRARY/libADAS -f tina.mk                     	all
endif
ifeq ($(MPPCFG_EIS),Y)
	PACKAGE_TOP=$(CUR_PATH) make -C media/LIBRARY/libVideoStabilization -f tina.mk all
endif
	make -C media/LIBRARY/lib_aw_ai_algo -f tina.mk               all
	make -C media/LIBRARY/lib_aw_ai_core -f tina.mk               all
	make -C media/LIBRARY/lib_aw_ai_mt -f tina.mk                 all
ifeq ($(MPPCFG_VO),Y)
	make -C media/librender -f tina.mk                            all
endif
ifeq ($(MPPCFG_VI),Y)
	make -C media -f tina_mpp_vi.mk                     all
	make -C media -f tina_mpp_isp.mk                    all
endif
ifeq ($(MPPCFG_ISE),Y)
	make -C media -f tina_mpp_ise.mk                    all
endif
ifeq ($(MPPCFG_EIS),Y)
	make -C media -f tina_mpp_eis.mk                    all
endif
ifeq ($(MPPCFG_VO),Y)
	make -C media -f tina_mpp_vo.mk                     all
endif
ifeq ($(MPPCFG_UVC),Y)
	make -C media -f tina_mpp_uvc.mk                    all
endif

	make -C media/component -f tina.mk                            all
	make -C media -f tina.mk                                      all
#	make -C media/isp_tool                              all
	make -C media -f tina_mpp_static.mk                 all

# used to compile g711a/g711u/g726 decoder
#	make -C media/LIBRARY/g711a -f tina.mk		all
#	make -C media/LIBRARY/g711u -f tina.mk      all
#	make -C media/LIBRARY/g726 -f tina.mk       all

	# must compile configfileparser before compile sample
	make -C sample/configfileparser -f tina.mk  all

ifeq ($(MPPCFG_SAMPLE),Y)

ifeq ($(MPPCFG_SAMPLE_TAKE_PICTURE),Y)
	make -C sample -f tina.mk TARGET=sample_takePicture  all
endif
ifeq ($(MPPCFG_SAMPLE_VENC_GDCZOOM),Y)
	make -C sample -f tina.mk TARGET=sample_vencGdcZoom  all
endif
ifeq ($(MPPCFG_SAMPLE_VENC_RECREATE),Y)
	make -C sample -f tina.mk TARGET=sample_vencRecreate  all
endif

ifeq ($(MPPCFG_SAMPLE_ONLINEVENC),Y)
	make -C sample -f tina.mk TARGET=sample_OnlineVenc  all
endif
ifeq ($(MPPCFG_SAMPLE_VENC_QPMAP),Y)
	make -C sample -f tina.mk TARGET=sample_vencQpMap  all
endif
ifeq ($(MPPCFG_SAMPLE_VIRVI2VO_ZOOM),Y)
	make -C sample -f tina.mk TARGET=sample_virvi2vo_zoom  all
endif
ifeq ($(MPPCFG_SAMPLE_CODEC_PARALLEL),Y)
	make -C sample -f tina.mk TARGET=sample_CodecParallel  all
endif

ifeq ($(MPPCFG_SAMPLE_ADEC),Y)
	make -C sample -f tina.mk TARGET=sample_adec  all
endif
ifeq ($(MPPCFG_SAMPLE_ADEC2AO),Y)
	make -C sample -f tina.mk TARGET=sample_adec2ao  all
endif
ifeq ($(MPPCFG_SAMPLE_AEC),Y)
	make -C sample -f tina.mk TARGET=sample_aec  all
endif
ifeq ($(MPPCFG_SAMPLE_AENC),Y)
	make -C sample -f tina.mk TARGET=sample_aenc  all
endif
ifeq ($(MPPCFG_SAMPLE_AI),Y)
	make -C sample -f tina.mk TARGET=sample_ai  all
endif
ifeq ($(MPPCFG_SAMPLE_AI2AENC),Y)
	make -C sample -f tina.mk TARGET=sample_ai2aenc  all
endif

ifeq ($(MPPCFG_SAMPLE_AI2AENC2MUXER),Y)
	make -C sample -f tina.mk TARGET=sample_ai2aenc2muxer  all
endif
ifeq ($(MPPCFG_SAMPLE_AO),Y)
	make -C sample -f tina.mk TARGET=sample_ao  all
endif
ifeq ($(MPPCFG_SAMPLE_AO_RESAMPLE_MIXER),Y)
	make -C sample -f tina.mk TARGET=sample_ao_resample_mixer  all
endif

ifeq ($(MPPCFG_SAMPLE_AO2AI_AEC),Y)
	make -C sample -f tina.mk TARGET=sample_ao2ai_aec  all
endif
ifeq ($(MPPCFG_SAMPLE_AO2AI_AEC_RATE_MIXER),Y)
	make -C sample -f tina.mk TARGET=sample_ao2ai_aec_rate_mixer  all
endif
ifeq ($(MPPCFG_SAMPLE_AOSYNC),Y)
	make -C sample -f tina.mk TARGET=sample_aoSync  all
endif
ifeq ($(MPPCFG_SAMPLE_DEMUX),Y)
	make -C sample -f tina.mk TARGET=sample_demux  all
endif
ifeq ($(MPPCFG_SAMPLE_DEMUX2ADEC),Y)
	make -C sample -f tina.mk TARGET=sample_demux2adec  all
endif
ifeq ($(MPPCFG_SAMPLE_DEMUX2ADEC2AO),Y)
	make -C sample -f tina.mk TARGET=sample_demux2adec2ao  all
endif

ifeq ($(MPPCFG_SAMPLE_DEMUX2VDEC),Y)
	make -C sample -f tina.mk TARGET=sample_demux2vdec  all
endif
ifeq ($(MPPCFG_SAMPLE_DEMUX2VDEC_SAVEFRAME),Y)
	make -C sample -f tina.mk TARGET=sample_demux2vdec_saveFrame  all
endif
ifeq ($(MPPCFG_SAMPLE_DEMUX2VDEC2VO),Y)
	make -C sample -f tina.mk TARGET=sample_demux2vdec2vo  all
endif
ifeq ($(MPPCFG_SAMPLE_DIRECTIOREAD),Y)
	make -C sample -f tina.mk TARGET=sample_directIORead  all
endif
ifeq ($(MPPCFG_SAMPLE_DRIVERVIPP),Y)
	make -C sample -f tina.mk TARGET=sample_driverVipp  all
endif

ifeq ($(MPPCFG_SAMPLE_FILE_REPAIR),Y)
	make -C sample -f tina.mk TARGET=sample_file_repair  all
endif
ifeq ($(MPPCFG_SAMPLE_FISH),Y)
	make -C sample -f tina.mk TARGET=sample_fish  all
endif
ifeq ($(MPPCFG_SAMPLE_G2D),Y)
	make -C sample -f tina.mk TARGET=sample_g2d  all
endif
ifeq ($(MPPCFG_SAMPLE_GLOG),Y)
	make -C sample -f tina.mk TARGET=sample_glog  all
endif
ifeq ($(MPPCFG_SAMPLE_HELLO),Y)
	make -C sample -f tina.mk TARGET=sample_hello  all
endif

ifeq ($(MPPCFG_SAMPLE_ISPOSD),Y)
	make -C sample -f tina.mk TARGET=sample_isposd  all
endif
ifeq ($(MPPCFG_SAMPLE_MOTIONDETECT),Y)
	make -C sample -f tina.mk TARGET=sample_MotionDetect  all
endif
ifeq ($(MPPCFG_SAMPLE_MOTOR),Y)
	make -C sample -f tina.mk TARGET=sample_motor  all
endif
ifeq ($(MPPCFG_SAMPLE_MULTI_VI2VENC2MUXER),Y)
	make -C sample -f tina.mk TARGET=sample_multi_vi2venc2muxer  all
endif
ifeq ($(MPPCFG_SAMPLE_PERSONDETECT),Y)
	make -C sample -f tina.mk TARGET=sample_PersonDetect all
endif
ifeq ($(MPPCFG_SAMPLE_PTHREAD_CANCEL),Y)
	make -C sample -f tina.mk TARGET=sample_pthread_cancel  all
endif
ifeq ($(MPPCFG_SAMPLE_REGION),Y)
	make -C sample -f tina.mk TARGET=sample_region  all
endif
ifeq ($(MPPCFG_SAMPLE_REGION_DETECT),Y)
	make -C sample -f tina.mk TARGET=sample_RegionDetect  all
endif

ifeq ($(MPPCFG_SAMPLE_RTSP),Y)
	make -C sample -f tina.mk TARGET=sample_rtsp  all
endif
ifeq ($(MPPCFG_SAMPLE_SMARTIPC_DEMO),Y)
	make -C sample -f tina.mk TARGET=sample_smartIPC_demo  all
endif
ifeq ($(MPPCFG_SAMPLE_SMARTPREVIEW_DEMO),Y)
	make -C sample -f tina.mk TARGET=sample_smartPreview_demo  all
endif
ifeq ($(MPPCFG_SAMPLE_SELECT),Y)
	make -C sample -f tina.mk TARGET=sample_select  all
endif
ifeq ($(MPPCFG_SAMPLE_SOUND_CONTROLER),Y)
	make -C sample -f tina.mk TARGET=sample_sound_controler  all
endif
ifeq ($(MPPCFG_SAMPLE_TIMELAPSE),Y)
	make -C sample -f tina.mk TARGET=sample_timelapse  all
endif
ifeq ($(MPPCFG_SAMPLE_TWINCHN_VIRVI2VENC2CE),Y)
	make -C sample -f tina.mk TARGET=sample_twinchn_virvi2venc2ce  all
endif

ifeq ($(MPPCFG_SAMPLE_UILAYER),Y)
	make -C sample -f tina.mk TARGET=sample_UILayer  all
endif
ifeq ($(MPPCFG_SAMPLE_UVC_VO),Y)
	make -C sample -f tina.mk TARGET=sample_uvc_vo  all
endif
ifeq ($(MPPCFG_SAMPLE_UVC2VDEC_VO),Y)
	make -C sample -f tina.mk TARGET=sample_uvc2vdec_vo  all
endif
ifeq ($(MPPCFG_SAMPLE_UVC2VDENC2VO),Y)
	make -C sample -f tina.mk TARGET=sample_uvc2vdenc2vo  all
endif
ifeq ($(MPPCFG_SAMPLE_UVC2VO),Y)
	make -C sample -f tina.mk TARGET=sample_uvc2vo  all
endif

ifeq ($(MPPCFG_SAMPLE_UAC),Y)
	make -C sample -f tina.mk TARGET=sample_uac  all
endif
ifeq ($(MPPCFG_SAMPLE_UVCOUT),Y)
	make -C sample -f tina.mk TARGET=sample_uvcout  all
endif
ifeq ($(MPPCFG_SAMPLE_USBCAMERA),Y)
	make -C sample -f tina.mk TARGET=sample_usbcamera  all
endif
ifeq ($(MPPCFG_SAMPLE_VDEC),Y)
	make -C sample -f tina.mk TARGET=sample_vdec  all
endif
ifeq ($(MPPCFG_SAMPLE_VENC),Y)
	make -C sample -f tina.mk TARGET=sample_venc  all
endif
ifeq ($(MPPCFG_SAMPLE_VENC2MUXER),Y)
	make -C sample -f tina.mk TARGET=sample_venc2muxer  all
endif
ifeq ($(MPPCFG_SAMPLE_VI_G2D),Y)
	make -C sample -f tina.mk TARGET=sample_vi_g2d  all
endif

ifeq ($(MPPCFG_SAMPLE_VI_RESET),Y)
	make -C sample -f tina.mk TARGET=sample_vi_reset  all
endif
ifeq ($(MPPCFG_SAMPLE_VIN_ISP_TEST),Y)
	make -C sample -f tina.mk TARGET=sample_vin_isp_test  all
endif
ifeq ($(MPPCFG_SAMPLE_VIRVI),Y)
	make -C sample -f tina.mk TARGET=sample_virvi  all
endif
ifeq ($(MPPCFG_SAMPLE_VIRVI2EIS2VENC),Y)
	make -C sample -f tina.mk TARGET=sample_virvi2eis2venc  all
endif
ifeq ($(MPPCFG_SAMPLE_VIRVI2FISH2VENC),Y)
	make -C sample -f tina.mk TARGET=sample_virvi2fish2venc  all
endif

ifeq ($(MPPCFG_SAMPLE_VIRVI2FISH2VO),Y)
	make -C sample -f tina.mk TARGET=sample_virvi2fish2vo  all
endif
ifeq ($(MPPCFG_SAMPLE_VIRVI2VENC),Y)
	make -C sample -f tina.mk TARGET=sample_virvi2venc  all
endif
ifeq ($(MPPCFG_SAMPLE_VIRVI2VENC2CE),Y)
	make -C sample -f tina.mk TARGET=sample_virvi2venc2ce  all
endif
ifeq ($(MPPCFG_SAMPLE_VIRVI2VENC2MUXER),Y)
	make -C sample -f tina.mk TARGET=sample_virvi2venc2muxer  all
endif
ifeq ($(MPPCFG_SAMPLE_VIRVI2VO),Y)
	make -C sample -f tina.mk TARGET=sample_virvi2vo  all
endif

ifeq ($(MPPCFG_SAMPLE_VO),Y)
	make -C sample -f tina.mk TARGET=sample_vo  all
endif

ifeq ($(MPPCFG_SAMPLE_RECORDER),Y)
	make -C sample -f tina.mk TARGET=sample_recorder  all
endif
ifeq ($(MPPCFG_SAMPLE_ODET_DEMO), Y)
	make -C sample -f tina.mk TARGET=sample_odet_demo  all
endif
ifeq ($(MPPCFG_SAMPLE_AI2AO),Y)
	make -C sample -f tina.mk TARGET=sample_ai2ao  all
endif

endif ## MPPCFG_SAMPLE

	@echo build eyesee-mpp-middleware done!

clean:
	@echo ==================================================
	@echo clean eyesee-mpp-middleware
	@echo ==================================================
	make -C media/utils -f tina.mk                                clean
	make -C media/LIBRARY/libstream -f tina.mk                    clean
	make -C media/LIBRARY/libFsWriter -f tina.mk                  clean
	make -C media/LIBRARY/libmuxer/common/libavutil -f tina.mk    clean
	make -C media/LIBRARY/libmuxer/mp3_muxer -f tina.mk           clean
	make -C media/LIBRARY/libmuxer/aac_muxer -f tina.mk           clean
	make -C media/LIBRARY/libmuxer/mp4_muxer -f tina.mk           clean
	make -C media/LIBRARY/libmuxer/mpeg2ts_muxer -f tina.mk       clean
	make -C media/LIBRARY/libmuxer/raw_muxer -f tina.mk           clean
	make -C media/LIBRARY/libmuxer/muxers -f tina.mk              clean
	make -C media/LIBRARY/libfilerepair/ -f tina.mk               clean
	PACKAGE_TOP=$(CUR_PATH) make -C media/LIBRARY/libisp    -f tina.mk  clean
	make -C media/LIBRARY/libISE -f tina.mk                       clean
	PACKAGE_TOP=$(CUR_PATH) make -C media/LIBRARY/libcedarc -f tina.mk  clean
	make -C media/LIBRARY/libcedarx/config -f tina.mk   clean
	make -C media/LIBRARY/libcedarx/libcore/base -f tina.mk clean
	make -C media/LIBRARY/libcedarx/libcore/common -f tina.mk clean
	make -C media/LIBRARY/libcedarx/libcore/stream/file -f tina.mk clean
	make -C media/LIBRARY/libcedarx/libcore/stream/base -f tina.mk clean
	make -C media/LIBRARY/libcedarx/libcore/parser/aac -f tina.mk clean
	make -C media/LIBRARY/libcedarx/libcore/parser/id3v2 -f tina.mk clean
	make -C media/LIBRARY/libcedarx/libcore/parser/mp3 -f tina.mk clean
	make -C media/LIBRARY/libcedarx/libcore/parser/mov -f tina.mk clean
	make -C media/LIBRARY/libcedarx/libcore/parser/mpg -f tina.mk clean
	make -C media/LIBRARY/libcedarx/libcore/parser/ts -f tina.mk clean
	make -C media/LIBRARY/libcedarx/libcore/parser/wav -f tina.mk clean
	make -C media/LIBRARY/libcedarx/libcore/parser/base -f tina.mk clean
	make -C media/LIBRARY/libdemux -f tina.mk                     clean
	make -C media/LIBRARY/AudioLib/lib -f tina.mk       clean
	make -C media/LIBRARY/aec_lib -f tina.mk clean
	make -C media/LIBRARY/drc_lib -f tina.mk clean
	make -C media/LIBRARY/agc_lib -f tina.mk clean
	make -C media/LIBRARY/ans_lib -f tina.mk clean
	make -C media/LIBRARY/libResample -f tina.mk clean
	make -C media/LIBRARY/AudioLib/midware/encoding -f tina.mk clean
	make -C media/LIBRARY/textEncLib -f tina.mk clean
	make -C media/LIBRARY/AudioLib/midware/decoding -f tina.mk clean
	make -C media/LIBRARY/libMODSoft -f tina.mk                   clean
	make -C media/LIBRARY/libADAS -f tina.mk                   clean
	PACKAGE_TOP=$(CUR_PATH) make -C media/LIBRARY/libVideoStabilization -f tina.mk  clean
	make -C media/LIBRARY/lib_aw_ai_algo -f tina.mk               clean
	make -C media/LIBRARY/lib_aw_ai_core -f tina.mk               clean
	make -C media/LIBRARY/lib_aw_ai_mt -f tina.mk                 clean
	make -C media/librender -f tina.mk                            clean
	make -C media -f tina_mpp_vi.mk                     clean
	make -C media -f tina_mpp_isp.mk                    clean
	make -C media -f tina_mpp_ise.mk                    clean
	make -C media -f tina_mpp_eis.mk                    clean
	make -C media -f tina_mpp_vo.mk                     clean
	make -C media -f tina_mpp_uvc.mk                    clean
	make -C media/component -f tina.mk                            clean
	make -C media -f tina.mk                                      clean
#	make -C media/isp_tool                              clean
	make -C media -f tina_mpp_static.mk                 clean

#	make -C media/LIBRARY/g711a -f tina.mk	clean
#	make -C media/LIBRARY/g711u -f tina.mk  clean
#	make -C media/LIBRARY/g726 -f tina.mk   clean

	make -C sample/configfileparser -f tina.mk  clean

	make -C sample -f tina.mk TARGET=sample_vencRecreate  clean
	make -C sample -f tina.mk TARGET=sample_takePicture  clean
	make -C sample -f tina.mk TARGET=sample_vencGdcZoom  clean

	make -C sample -f tina.mk TARGET=sample_OnlineVenc  clean
	make -C sample -f tina.mk TARGET=sample_vencQpMap  clean
	make -C sample -f tina.mk TARGET=sample_virvi2vo_zoom  clean
	make -C sample -f tina.mk TARGET=sample_CodecParallel  clean

	make -C sample -f tina.mk TARGET=sample_adec  clean
	make -C sample -f tina.mk TARGET=sample_adec2ao  clean
	make -C sample -f tina.mk TARGET=sample_aec  clean
	make -C sample -f tina.mk TARGET=sample_aenc  clean
	make -C sample -f tina.mk TARGET=sample_ai  clean
	make -C sample -f tina.mk TARGET=sample_ai2aenc  clean

	make -C sample -f tina.mk TARGET=sample_ai2aenc2muxer  clean
	make -C sample -f tina.mk TARGET=sample_ao  clean
	make -C sample -f tina.mk TARGET=sample_ao_resample_mixer  clean

	make -C sample -f tina.mk TARGET=sample_ao2ai_aec  clean
	make -C sample -f tina.mk TARGET=sample_ao2ai_aec_rate_mixer  clean
	make -C sample -f tina.mk TARGET=sample_aoSync clean
	make -C sample -f tina.mk TARGET=sample_demux  clean
	make -C sample -f tina.mk TARGET=sample_demux2adec  clean
	make -C sample -f tina.mk TARGET=sample_demux2adec2ao  clean

	make -C sample -f tina.mk TARGET=sample_demux2vdec  clean
	make -C sample -f tina.mk TARGET=sample_demux2vdec2vo  clean
	make -C sample -f tina.mk TARGET=sample_demux2vdec_saveFrame  clean
	make -C sample -f tina.mk TARGET=sample_directIORead  clean
	make -C sample -f tina.mk TARGET=sample_driverVipp  clean

	make -C sample -f tina.mk TARGET=sample_file_repair  clean
	make -C sample -f tina.mk TARGET=sample_fish  clean
	make -C sample -f tina.mk TARGET=sample_g2d  clean
	make -C sample -f tina.mk TARGET=sample_glog  clean
	make -C sample -f tina.mk TARGET=sample_hello  clean

	make -C sample -f tina.mk TARGET=sample_isposd  clean
	make -C sample -f tina.mk TARGET=sample_MotionDetect  clean
	make -C sample -f tina.mk TARGET=sample_motor  clean
	make -C sample -f tina.mk TARGET=sample_multi_vi2venc2muxer  clean
	make -C sample -f tina.mk TARGET=sample_PersonDetect clean
	make -C sample -f tina.mk TARGET=sample_pthread_cancel  clean
	make -C sample -f tina.mk TARGET=sample_region  clean
	make -C sample -f tina.mk TARGET=sample_RegionDetect  clean

	make -C sample -f tina.mk TARGET=sample_rtsp  clean
	make -C sample -f tina.mk TARGET=sample_smartIPC_demo  clean
	make -C sample -f tina.mk TARGET=sample_smartPreview_demo  clean
	make -C sample -f tina.mk TARGET=sample_select  clean
	make -C sample -f tina.mk TARGET=sample_sound_controler  clean
	make -C sample -f tina.mk TARGET=sample_timelapse  clean
	make -C sample -f tina.mk TARGET=sample_twinchn_virvi2venc2ce  clean
	
	make -C sample -f tina.mk TARGET=sample_UILayer  clean
	make -C sample -f tina.mk TARGET=sample_uvc_vo  clean
	make -C sample -f tina.mk TARGET=sample_uvc2vdec_vo  clean
	make -C sample -f tina.mk TARGET=sample_uvc2vdenc2vo  clean
	make -C sample -f tina.mk TARGET=sample_uvc2vo  clean
	
	make -C sample -f tina.mk TARGET=sample_uac  clean
	make -C sample -f tina.mk TARGET=sample_uvcout  clean
	make -C sample -f tina.mk TARGET=sample_usbcamera  clean
	make -C sample -f tina.mk TARGET=sample_vdec  clean
	make -C sample -f tina.mk TARGET=sample_venc  clean
	make -C sample -f tina.mk TARGET=sample_venc2muxer  clean
	make -C sample -f tina.mk TARGET=sample_vi_g2d  clean
	
	make -C sample -f tina.mk TARGET=sample_vi_reset  clean
	make -C sample -f tina.mk TARGET=sample_vin_isp_test  clean
	make -C sample -f tina.mk TARGET=sample_virvi  clean
	make -C sample -f tina.mk TARGET=sample_virvi2eis2venc  clean
	make -C sample -f tina.mk TARGET=sample_virvi2fish2venc  clean
	
	make -C sample -f tina.mk TARGET=sample_virvi2fish2vo  clean
	make -C sample -f tina.mk TARGET=sample_virvi2venc  clean
	make -C sample -f tina.mk TARGET=sample_virvi2venc2ce  clean
	make -C sample -f tina.mk TARGET=sample_virvi2venc2muxer  clean
	make -C sample -f tina.mk TARGET=sample_virvi2vo  clean
	
	make -C sample -f tina.mk TARGET=sample_vo  clean

	make -C sample -f tina.mk TARGET=sample_recorder clean
	make -C sample -f tina.mk TARGET=sample_odet_demo  clean

	@echo clean eyesee-mpp-middleware done!
