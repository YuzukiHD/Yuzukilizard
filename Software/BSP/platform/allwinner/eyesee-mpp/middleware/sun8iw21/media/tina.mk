# Makefile for eyesee-mpp/middleware/media
CUR_PATH := .
PACKAGE_TOP := ..
EYESEE_MPP_INCLUDE:=$(STAGING_DIR)/usr/include/eyesee-mpp
EYESEE_MPP_LIBDIR:=$(STAGING_DIR)/usr/lib/eyesee-mpp
# STAGING_DIR is exported in rules.mk, so it can be used directly here.
# STAGING_DIR:=.../tina-v316/out/v316-perfnor/staging_dir/target

include $(PACKAGE_TOP)/config/mpp_config.mk
include $(PACKAGE_TOP)/media/LIBRARY/libcedarx/config.mk
#set source files here.
SRCCS := \
    mpi_sys.c \
    mpi_clock.c \
    mpi_region.c \
    ChannelRegionInfo.c \
    component/Clock_Component.c \
    component/RecAVSync.c

ifeq ($(MPPCFG_TEXTENC),Y)
SRCCS += \
    mpi_tenc.c \
    component/TextEnc_Component.c
endif
ifeq ($(MPPCFG_VENC),Y)
SRCCS += \
    mpi_venc.c \
    component/VideoEnc_Component.c
endif
ifeq ($(MPPCFG_VDEC),Y)
SRCCS += \
    mpi_vdec.c \
    component/VideoDec_Component.c \
    component/VideoDec_InputThread.c
endif
ifeq ($(MPPCFG_AIO),Y)
SRCCS += \
    mpi_ai.c \
    mpi_ao.c \
    component/AIChannel_Component.c \
    component/AOChannel_Component.c \
    audio/alsa_interface.c \
    audio/audio_hw.c \
    audio/pcmBufferManager.c
endif
ifeq ($(MPPCFG_AENC),Y)
SRCCS += \
    mpi_aenc.c \
    component/AudioEnc_Component.c
endif
ifeq ($(MPPCFG_ADEC),Y)
SRCCS += \
    mpi_adec.c \
    component/AudioDec_Component.c
endif
ifeq ($(MPPCFG_MUXER),Y)
SRCCS += \
    mpi_mux.c \
    component/RecRenderSink.c \
    component/RecRender_cache.c \
    component/RecRender_Component.c
endif
ifeq ($(MPPCFG_DEMUXER),Y)
SRCCS += \
    mpi_demux.c \
    component/Demux_Component.c
endif
#include directories
INCLUDE_DIRS := \
    $(EYESEE_MPP_INCLUDE)/system/public/include \
    $(EYESEE_MPP_INCLUDE)/system/public/include/utils \
    $(EYESEE_MPP_INCLUDE)/system/public/include/vo \
    $(EYESEE_MPP_INCLUDE)/system/public/include/kernel-headers \
    $(EYESEE_MPP_INCLUDE)/system/public/libion/include \
    $(PACKAGE_TOP)/config \
    $(PACKAGE_TOP)/include/utils \
    $(PACKAGE_TOP)/include/media \
    $(PACKAGE_TOP)/include/media/utils \
    $(PACKAGE_TOP)/include \
    $(PACKAGE_TOP)/media/include/utils \
    $(PACKAGE_TOP)/media/include/component \
    $(PACKAGE_TOP)/media/component \
    $(PACKAGE_TOP)/media/include/include_render \
    $(PACKAGE_TOP)/media/include/videoIn \
    $(PACKAGE_TOP)/media/include/audio \
    $(PACKAGE_TOP)/media/include \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarc/include \
    $(PACKAGE_TOP)/media/LIBRARY/AudioLib/osal \
    $(PACKAGE_TOP)/media/LIBRARY/AudioLib/midware/encoding \
    $(PACKAGE_TOP)/media/LIBRARY/AudioLib/midware/encoding/include \
    $(PACKAGE_TOP)/media/LIBRARY/AudioLib/midware/decoding/include \
    $(PACKAGE_TOP)/media/LIBRARY/audioEffectLib/include \
    $(PACKAGE_TOP)/media/LIBRARY/textEncLib/include \
    $(PACKAGE_TOP)/media/LIBRARY/include_demux \
    $(PACKAGE_TOP)/media/LIBRARY/include_muxer \
    $(PACKAGE_TOP)/media/LIBRARY/include_FsWriter \
    $(PACKAGE_TOP)/media/LIBRARY/include_stream \
    $(PACKAGE_TOP)/media/LIBRARY/libmuxer/mp4_muxer \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarx/libcore/include \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarx/libcore/base/include \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarx/libcore/common/iniparser \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarx/libcore/common/plugin \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarx/libcore/parser/include \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarx/libcore/stream/include \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarx \
    $(PACKAGE_TOP)/media/LIBRARY/include_ai_common \
    $(PACKAGE_TOP)/media/LIBRARY/libISE/include \
    $(PACKAGE_TOP)/media/LIBRARY/libVLPR/include \
    $(PACKAGE_TOP)/media/LIBRARY/libisp/include/V4l2Camera \
    $(PACKAGE_TOP)/media/LIBRARY/libisp/include/device \
    $(PACKAGE_TOP)/media/LIBRARY/libisp/include \
    $(PACKAGE_TOP)/media/LIBRARY/libisp/isp_dev \
    $(PACKAGE_TOP)/media/LIBRARY/libisp/isp_tuning \
    $(PACKAGE_TOP)/media/LIBRARY/libisp \
    $(PACKAGE_TOP)/media/LIBRARY/libVideoStabilization \
    $(PACKAGE_TOP)/media/LIBRARY/libkfc/include \
    $(PACKAGE_TOP)/media/LIBRARY/lib_aw_ai_core/include \
    $(PACKAGE_TOP)/media/LIBRARY/lib_aw_ai_mt/include \
    $(PACKAGE_TOP)/media/LIBRARY/aec_lib/include \
    $(PACKAGE_TOP)/media/LIBRARY/ans_lib/include \
    $(PACKAGE_TOP)/media/LIBRARY/drc_lib/include \
    $(PACKAGE_TOP)/media/LIBRARY/agc_lib/include \
    $(PACKAGE_TOP)/media/LIBRARY/libResample/include \
    $(LINUX_USER_HEADERS)/include

LOCAL_SHARED_LIBS := \
    librt \
    libpthread \
    libion \
    liblog \
    libmedia_utils \
    libcedarxstream \
    libMemAdapter \
    libcdx_common \
    libasound \
    libmpp_component

ifeq ($(MPPCFG_VI),Y)
LOCAL_SHARED_LIBS += \
    libmpp_vi \
    libmpp_isp
endif

ifeq ($(MPPCFG_VO),Y)
LOCAL_SHARED_LIBS += libmpp_vo libhwdisplay
endif

ifeq ($(MPPCFG_TEXTENC),Y)
LOCAL_SHARED_LIBS += libcedarx_tencoder
endif

ifeq ($(MPPCFG_VENC),Y)
LOCAL_SHARED_LIBS += \
    libvencoder \
    libvenc_base \
    libvenc_codec
endif

ifeq ($(MPPCFG_VDEC),Y)
LOCAL_SHARED_LIBS += \
    libvdecoder
endif

ifeq ($(MPPCFG_AENC),Y)
LOCAL_SHARED_LIBS += \
    libcedarx_aencoder
endif

ifeq ($(MPPCFG_ADEC),Y)
LOCAL_SHARED_LIBS += \
    libadecoder
endif

ifeq ($(MPPCFG_DEMUXER),Y)
LOCAL_SHARED_LIBS += \
    libcdx_parser \
    libcdx_stream
endif

ifeq ($(MPPCFG_ISE),Y)
    LOCAL_SHARED_LIBS += libmpp_ise
endif

ifeq ($(MPPCFG_EIS),Y)
    LOCAL_SHARED_LIBS += libmpp_eis
endif

ifeq ($(MPPCFG_UVC),Y)
    LOCAL_SHARED_LIBS += libmpp_uvc
endif

ifeq ($(MPPCFG_AIO),Y)
	LOCAL_SHARED_LIBS += libResample
endif

LOCAL_STATIC_LIBS :=

ifeq ($(MPPCFG_AEC),Y)
LOCAL_STATIC_LIBS += \
    libAec
endif
ifeq ($(MPPCFG_ANS),Y)
LOCAL_STATIC_LIBS += \
    libAns
endif
ifeq ($(MPPCFG_SOFTDRC),Y)
LOCAL_STATIC_LIBS += \
    libDrc
endif
ifneq ($(filter Y, $(MPPCFG_AI_AGC) $(MPPCFG_AGC)),)
LOCAL_STATIC_LIBS += \
    libAgc
endif

ifeq ($(MPPCFG_MUXER),Y)
LOCAL_STATIC_LIBS += \
    libmuxers \
    libmp3_muxer \
    libaac_muxer \
    libmp4_muxer \
    libmpeg2ts_muxer \
    libffavutil \
    libraw_muxer \
    libFsWriter
endif

ifeq ($(MPPCFG_DEMUXER),Y)
LOCAL_STATIC_LIBS += libcedarxdemuxer
endif

#set dst file name: shared library, static library, execute bin.
ifeq ($(MPPCFG_COMPILE_DYNAMIC_LIB), Y)
LOCAL_TARGET_DYNAMIC := libmedia_mpp
endif
LOCAL_TARGET_STATIC :=
LOCAL_TARGET_BIN :=

#generate include directory flags for gcc.
inc_paths := $(foreach inc,$(filter-out -I%,$(INCLUDE_DIRS)),$(addprefix -I, $(inc))) \
                $(filter -I%, $(INCLUDE_DIRS))
#Extra flags to give to the C compiler
LOCAL_CFLAGS := $(CFLAGS) $(CEDARX_CFLAGS) $(CEDARX_EXT_CFLAGS) $(inc_paths) -fPIC -Wall \
    -Wno-unused-function -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-label -D__OS_LINUX
#Extra flags to give to the C++ compiler
LOCAL_CXXFLAGS := $(CXXFLAGS) $(CEDARX_CFLAGS) $(CEDARX_EXT_CFLAGS) $(inc_paths) -fPIC -Wall \
    -Wno-unused-function -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-label -D__OS_LINUX
#Extra flags to give to the C preprocessor and programs that use it (the C and Fortran compilers).
LOCAL_CPPFLAGS := $(CPPFLAGS)
#target device arch: x86, arm
LOCAL_TARGET_ARCH := $(ARCH)
#Extra flags to give to compilers when they are supposed to invoke the linker,‘ld’.
LOCAL_LDFLAGS := $(LDFLAGS)

LOCAL_DYNAMIC_LDFLAGS := $(LOCAL_LDFLAGS) -shared \
    -L $(EYESEE_MPP_LIBDIR) \
    -L $(PACKAGE_TOP)/media/LIBRARY/libmuxer/muxers \
    -L $(PACKAGE_TOP)/media/LIBRARY/libmuxer/mp3_muxer \
    -L $(PACKAGE_TOP)/media/LIBRARY/libmuxer/aac_muxer \
    -L $(PACKAGE_TOP)/media/LIBRARY/libmuxer/mp4_muxer \
    -L $(PACKAGE_TOP)/media/LIBRARY/libmuxer/mpeg2ts_muxer \
    -L $(PACKAGE_TOP)/media/LIBRARY/libmuxer/common/libavutil \
    -L $(PACKAGE_TOP)/media/LIBRARY/libmuxer/raw_muxer \
    -L $(PACKAGE_TOP)/media/LIBRARY/libFsWriter \
    -L $(PACKAGE_TOP)/media/LIBRARY/libdemux \
    -L $(PACKAGE_TOP)/media/utils \
    -L $(PACKAGE_TOP)/media/LIBRARY/libstream \
    -L $(PACKAGE_TOP)/media/LIBRARY/libcedarc/memory \
    -L $(PACKAGE_TOP)/media/LIBRARY/libcedarc/vencoder \
    -L $(PACKAGE_TOP)/media/LIBRARY/libcedarc/vencoder/base \
    -L $(PACKAGE_TOP)/media/LIBRARY/libcedarc/vencoder/libcodec \
    -L $(PACKAGE_TOP)/media/LIBRARY/libcedarc/library/out \
    -L $(PACKAGE_TOP)/media/LIBRARY/libcedarx/libcore/common \
    -L $(PACKAGE_TOP)/media/LIBRARY/libcedarx/libcore/parser/base \
    -L $(PACKAGE_TOP)/media/LIBRARY/libcedarx/libcore/stream/base \
    -L $(PACKAGE_TOP)/media/LIBRARY/libcedarc/vdecoder \
    -L $(PACKAGE_TOP)/media/LIBRARY/AudioLib/midware/decoding \
    -L $(PACKAGE_TOP)/media/LIBRARY/AudioLib/midware/encoding \
    -L $(PACKAGE_TOP)/media/LIBRARY/aec_lib/out \
    -L $(PACKAGE_TOP)/media/LIBRARY/agc_lib/out \
    -L $(PACKAGE_TOP)/media/LIBRARY/ans_lib/out \
    -L $(PACKAGE_TOP)/media/LIBRARY/drc_lib/out \
    -L $(PACKAGE_TOP)/media/LIBRARY/textEncLib \
    -L $(PACKAGE_TOP)/media/LIBRARY/libResample \
    -L $(PACKAGE_TOP)/media \
    -L $(PACKAGE_TOP)/media/component \
    -Wl,-Bstatic \
    -Wl,--start-group $(foreach n, $(LOCAL_STATIC_LIBS), -l$(patsubst lib%,%,$(patsubst %.a,%,$(notdir $(n))))) -Wl,--end-group \
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
#add dynamic lib name suffix and static lib name suffix.
target_dynamic := $(if $(LOCAL_TARGET_DYNAMIC),$(addsuffix .so,$(LOCAL_TARGET_DYNAMIC)),)
target_static := $(if $(LOCAL_TARGET_STATIC),$(addsuffix .a,$(LOCAL_TARGET_STATIC)),)

#generate exe file.
.PHONY: all
all: $(target_dynamic) $(target_static)
	@echo ===================================
	@echo build eyesee-mpp-middleware-media done
	@echo ===================================

$(target_dynamic): $(OBJS) $(DEPEND_LIBS)
	$(CXX) $(OBJS) $(LOCAL_DYNAMIC_LDFLAGS) -o $@
	@echo ----------------------------
	@echo "finish target: $@"
#	@echo "object files:  $+"
#	@echo "source files:  $(SRCCS)"
	@echo ----------------------------

$(target_static): $(OBJS)
	$(AR) -rcs -o $@ $+
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
	-rm -f $(OBJS) $(OBJS:%=%.d) $(target_dynamic) $(target_static)

#add *.h prerequisites
-include $(OBJS:%=%.d)

