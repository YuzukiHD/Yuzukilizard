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
    sample_virvi.c

#include directories
INCLUDE_DIRS := \
    $(CUR_PATH) \
    $(EYESEE_MPP_INCLUDE)/system/public/include \
    $(EYESEE_MPP_INCLUDE)/system/public/include/utils \
    $(EYESEE_MPP_INCLUDE)/system/public/include/kernel-headers \
    $(PACKAGE_TOP)/include/utils \
    $(PACKAGE_TOP)/include/media \
    $(PACKAGE_TOP)/include \
    $(PACKAGE_TOP)/media/include/utils \
    $(PACKAGE_TOP)/media/include/component \
    $(PACKAGE_TOP)/media/LIBRARY/libisp/include \
    $(PACKAGE_TOP)/media/LIBRARY/libisp/include/V4l2Camera \
    $(PACKAGE_TOP)/media/LIBRARY/libisp/isp_tuning \
    $(PACKAGE_TOP)/media/LIBRARY/libAIE_Vda/include \
    $(PACKAGE_TOP)/media/LIBRARY/include_stream \
    $(PACKAGE_TOP)/media/LIBRARY/include_FsWriter \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarc/include \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarx/libcore/common/iniparser \
    $(PACKAGE_TOP)/sample/configfileparser

ifeq ($(MPPCFG_COMPILE_DYNAMIC_LIB), Y)
LOCAL_SHARED_LIBS := \
    libdl \
    librt \
    libpthread \
    liblog \
    libMemAdapter \
    libvdecoder \
    libmedia_utils \
    libmedia_mpp \
    libmpp_vi \
    libmpp_vo \
    libmpp_isp \
    libmpp_ise \
    libmpp_component \
    libsample_confparser \
    libISP
LOCAL_STATIC_LIBS :=
else
LOCAL_SHARED_LIBS := \
    libdl \
    librt \
    libpthread \
    liblog \
    libion \
    libsample_confparser \
    libhwdisplay \
    libasound \
    libcutils \
    libcdx_common \
    libcdx_base \
    libcdx_parser

LOCAL_STATIC_LIBS := \
    libaw_mpp \
    libmedia_utils \
    libcedarx_aencoder \
    libaacenc \
    libmp3enc \
    libadecoder \
    libcedarxdemuxer \
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
    libmatrix \
    libmuxers \
    libmp4_muxer \
    libraw_muxer \
    libmpeg2ts_muxer \
    libaac_muxer \
    libmp3_muxer \
    libffavutil \
    libFsWriter \
    libcedarxstream \
    libion

    ifeq ($(MPPCFG_EIS),Y)
        LOCAL_STATIC_LIBS += \
            libEIS \
            lib_eis
    endif
    ifeq ($(MPPCFG_ISE_BI),Y)
        LOCAL_STATIC_LIBS += lib_ise_bi
    endif
    ifeq ($(MPPCFG_ISE_MO),Y)
        LOCAL_STATIC_LIBS += lib_ise_mo
    endif
    ifeq ($(MPPCFG_ISE_STI),Y)
        LOCAL_STATIC_LIBS += lib_ise_sti
    endif

endif

#set dst file name: shared library, static library, execute bin.
LOCAL_TARGET_DYNAMIC :=
LOCAL_TARGET_STATIC :=
LOCAL_TARGET_BIN := sample_virvi

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
    $(PACKAGE_TOP)/media/LIBRARY/libcedarx/libcore/parser/base \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarx/libcore/stream/base \
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
    $(PACKAGE_TOP)/media/LIBRARY/libisp \
    $(PACKAGE_TOP)/media/LIBRARY/libisp/out \
    $(PACKAGE_TOP)/media/LIBRARY/libisp/isp_cfg \
    $(PACKAGE_TOP)/media/LIBRARY/libisp/isp_dev \
    $(PACKAGE_TOP)/media/LIBRARY/libisp/iniparser \
    $(PACKAGE_TOP)/media/LIBRARY/libISE/out \
    $(PACKAGE_TOP)/media/LIBRARY/libVideoStabilization \
    $(PACKAGE_TOP)/media/LIBRARY/libVideoStabilization/out \
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

