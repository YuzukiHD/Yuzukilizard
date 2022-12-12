# Makefile for eyesee-mpp/framework/demo/sample_recorderCallbackOut
CUR_PATH := .
PACKAGE_TOP := ../..
EYESEE_MPP_INCLUDE:=$(STAGING_DIR)/usr/include/eyesee-mpp
EYESEE_MPP_LIBDIR:=$(STAGING_DIR)/usr/lib/eyesee-mpp
# STAGING_DIR is exported in rules.mk, so it can be used directly here.
# STAGING_DIR:=.../tina-v316/out/v316-perfnor/staging_dir/target

-include $(EYESEE_MPP_INCLUDE)/middleware/config/mpp_config.mk

#set source files here.
SRCCS := \
    sample_RecorderCallbackOut.cpp

#include directories
INCLUDE_DIRS := \
    $(CUR_PATH) \
    $(PACKAGE_TOP)/include \
    $(PACKAGE_TOP)/include/media \
    $(PACKAGE_TOP)/include/media/camera \
    $(PACKAGE_TOP)/include/media/ise \
    $(PACKAGE_TOP)/include/media/recorder \
    $(PACKAGE_TOP)/include/utils \
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
    $(LINUX_USER_HEADERS)/include

ifeq ($(MPPCFG_COMPILE_DYNAMIC_LIB), Y)
LOCAL_SHARED_LIBS := \
    liblog \
    libpthread \
    libhwdisplay \
    libcdx_common \
    libsample_confparser \
    libcedarxrender \
    libmpp_vi \
    libmpp_isp \
    libmpp_ise \
    libmpp_vo \
    libmpp_component \
    libmedia_utils \
    libmedia_mpp \
    libISP \
    libMemAdapter \
    libvencoder \
    libcustomaw_media_utils
ifeq ($(MPPCFG_MOD),Y)
    LOCAL_SHARED_LIBS += libai_MOD
endif
ifeq ($(MPPCFG_EVEFACE),Y)
    LOCAL_SHARED_LIBS += libeve_event
endif
ifeq ($(MPPCFG_VLPR),Y)
    LOCAL_SHARED_LIBS += libai_VLPR
endif
ifneq ($(MPPCFG_MOD)_$(MPPCFG_VLPR),N_N)
    LOCAL_SHARED_LIBS += libCVEMiddleInterface
endif
ifeq ($(MPPCFG_ADAS_DETECT_V2), Y)
    LOCAL_SHARED_LIBS += libadas-v2
endif

LOCAL_STATIC_LIBS := \
    libcamera \
	librecorder

else
LOCAL_SHARED_LIBS := \
    libdl \
    libpthread \
    libasound \
    libglog \
    libz

LOCAL_STATIC_LIBS := \
    libcamera \
    librecorder \
    libcustomaw_media_utils \
    liblog \
    libsample_confparser \
    libaw_mpp \
    libcedarxdemuxer \
    libmuxers \
    libmp4_muxer \
    libraw_muxer \
    libmpeg2ts_muxer \
    libaac_muxer \
    libmp3_muxer \
    libffavutil \
    libFsWriter \
    libcedarxstream \
    libadecoder \
    libaac \
    libmp3 \
    libwav \
    libaw_g726dec \
    libaw_g711adec \
    libaw_g711udec \
    libcedarx_aencoder \
    libaacenc \
    libmp3enc \
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
    libcedarx_tencoder \
    libISP \
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
    libiniparser \
    libisp_ini \
    libisp_dev \
    libmedia_utils \
    libhwdisplay \
    libion \
    libcedarxrender \
    libcdx_base \
    libcdx_common

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

ifeq ($(MPPCFG_ISE_BI),Y)
    LOCAL_STATIC_LIBS += lib_ise_bi
endif
ifeq ($(MPPCFG_ISE_MO),Y)
    LOCAL_STATIC_LIBS += lib_ise_mo
endif
ifeq ($(MPPCFG_ISE_GDC),Y)
    LOCAL_STATIC_LIBS += lib_ise_gdc
endif

ifeq ($(MPPCFG_EIS),Y)
    LOCAL_STATIC_LIBS += \
        libEIS \
        lib_eis
endif
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
ifeq ($(MPPCFG_ADAS_DETECT_V2), Y)
    LOCAL_STATIC_LIBS += libadas-v2
endif
endif

#set dst file name: shared library, static library, execute bin.
LOCAL_TARGET_DYNAMIC :=
LOCAL_TARGET_STATIC :=
LOCAL_TARGET_BIN := sample_RecorderCallbackOut

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
    $(PACKAGE_TOP)/media/camera \
    $(PACKAGE_TOP)/utils \
    $(PACKAGE_TOP)/media/recorder \
    $(PACKAGE_TOP)/media/ise
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
all: $(LOCAL_TARGET_BIN)
	@echo ===================================
	@echo build eyesee-mpp-framework-demo-$(LOCAL_TARGET_BIN) done
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

$(LOCAL_TARGET_BIN): $(OBJS) $(DEPEND_LIBS)
	$(CXX) $(OBJS) $(LOCAL_BIN_LDFLAGS) -o $@
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
