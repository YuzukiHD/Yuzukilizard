# Makefile for awTuningApp
CUR_PATH := .
#PACKAGE_TOP := $(PACKAGE_TOP)
PACKAGE_TOP := ../../../..
EYESEE_MPP_INCLUDE:=$(STAGING_DIR)/usr/include/eyesee-mpp
EYESEE_MPP_LIBDIR:=$(STAGING_DIR)/usr/lib/eyesee-mpp
USR_INCLUDE:=$(STAGING_DIR)/usr/include
include $(PACKAGE_TOP)/config/mpp_config.mk

#set source files here.
SRCCS := \
    awTuningApp.c \
    log_handle.c \
    socket_protocol.c \
	thread_pool.c \
    server/capture_image.c \
    server/isp_handle.c \
    server/mini_shell.c \
    server/server.c \
    server/server_api.c \
    server/server_core.c \
    server/register_opt.c \
    server/raw_flow_opt.c \
    ../isp.c \
    ../isp_events/events.c \
    ../isp_tuning/isp_tuning.c \
    ../isp_manage/isp_manage.c \
	isp_vencode/ispSimpleCode.c

#include directories
INCLUDE_DIRS := \
    $(CUR_PATH) \
	$(CUR_PATH)/server/ \
	$(CUR_PATH)/isp_vencode/include \
    $(EYESEE_MPP_INCLUDE)/system/public/include \
    $(EYESEE_MPP_INCLUDE)/system/public/include/utils \
    $(EYESEE_MPP_INCLUDE)/system/public/include/kernel-headers \
    $(PACKAGE_TOP)/include/utils \
    $(PACKAGE_TOP)/include/media \
    $(PACKAGE_TOP)/include \
    $(PACKAGE_TOP)/media/include/utils \
    $(PACKAGE_TOP)/media/include/component \
    $(PACKAGE_TOP)/media/LIBRARY/libisp/include \
	$(PACKAGE_TOP)/media/LIBRARY/libisp \
	$(PACKAGE_TOP)/media/LIBRARY/libisp/include/device \
    $(PACKAGE_TOP)/media/LIBRARY/libisp/include/V4l2Camera \
    $(PACKAGE_TOP)/media/LIBRARY/libisp/isp_tuning \
    $(PACKAGE_TOP)/media/LIBRARY/include_stream \
    $(PACKAGE_TOP)/media/LIBRARY/include_FsWriter \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarc/include

LOCAL_SHARED_LIBS :=
LOCAL_STATIC_LIBS :=

ifeq ($(MPPCFG_TOOLCHAIN_LIBC), glibc)
LOCAL_SHARED_LIBS += \
	libdl
endif

ifeq ($(MPPCFG_COMPILE_DYNAMIC_LIB), Y)
LOCAL_SHARED_LIBS += \
    liblog \
    libion \
    libISP \
    librt \
    libpthread \
    libvencoder \
    libvenc_codec \
    libvenc_base \
    libMemAdapter \
    libVE \
    libcdc_base \
    libcdx_base \
    libcdx_common \
    libmedia_utils

LOCAL_STATIC_LIBS += \
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

else
LOCAL_SHARED_LIBS += \
    librt \
    libpthread \
    libglog

LOCAL_STATIC_LIBS += \
    liblog \
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
    libion \
	libvencoder \
    libMemAdapter \
    libvenc_codec \
    libVE \
    libvenc_base \
    libcdc_base \
    libmedia_utils
endif

COMMON_CFLAGS += -DANDROID_VENCODE=1

#set dst file name: shared library, static library, execute bin.
LOCAL_TARGET_DYNAMIC :=
LOCAL_TARGET_STATIC :=
LOCAL_TARGET_BIN := awTuningApp_isp600

#generate include directory flags for gcc.
inc_paths := $(foreach inc,$(filter-out -I%,$(INCLUDE_DIRS)),$(addprefix -I, $(inc))) \
                $(filter -I%, $(INCLUDE_DIRS))
#Extra flags to give to the C compiler
LOCAL_CFLAGS := $(CFLAGS) $(CEDARX_EXT_CFLAGS) $(inc_paths) -fPIC -Wall -Wno-unused-but-set-variable $(COMMON_CFLAGS)
#Extra flags to give to the C++ compiler
LOCAL_CXXFLAGS := $(CXXFLAGS) $(CEDARX_EXT_CFLAGS) $(inc_paths) -fPIC -Wall -Wno-unused-but-set-variable $(COMMON_CFLAGS)
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
    $(PACKAGE_TOP)/media/LIBRARY/libisp \
    $(PACKAGE_TOP)/media/LIBRARY/libisp/out\
    $(PACKAGE_TOP)/media/LIBRARY/libisp/out/out\
    $(PACKAGE_TOP)/media/LIBRARY/libisp/isp_cfg \
    $(PACKAGE_TOP)/media/LIBRARY/libisp/isp_dev \
    $(PACKAGE_TOP)/media/LIBRARY/libisp/iniparser \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarc/vencoder \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarc/memory \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarc/library/out \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarc/vencoder/base \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarc/vencoder/libcodec \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarc/ve \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarc/base \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarx/libcore/base \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarx/libcore/common

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

