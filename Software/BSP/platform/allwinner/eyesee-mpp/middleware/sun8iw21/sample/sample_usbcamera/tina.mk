# Makefile for eyesee-mpp/middleware/sample/sample_vo
CUR_PATH := .
PACKAGE_TOP := ../..
EYESEE_MPP_INCLUDE:=$(STAGING_DIR)/usr/include/eyesee-mpp
EYESEE_MPP_LIBDIR:=$(STAGING_DIR)/usr/lib/eyesee-mpp
# STAGING_DIR is exported in rules.mk, so it can be used directly here.
# STAGING_DIR:=.../tina-v316/out/v316-perfnor/staging_dir/target

include $(PACKAGE_TOP)/config/mpp_config.mk

#set source files here.
SRCCS := \
	./sample_usbcamera.c

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
    $(PACKAGE_TOP)/media/LIBRARY/include_muxer \
    $(PACKAGE_TOP)/media/LIBRARY/libfilerepair/include \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarx/libcore/common/iniparser \
    $(PACKAGE_TOP)/media/LIBRARY/libBodyDetect/include \
    $(PACKAGE_TOP)/sample/configfileparser \
    $(LINUX_USER_HEADERS)/include

ifeq ($(MPPCFG_COMPILE_DYNAMIC_LIB), Y)
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
    libaw_g726dec \
    libaw_g711adec \
    libaw_g711udec
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
ifeq ($(MPPCFG_BODY_DETECT),Y)
LOCAL_SHARED_LIBS += \
    libawipubsp \
    libawnn \
    libpdet
endif

# These libraries are only used by the corresponding samples
ifeq ($(TARGET), sample_rtsp)
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

## static lib mode
else
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
    libcdx_base

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
ifeq ($(MPPCFG_HW_DISPLAY),Y)
LOCAL_STATIC_LIBS += \
	libhwdisplay
endif
ifeq ($(MPPCFG_VO),Y)
LOCAL_STATIC_LIBS += \
    libcedarxrender
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
    libcedarx_aencoder \
    libaacenc \
    libmp3enc
endif
ifeq ($(MPPCFG_ADEC), Y)
LOCAL_STATIC_LIBS += \
    libadecoder \
    libaac \
    libmp3 \
    libwav \
    libaw_g726dec \
    libaw_g711adec \
    libaw_g711udec
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
ifeq ($(MPPCFG_BODY_DETECT),Y)
LOCAL_STATIC_LIBS += \
    libawipubsp \
    libawnn \
    libpdet
endif
# These libraries are only used by the corresponding samples
ifeq ($(TARGET), sample_rtsp)
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

#set dst file name: shared library, static library, execute bin.
LOCAL_TARGET_DYNAMIC :=
LOCAL_TARGET_STATIC :=
LOCAL_TARGET_BIN := sample_usbcamera

#generate include directory flags for gcc.
inc_paths := $(foreach inc,$(filter-out -I%,$(INCLUDE_DIRS)),$(addprefix -I, $(inc))) \
                $(filter -I%, $(INCLUDE_DIRS))
#Extra flags to give to the C compiler
LOCAL_CFLAGS := $(CFLAGS) $(CEDARX_EXT_CFLAGS) $(inc_paths) -fPIC -Wall -Wno-unused-but-set-variable
#Extra flags to give to the C++ compiler
LOCAL_CXXFLAGS := $(CXXFLAGS) $(CEDARX_EXT_CFLAGS) $(inc_paths) -fPIC -Wall -Wno-unused-but-set-variable
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
    $(PACKAGE_TOP)/media/LIBRARY/libcedarc/vdecoder/videoengine/mjpegplus \
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
    $(PACKAGE_TOP)/media/LIBRARY/libisp \
    $(PACKAGE_TOP)/media/LIBRARY/libisp/out/out \
    $(PACKAGE_TOP)/media/LIBRARY/libisp/isp_cfg \
    $(PACKAGE_TOP)/media/LIBRARY/libisp/isp_dev \
    $(PACKAGE_TOP)/media/LIBRARY/libisp/iniparser \
    $(PACKAGE_TOP)/media/LIBRARY/libISE/out \
    $(PACKAGE_TOP)/media/LIBRARY/libVideoStabilization \
    $(PACKAGE_TOP)/media/LIBRARY/libVideoStabilization/out \
    $(PACKAGE_TOP)/media/LIBRARY/libBodyDetect/out \
    $(PACKAGE_TOP)/media/librender
empty:=
space:= $(empty) $(empty)

LOCAL_BIN_LDFLAGS := $(LOCAL_LDFLAGS) \
    $(patsubst %,-L%,$(LIB_SEARCH_PATHS)) \
    -Wl,-rpath-link=$(subst $(space),:,$(strip $(LIB_SEARCH_PATHS))) \
    -Wl,-Bstatic \
    -Wl,--start-group $(foreach n, $(LOCAL_STATIC_LIBS), -l$(patsubst lib%,%,$(patsubst %.a,%,$(notdir $(n))))) -Wl,--end-group \
    -Wl,-Bdynamic \
    $(foreach y, $(LOCAL_SHARED_LIBS), -l$(patsubst lib%,%,$(patsubst %.so,%,$(notdir $(y)))))

#generate object files
OBJS := $(SRCCS:%=%.o) #OBJS=$(patsubst %,%.o,$(SRCCS))

#add dynamic lib name suffix and static lib name suffix.
target_dynamic := $(if $(LOCAL_TARGET_DYNAMIC),$(addsuffix .so,$(LOCAL_TARGET_DYNAMIC)),)
target_static := $(if $(LOCAL_TARGET_STATIC),$(addsuffix .a,$(LOCAL_TARGET_STATIC)),)

#generate exe file.
.PHONY: all
all: $(LOCAL_TARGET_BIN)
	@echo ===================================
	@echo build eyesee-mpp-middleware-sample-$(LOCAL_TARGET_BIN) done
	@echo ===================================

$(target_dynamic): $(OBJS)
	$(CXX) $+ $(LOCAL_DYNAMIC_LDFLAGS) -o $@
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

$(LOCAL_TARGET_BIN): $(OBJS)
	$(CXX) $+ $(LOCAL_BIN_LDFLAGS) -o $@
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
	-rm -f $(OBJS) $(OBJS:%=%.d) $(target_dynamic) $(target_static) $(LOCAL_TARGET_BIN)

#add *.h prerequisites
-include $(OBJS:%=%.d)
