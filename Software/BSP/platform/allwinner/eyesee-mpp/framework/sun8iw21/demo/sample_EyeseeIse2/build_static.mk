TARGET_PATH :=$(call my-dir)
TOP_PATH := $(TARGET_PATH)

#########################################
include $(ENV_CLEAR)

TARGET_SRC := \
	sample_EyeseeIse.cpp

TARGET_INC := \
    $(TARGET_TOP)/framework/include \
    $(TARGET_TOP)/framework/include/media \
    $(TARGET_TOP)/framework/include/media/camera \
    $(TARGET_TOP)/framework/include/media/ise \
    $(TARGET_TOP)/framework/include/media/recorder \
    $(TARGET_TOP)/framework/include/utils \
    $(TARGET_TOP)/system/public/libion/include \
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
    $(TARGET_TOP)/middleware/media/LIBRARY/libAIE_Vda/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/include_stream \
    $(TARGET_TOP)/middleware/media/LIBRARY/include_FsWriter \
    $(TARGET_TOP)/middleware/media/LIBRARY/include_muxer \
    $(TARGET_TOP)/middleware/media/LIBRARY/libcedarc/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/libcedarx/libcore/common/iniparser \
    $(TARGET_TOP)/middleware/media/LIBRARY/libaiMOD/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/include_ai_common \
    $(TARGET_TOP)/middleware/media/LIBRARY/include_eve_common \
    $(TARGET_TOP)/middleware/media/LIBRARY/libVLPR/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/libeveface/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/libISE \
    $(TARGET_TOP)/middleware/sample/configfileparser

TARGET_SHARED_LIB := \
    libion \
    liblog \
    libpthread \
    libhwdisplay \
    libcdx_common \
    libcdx_base \
    libsample_confparser \
    libasound \
    libcdx_parser \
    libcutils \

TARGET_STATIC_LIB := \
    libcamera \
    librecorder \
    libise \
    libaw_mpp \
    libEIS \
    lib_eis \
    libISP \
    libcedarx_aencoder \
    libcedarxdemuxer \
    libvdecoder \
    libadecoder \
    lib_ise_mo \
    libmuxers \
    libmp4_muxer \
    libisp_base \
    libisp_ini \
    libisp_ae \
    libisp_af \
    libisp_afs \
    libisp_awb \
    libisp_gtm \
    libisp_iso \
    libisp_math \
    libisp_md \
    libisp_pltm \
    libisp_rolloff \
    libmatrix \
    libiniparser \
    libisp_dev \
    libaacenc \
    libmp3enc \
    libg711enc \
    libg726enc \
    libadpcmenc \
    libvideoengine \
    libmp3_muxer \
    libaac_muxer \
    libraw_muxer \
    libmpeg2ts_muxer \
    libFsWriter \
    libcedarxstream \
    libffavutil \
    libVE \
    libawh264 \
    libawh265 \
    libawmjpegplus \
    libMemAdapter \
    libmedia_utils \
    libvencoder \
    libvenc_base \
    libcustomaw_media_utils \
    libcdc_base \
    libvenc_codec

TARGET_CPPFLAGS += -fPIC -Wall -Wno-unused-but-set-variable -ldl
TARGET_CFLAGS += -fPIC -Wall -Wno-unused-but-set-variable

TARGET_MODULE := sample_EyeseeIse

include $(BUILD_BIN)
#########################################
