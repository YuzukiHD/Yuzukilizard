# Makefile for eyesee-mpp/framework/demo/sample_Adas
CUR_PATH := .
PACKAGE_TOP := ../..
PRODUCT_DIR := V5CDRTP
EYESEE_MPP_INCLUDE:=$(STAGING_DIR)/usr/include/eyesee-mpp
EYESEE_MPP_LIBDIR:=$(STAGING_DIR)/usr/lib/eyesee-mpp
EYESEE_MINIGUI_INCLUDE:=$(STAGING_DIR)/usr/include/eyesee-minigui

-include $(EYESEE_MPP_INCLUDE)/middleware/config/mpp_config.mk

#	minigui/sample_adas_ui.cpp \
#	fbtools/fbtools.cpp \
#	audioplayer/audioCtrl.cpp \
#	audioplayer/wav_player.cpp



#set source files here.
SRCCS := \
    sample_adas.cpp \
	minigui/sample_adas_ui.cpp \
	fbtools/fbtools.cpp \
	audioplayer/audioCtrl.cpp \
	audioplayer/wav_player.cpp
#include directories
INCLUDE_DIRS := \
    $(CUR_PATH) \
    $(CUR_PATH)/minigui \
	$(CUR_PATH)/fbtools \
	$(CUR_PATH)/audioplayer \
    $(PACKAGE_TOP)/include \
    $(PACKAGE_TOP)/include/media \
    $(PACKAGE_TOP)/include/media/camera \
    $(PACKAGE_TOP)/include/media/ise \
    $(PACKAGE_TOP)/include/media/player \
    $(PACKAGE_TOP)/include/utils \
	$(PACKAGE_TOP)/../awcdr/include/$(PRODUCT_DIR) \
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
	$(EYESEE_MPP_INCLUDE)/middleware/media/LIBRARY/libADAS/include \
    $(EYESEE_MPP_INCLUDE)/middleware/media/LIBRARY/libVLPR/include \
	$(EYESEE_MPP_INCLUDE)/middleware/media/LIBRARY/include_eve_common \
    $(EYESEE_MPP_INCLUDE)/middleware/media/LIBRARY/libeveface/include \
    $(EYESEE_MPP_INCLUDE)/middleware/media/LIBRARY/include_stream \
    $(EYESEE_MPP_INCLUDE)/middleware/media/LIBRARY/include_FsWriter \
    $(EYESEE_MPP_INCLUDE)/middleware/media/LIBRARY/libcedarc/include \
    $(EYESEE_MPP_INCLUDE)/middleware/media/LIBRARY/libcedarx/libcore/common/iniparser \
    $(EYESEE_MPP_INCLUDE)/middleware/sample/configfileparser \
	$(EYESEE_MPP_INCLUDE)/middleware/media/LIBRARY/include_muxer \
	$(EYESEE_MPP_INCLUDE)/middleware/media/include/audio \
    $(EYESEE_MPP_INCLUDE)/system/public/include \
    $(EYESEE_MPP_INCLUDE)/system/public/include/utils \
    $(EYESEE_MPP_INCLUDE)/system/public/include/vo \
	$(EYESEE_MPP_INCLUDE)/system/public/libion/include \
	$(EYESEE_MINIGUI_INCLUDE) \
	$(LINUX_USER_HEADERS)/include
	
LOCAL_SHARED_LIBS := \
    libpthread \
    libdl \
    librt \
    libcdx_base \
    libcdx_parser \
    libcdx_stream \
    libcdx_common \
    libasound \
    liblog \
    libsample_confparser \
	libADAS
LOCAL_SHARED_LIBS += \
    libminigui_ths \
	libpng \
    libjpeg \
	libts \
    libz 
	
LOCAL_STATIC_LIBS := \
	libcamera \
    librecorder \
    libcustomaw_media_utils \
    libaw_mpp \
    libmedia_utils \
    libcedarx_aencoder \
    libcedarx_tencoder \
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
    libMemAdapter \
    libVE \
    libcdc_base \
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
    libmuxers \
    libmp4_muxer \
    libraw_muxer \
    libmpeg2ts_muxer \
    libaac_muxer \
    libmp3_muxer \
    libffavutil \
    libFsWriter \
    libcedarxstream \
    libion \
    libhwdisplay \
    libwifi_sta \
    libwifi_ap \
    libwpa_ctl \
    librgb_ctrl \
    liblz4 \
    libluaconfig \
    liblua \
    libSQLiteCpp \
    libsqlite3 \
    libcutils \
    libvencoder \
    libvenc_codec \
    libvenc_base  \
    libAec 
ifeq ($(MPPCFG_ISE),Y)
    LOCAL_STATIC_LIBS += \
        libise \
        lib_ise_mo
endif
ifeq ($(MPPCFG_EIS),Y)
    LOCAL_STATIC_LIBS += \
        libEIS \
        lib_eis
endif
ifeq ($(MPPCFG_USE_KFC),Y)
    LOCAL_STATIC_LIBS += lib_hal
endif

#set dst file name: shared library, static library, execute bin.
LOCAL_TARGET_DYNAMIC :=
LOCAL_TARGET_STATIC :=
LOCAL_TARGET_BIN := sample_adas

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
	$(PACKAGE_TOP)/media/recorder \
    $(PACKAGE_TOP)/utils \
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

#add dynamic lib name suffix and static lib name suffix.
target_dynamic := $(if $(LOCAL_TARGET_DYNAMIC),$(addsuffix .so,$(LOCAL_TARGET_DYNAMIC)),)
target_static := $(if $(LOCAL_TARGET_STATIC),$(addsuffix .a,$(LOCAL_TARGET_STATIC)),)

#generate exe file.
.PHONY: all
all: $(LOCAL_TARGET_BIN)
	@echo ===================================
	@echo build eyesee-mpp-framework-demo-sample_ADAS done
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

