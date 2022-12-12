# Makefile for eyesee-mpp/middleware/media/LIBRARY/libcedarx/libcore/parser/base
CUR_PATH := .
PACKAGE_TOP := ../../../../../..
EYESEE_MPP_INCLUDE:=$(STAGING_DIR)/usr/include/eyesee-mpp
EYESEE_MPP_LIBDIR:=$(STAGING_DIR)/usr/lib/eyesee-mpp
# STAGING_DIR is exported in rules.mk, so it can be used directly here.
# STAGING_DIR:=.../tina-v316/out/v316-perfnor/staging_dir/target

include $(PACKAGE_TOP)/config/mpp_config.mk
CEDARX_ROOT=$(CUR_PATH)/../../..
include $(CEDARX_ROOT)/config.mk

#set source files here.
SRCCS := \
    $(notdir $(wildcard $(CUR_PATH)/*.c)) \
    ./id3base/StringContainer.c \
    ./id3base/Id3Base.c \
    ./id3base/CdxUtfCode.c \
    ./id3base/CdxMetaData.c

#include directories
INCLUDE_DIRS := \
    $(CEDARX_ROOT)/ \
    $(CEDARX_ROOT)/libcore \
    $(CEDARX_ROOT)/libcore/include \
    $(CEDARX_ROOT)/libcore/base/include \
    $(CEDARX_ROOT)/libcore/parser/include \
    $(CEDARX_ROOT)/libcore/stream/include \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarc/include \
    $(PACKAGE_TOP)/media/LIBRARY/AudioLib/osal \
    $(PACKAGE_TOP)/media/LIBRARY/AudioLib/midware/decoding/include \
    $(CUR_PATH)/id3base

LOCAL_SHARED_LIBS := \
    libz \
    libdl \
    libcdx_stream \
    libcdx_base

LOCAL_STATIC_LIBS := \
    libcdx_ts_parser \
    libcdx_mov_parser \
    libcdx_mpg_parser \
    libcdx_mp3_parser \
    libcdx_id3v2_parser \
    libcdx_aac_parser \
    libcdx_wav_parser
#   libcdx_remux_parser \
#   libcdx_asf_parser \
#   libcdx_avi_parser \
#   libcdx_flv_parser \
#   libcdx_mms_parser \
#   libcdx_dash_parser \
#   libcdx_hls_parser \
#   libcdx_mkv_parser \
#   libcdx_bd_parser \
#   libcdx_pmp_parser \
#   libcdx_ogg_parser \
#   libcdx_m3u9_parser \
#   libcdx_playlist_parser \
#   libcdx_ape_parser \
#   libcdx_flac_parser \
#   libcdx_amr_parser \
#   libcdx_atrac_parser \
#   libcdx_mmshttp_parser\
#   libcdx_awts_parser\
#   libcdx_sstr_parser \
#   libcdx_caf_parser \
#   libcdx_g729_parser \
#   libcdx_dsd_parser \
#   libcdx_aiff_parser \
#   libcdx_awrawstream_parser\
#   libcdx_awspecialstream_parser \
#   libcdx_pls_parser
#   libxml2

#set dst file name: shared library, static library, execute bin.
ifeq ($(MPPCFG_COMPILE_DYNAMIC_LIB), Y)
LOCAL_TARGET_DYNAMIC := libcdx_parser
endif
ifeq ($(MPPCFG_COMPILE_STATIC_LIB), Y)
LOCAL_TARGET_STATIC := libcdx_parser
endif
LOCAL_TARGET_BIN :=

#generate include directory flags for gcc.
inc_paths := $(foreach inc,$(filter-out -I%,$(INCLUDE_DIRS)),$(addprefix -I, $(inc))) \
                $(filter -I%, $(INCLUDE_DIRS))
#Extra flags to give to the C compiler
LOCAL_CFLAGS := $(CFLAGS) $(CEDARX_CFLAGS) $(inc_paths) -fPIC -Wall -Wno-psabi -D__OS_LINUX
#Extra flags to give to the C++ compiler
LOCAL_CXXFLAGS := $(CXXFLAGS) $(CEDARX_CFLAGS) $(inc_paths) -fPIC -Wall -Wno-psabi -D__OS_LINUX
#Extra flags to give to the C preprocessor and programs that use it (the C and Fortran compilers).
LOCAL_CPPFLAGS := $(CPPFLAGS)
#target device arch: x86, arm
LOCAL_TARGET_ARCH := $(ARCH)
#Extra flags to give to compilers when they are supposed to invoke the linker,‘ld’.
LOCAL_LDFLAGS := $(LDFLAGS)

LIB_SEARCH_PATHS := \
    $(EYESEE_MPP_LIBDIR) \
    $(CEDARX_ROOT)/libcore/base \
    $(CEDARX_ROOT)/libcore/stream/base \
    $(CEDARX_ROOT)/libcore/parser/aac \
    $(CEDARX_ROOT)/libcore/parser/id3v2 \
    $(CEDARX_ROOT)/libcore/parser/mp3 \
    $(CEDARX_ROOT)/libcore/parser/mov \
    $(CEDARX_ROOT)/libcore/parser/mpg \
    $(CEDARX_ROOT)/libcore/parser/ts \
    $(CEDARX_ROOT)/libcore/parser/wav

LOCAL_DYNAMIC_LDFLAGS := $(LOCAL_LDFLAGS) -shared \
    $(patsubst %,-L%,$(LIB_SEARCH_PATHS)) \
    -Wl,-Bstatic \
    -Wl,--start-group $(foreach n, $(LOCAL_STATIC_LIBS), -l$(patsubst lib%,%,$(patsubst %.a,%,$(notdir $(n))))) -Wl,--end-group \
    -Wl,-Bdynamic \
    $(foreach y, $(LOCAL_SHARED_LIBS), -l$(patsubst lib%,%,$(patsubst %.so,%,$(notdir $(y)))))

#generate object files
OBJS := $(SRCCS:%=%.o) #OBJS=$(patsubst %,%.o,$(SRCCS))
DEPEND_STATIC_LIBS := $(wildcard $(foreach p, $(patsubst %/,%,$(LIB_SEARCH_PATHS)), \
                            $(addprefix $(p)/, \
                              $(foreach n,$(LOCAL_STATIC_LIBS),$(patsubst %,%.a,$(patsubst %.a,%,$(notdir $(n))))) \
                            ) \
                          ) \
               )

#add dynamic lib name suffix and static lib name suffix.
target_dynamic := $(if $(LOCAL_TARGET_DYNAMIC),$(LOCAL_TARGET_DYNAMIC).so,)
target_static := $(if $(LOCAL_TARGET_STATIC),$(LOCAL_TARGET_STATIC).a,)

#generate exe file.
.PHONY: all
all: $(target_dynamic) $(target_static)
	@echo ===================================
	@echo build eyesee-mpp-middleware-media-LIBRARY-libcedarx-libcore-parser-base done
	@echo ===================================

$(target_dynamic): $(OBJS) $(DEPEND_STATIC_LIBS)
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
%.cpp.o: %.cpp
	$(CXX) $(LOCAL_CXXFLAGS) $(LOCAL_CPPFLAGS) -MD -MP -MF $(@:%=%.d) -c -o $@ $<
%.cc.o: %.cc
	$(CXX) $(LOCAL_CXXFLAGS) $(LOCAL_CPPFLAGS) -MD -MP -MF $(@:%=%.d) -c -o $@ $<

%.c.o: %.c
	$(CC) $(LOCAL_CFLAGS) $(LOCAL_CPPFLAGS) -MD -MP -MF $(@:%=%.d) -c -o $@ $<

# clean all
.PHONY: clean
clean:
	-rm -f $(OBJS) $(OBJS:%=%.d) $(target_dynamic) $(target_static)

#add *.h prerequisites
-include $(OBJS:%=%.d)

