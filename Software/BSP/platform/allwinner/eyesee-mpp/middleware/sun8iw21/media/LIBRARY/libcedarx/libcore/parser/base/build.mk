TARGET_PATH:= $(call my-dir)

include $(ENV_CLEAR)

CEDARX_ROOT=$(TARGET_PATH)/../../../
include $(CEDARX_ROOT)/config.mk

TARGET_SRC = \
                $(notdir $(wildcard $(TARGET_PATH)/*.c)) \
				./id3base/StringContainer.c \
				./id3base/Id3Base.c \
				./id3base/CdxUtfCode.c \
				./id3base/CdxMetaData.c \

TARGET_INC:= \
    $(CEDARX_ROOT)/ \
    $(CEDARX_ROOT)/libcore \
    $(CEDARX_ROOT)/libcore/include \
    $(CEDARX_ROOT)/libcore/base/include \
    $(CEDARX_ROOT)/libcore/parser/include \
    $(CEDARX_ROOT)/libcore/stream/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/libcedarc/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/AudioLib/osal \
    $(TARGET_TOP)/middleware/media/LIBRARY/AudioLib/midware/decoding/include \
    $(TARGET_PATH)/id3base/


TARGET_CFLAGS += -fPIC -Wall -Wno-psabi -D__OS_LINUX

TARGET_MODULE:= libcdx_parser

TARGET_SHARED_LIB := libcdx_stream libcdx_base

TARGET_SHARED_LIB += libz libdl

TARGET_STATIC_LIB = \
	libcdx_ts_parser \
	libcdx_mov_parser \
	libcdx_mpg_parser \
	libcdx_mp3_parser \
	libcdx_id3v2_parser \
	libcdx_aac_parser \

#	libcdx_remux_parser \
#	libcdx_asf_parser \
#	libcdx_avi_parser \
#	libcdx_flv_parser \
#	libcdx_mms_parser \
#	libcdx_dash_parser \
#	libcdx_hls_parser \
#	libcdx_mkv_parser \
#	libcdx_bd_parser \
#	libcdx_pmp_parser \
#	libcdx_ogg_parser \
#	libcdx_m3u9_parser \
#	libcdx_playlist_parser \
#	libcdx_wav_parser \
#	libcdx_ape_parser \
#	libcdx_flac_parser \
#	libcdx_amr_parser \
#	libcdx_atrac_parser \
#	libcdx_mmshttp_parser\
#	libcdx_awts_parser\
#	libcdx_sstr_parser \
#	libcdx_caf_parser \
#	libcdx_g729_parser \
#	libcdx_dsd_parser \
#	libcdx_aiff_parser \
#	libcdx_awrawstream_parser\
#	libcdx_awspecialstream_parser \
#	libcdx_pls_parser

#TARGET_STATIC_LIB += libxml2

include $(BUILD_SHARED_LIB)

