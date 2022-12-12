# Makefile for eyesee-mpp/middleware/media/LIBRARY/libaiMOD
CUR_PATH := .
PACKAGE_TOP := ../../..
EYESEE_MPP_INCLUDE:=$(STAGING_DIR)/usr/include/eyesee-mpp
EYESEE_MPP_LIBDIR:=$(STAGING_DIR)/usr/lib/eyesee-mpp
# STAGING_DIR is exported in rules.mk, so it can be used directly here.
# STAGING_DIR:=.../tina-v316/out/v316-perfnor/staging_dir/target

include $(PACKAGE_TOP)/config/mpp_config.mk

#set source files here.
SRCCS :=

#include directories
INCLUDE_DIRS :=

LOCAL_SHARED_LIBS :=

LOCAL_PREBUILD_DYNAMIC_LIBS := $(wildcard $(CUR_PATH)/*.so)

LOCAL_PREBUILD_LIBS_PATH :=
ifeq ($(MPPCFG_TOOLCHAIN_LIBC), musl)
	LOCAL_PREBUILR_LIBS_PATH += $(CUR_PATH)/library/musl
else ifeq ($(MPPCFG_TOOLCHAIN_LIBC), glibc)
	LOCAL_PREBUILD_LIBS_PATH += $(CUR_PATH)/library/glibc
endif

#set dst file name: shared library, static library, execute bin.
LOCAL_TARGET_DYNAMIC :=
LOCAL_TARGET_PREBUILD_STATICS :=
LOCAL_TARGET_PREBUILD_DYNAMICS :=
LOCAL_TARGET_BIN :=

ifeq ($(MPPCFG_MOTION_DETECT_SOFT),Y)
    LOCAL_TARGET_PREBUILD_DYNAMICS := $(basename $(notdir $(LOCAL_PREBUILD_DYNAMIC_LIBS)))
endif
#generate include directory flags for gcc.
inc_paths := $(foreach inc,$(filter-out -I%,$(INCLUDE_DIRS)),$(addprefix -I, $(inc))) \
                $(filter -I%, $(INCLUDE_DIRS))
#Extra flags to give to the C compiler
LOCAL_CFLAGS := $(CFLAGS) $(CEDARX_EXT_CFLAGS) $(inc_paths) -fPIC -Wall
#Extra flags to give to the C++ compiler
LOCAL_CXXFLAGS := $(CXXFLAGS) $(CEDARX_EXT_CFLAGS) $(inc_paths) -fPIC -Wall
#Extra flags to give to the C preprocessor and programs that use it (the C and Fortran compilers).
LOCAL_CPPFLAGS := $(CPPFLAGS)
#target device arch: x86, arm
LOCAL_TARGET_ARCH := $(ARCH)
#Extra flags to give to compilers when they are supposed to invoke the linker,‘ld’.
LOCAL_LDFLAGS := $(LDFLAGS)

#generate object files
OBJS := $(SRCCS:%=%.o) #OBJS=$(patsubst %,%.o,$(SRCCS))

#add dynamic lib name suffix and static lib name suffix.
target_dynamic := $(patsubst %,out/%.so,$(patsubst %.so,%,$(LOCAL_TARGET_PREBUILD_DYNAMICS)))
target_static := $(patsubst %,out/%.a,$(patsubst %.a,%,$(LOCAL_TARGET_PREBUILD_STATICS)))

#generate exe file.
.PHONY: all
all: $(target_dynamic) $(target_static)
	@echo ===================================
	@echo build eyesee-mpp-middleware-media-LIBRARY-libMODSoft done
	@echo ===================================

$(target_dynamic) $(target_static): out/%: $(LOCAL_PREBUILD_LIBS_PATH)%
	mkdir -p out
	cp -f $< $@
	@echo ----------------------------
	@echo "finish target: $@"
#	@echo "object files:  $+"
#	@echo "source files:  $(SRCCS)"
	@echo ----------------------------

#patten rules to generate local object files
%.cpp.o: %.cpp
	$(CXX) $(LOCAL_CXXFLAGS) $(LOCAL_CPPFLAGS) -c -o $@ $<
%.cc.o: %.cc
	$(CXX) $(LOCAL_CXXFLAGS) $(LOCAL_CPPFLAGS) -c -o $@ $<

%.c.o: %.c
	$(CC) $(LOCAL_CFLAGS) $(LOCAL_CPPFLAGS) -c -o $@ $<

# clean all
.PHONY: clean
clean:
	-rm -f $(OBJS) $(target_dynamic) $(target_static)
	-rm -rf $(CUR_PATH)/out
