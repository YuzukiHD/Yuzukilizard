TARGET_PATH :=$(call my-dir)

################################################################################
include $(ENV_CLEAR)

include $(TARGET_TOP)/middleware/config/mpp_config.mk
include $(TARGET_TOP)/middleware/media/LIBRARY/libcedarx/config.mk

ifeq ($(MPPCFG_COMPILE_STATIC_LIB), Y)

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
    mpi_vdec.c \
    mpi_mux.c \
    mpi_demux.c \
    mpi_clock.c \
    mpi_region.c \
    ChannelRegionInfo.c \
    mpi_vi.c \
    mpi_vo.c \
    mpi_isp.c \
    component/mm_component.c \
    component/ComponentsRegistryTable.c \
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
    component/VideoVirVi_Component.c \
    audio/alsa_interface.c \
    audio/audio_hw.c \
    audio/pcmBufferManager.c \
    librender/video_render.c \
    librender/video_render_linux.cpp \
    librender/CedarXNativeRenderer.cpp \
    videoIn/videoInputHw.c \
    videoIn/VIPPDrawOSD_V5.c \
    component/VideoRender_Component.c \

ifeq ($(MPPCFG_ISE),Y)
TARGET_SRC += \
    mpi_ise.c \
    component/VideoISE_Component.c
endif

ifeq ($(MPPCFG_EIS),Y)
TARGET_SRC += \
    mpi_eis.c \
    component/VideoEIS_Component.c
endif

ifeq ($(MPPCFG_VIDEOSTABILIZATION),Y)
TARGET_SRC += videoIn/anti-shake.c
TARGET_SRC += videoIn/generic_buffer.c
TARGET_SRC += videoIn/iio_utils.c
endif

ifeq ($(MPPCFG_UVC),Y)
TARGET_SRC += mpi_uvc.c
TARGET_SRC += component/UvcVirVi_Component.c
TARGET_SRC += videoIn/uvcInput.c
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
    $(TARGET_TOP)/middleware/media/include/include_render \
    $(TARGET_TOP)/middleware/media/librender \
    $(TARGET_TOP)/middleware/media/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/libcedarc/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/AudioLib/osal \
    $(TARGET_TOP)/middleware/media/LIBRARY/AudioLib/midware/encoding \
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
    $(TARGET_TOP)/middleware/media/LIBRARY/libkfc/include

TARGET_STATIC_LIB :=
#ifeq ($(MPPCFG_USE_KFC),Y)
#    TARGET_STATIC_LIB += lib_hal
#endif

TARGET_CPPFLAGS += -fPIC -Wall -Wno-multichar -Wno-unused-function -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-label -mfpu=neon-vfpv4 -D__OS_LINUX
TARGET_CFLAGS += -fPIC -Wall -mfloat-abi=softfp -Wno-unused-function -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-label -mfpu=neon-vfpv4 -D__OS_LINUX

TARGET_MODULE := libaw_mpp

include $(BUILD_STATIC_LIB)

endif

