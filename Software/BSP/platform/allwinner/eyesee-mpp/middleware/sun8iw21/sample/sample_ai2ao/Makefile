# Makefile for eyesee-mpp/dragonboard/apps/DragonBoard/testcase/mictester
CUR_PATH := .
PACKAGE_TOP := ../../../..
EYESEE_MPP_INCLUDE:=$(STAGING_DIR)/usr/include/eyesee-mpp
EYESEE_MPP_LIBDIR:=$(STAGING_DIR)/usr/lib/eyesee-mpp
# STAGING_DIR is exported in rules.mk, so it can be used directly here.
# STAGING_DIR:=.../tina-v316/out/v316-perfnor/staging_dir/target

-include $(EYESEE_MPP_INCLUDE)/middleware/config/mpp_config.mk

# used to store all the generated testcase bin files
TESTCASE_BIN_DIR = 	../../bin

#set source files here.
SRCCS := \
	./sample_ai2ao.c

#include directories
INCLUDE_DIRS := \
	$(CUR_PATH) \
	$(PACKAGE_TOP)/include \
    $(EYESEE_MPP_INCLUDE)/middleware/include \
    $(EYESEE_MPP_INCLUDE)/middleware/include/utils \
    $(EYESEE_MPP_INCLUDE)/middleware/include/media \
    $(EYESEE_MPP_INCLUDE)/middleware/media/include/utils \
    $(EYESEE_MPP_INCLUDE)/middleware/media/include/component \
    $(EYESEE_MPP_INCLUDE)/middleware/media/LIBRARY/libisp/include \
    $(EYESEE_MPP_INCLUDE)/middleware/media/LIBRARY/libisp/include/V4l2Camera \
    $(EYESEE_MPP_INCLUDE)/middleware/media/LIBRARY/libisp/isp_tuning \
    $(EYESEE_MPP_INCLUDE)/middleware/media/LIBRARY/include_ai_common \
    $(EYESEE_MPP_INCLUDE)/middleware/media/LIBRARY/libaiMOD/include \
    $(EYESEE_MPP_INCLUDE)/middleware/media/LIBRARY/libVLPR/include \
    $(EYESEE_MPP_INCLUDE)/middleware/media/LIBRARY/include_muxer \
    $(EYESEE_MPP_INCLUDE)/middleware/media/LIBRARY/libADAS/include \
    $(EYESEE_MPP_INCLUDE)/middleware/media/LIBRARY/include_eve_common \
    $(EYESEE_MPP_INCLUDE)/middleware/media/LIBRARY/libeveface/include \
    $(EYESEE_MPP_INCLUDE)/middleware/media/LIBRARY/include_stream \
    $(EYESEE_MPP_INCLUDE)/middleware/media/LIBRARY/include_FsWriter \
    $(EYESEE_MPP_INCLUDE)/middleware/media/LIBRARY/libcedarc/include \
    $(EYESEE_MPP_INCLUDE)/middleware/media/LIBRARY/libcedarx/libcore/common/iniparser \
    $(EYESEE_MPP_INCLUDE)/middleware/sample/configfileparser \
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
    libmedia_utils \
	libResample

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

endif

#set dst file name: shared library, static library, execute bin.
LOCAL_TARGET_DYNAMIC :=
LOCAL_TARGET_STATIC :=
LOCAL_TARGET_BIN := mictester

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
    $(EYESEE_MPP_LIBDIR)

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
	-mkdir -p $(TESTCASE_BIN_DIR)
	-cp -f $(LOCAL_TARGET_BIN) $(TESTCASE_BIN_DIR)
	-cp -f $(wildcard *.conf $(TARGET)/*.conf) $(TESTCASE_BIN_DIR)
	@echo ===================================
	@echo build eyesee-mpp/dragonboard/apps/DragonBoard/testcase/mictester-$(LOCAL_TARGET_BIN) done
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
