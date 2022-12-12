# Makefile for eyesee-mpp/middleware/media/LIBRARY/libVideoStabilization
CUR_PATH := .
PACKAGE_TOP := $(PACKAGE_TOP)
EYESEE_MPP_INCLUDE:=$(STAGING_DIR)/usr/include/eyesee-mpp
EYESEE_MPP_LIBDIR:=$(STAGING_DIR)/usr/lib/eyesee-mpp
# STAGING_DIR is exported in rules.mk, so it can be used directly here.
# STAGING_DIR:=.../tina-v316/out/v316-perfnor/staging_dir/target

include $(PACKAGE_TOP)/config/mpp_config.mk

#set source files here.
SRCCS := \
    utils/generic_buffer.c \
    utils/iio_utils.c \
    utils/ring_buffer.c

#include directories
INCLUDE_DIRS := \
    $(EYESEE_MPP_INCLUDE)/system/public/include \
    $(EYESEE_MPP_INCLUDE)/system/public/include/utils \
    $(PACKAGE_TOP)/include \
    $(PACKAGE_TOP)/include/utils \
    $(CUR_PATH)

LOCAL_SHARED_LIBS :=

LOCAL_STATIC_LIBS :=

LOCAL_PREBUILD_DYNAMIC_LIBS_PATH :=
ifeq ($(MPPCFG_TOOLCHAIN_LIBC), musl)
	LOCAL_PREBUILD_DYNAMIC_LIBS_PATH += $(CUR_PATH)/algo/library/musl
else ifeq ($(MPPCFG_TOOLCHAIN_LIBC), glibc)
	LOCAL_PREBUILD_DYNAMIC_LIBS_PATH += $(CUR_PATH)/algo/library/glibc
endif

LOCAL_PREBUILD_STATIC_LIBS_PATH :=
ifeq ($(MPPCFG_TOOLCHAIN_LIBC), musl)
	LOCAL_PREBUILD_STATIC_LIBS_PATH += $(CUR_PATH)/algo/library/musl
else ifeq ($(MPPCFG_TOOLCHAIN_LIBC), glibc)
	LOCAL_PREBUILD_STATIC_LIBS_PATH += $(CUR_PATH)/algo/library/glibc
endif

#set dst file name: shared library, static library, execute bin.
ifeq ($(MPPCFG_COMPILE_DYNAMIC_LIB), Y)
	LOCAL_TARGET_DYNAMIC := libEIS
	LOCAL_TARGET_PREBUILD_DYNAMICS := $(basename $(notdir $(wildcard $(LOCAL_PREBUILD_DYNAMIC_LIBS_PATH)/*.so)))
endif
ifeq ($(MPPCFG_COMPILE_STATIC_LIB), Y)
	LOCAL_TARGET_STATIC := libEIS
	LOCAL_TARGET_PREBUILD_STATICS := $(basename $(notdir $(wildcard $(LOCAL_PREBUILD_STATIC_LIBS_PATH)/*.a)))
endif
LOCAL_TARGET_BIN :=

#generate include directory flags for gcc.
inc_paths := $(foreach inc,$(filter-out -I%,$(INCLUDE_DIRS)),$(addprefix -I, $(inc))) \
                $(filter -I%, $(INCLUDE_DIRS))
#Extra flags to give to the C compiler
LOCAL_CFLAGS := $(CFLAGS) $(CEDARX_EXT_CFLAGS) $(inc_paths) -fPIC -Wall \
    -Wno-unused-function -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-label \
#Extra flags to give to the C++ compiler
LOCAL_CXXFLAGS := $(CXXFLAGS) $(CEDARX_EXT_CFLAGS) $(inc_paths) -fPIC -Wall \
    -Wno-unused-function -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-label \
#Extra flags to give to the C preprocessor and programs that use it (the C and Fortran compilers).
LOCAL_CPPFLAGS := $(CPPFLAGS)
#target device arch: x86, arm
LOCAL_TARGET_ARCH := $(ARCH)
#Extra flags to give to compilers when they are supposed to invoke the linker,‘ld’.
LOCAL_LDFLAGS := $(LDFLAGS)

LOCAL_DYNAMIC_LDFLAGS := $(LOCAL_LDFLAGS) -shared \
    -L $(EYESEE_MPP_LIBDIR) \
    -Wl,-Bstatic \
    -Wl,--start-group $(foreach n, $(LOCAL_STATIC_LIBS), -l$(patsubst lib%,%,$(patsubst %.a,%,$(notdir $(n))))) -Wl,--end-group \
    -Wl,-Bdynamic \
    $(foreach y, $(LOCAL_SHARED_LIBS), -l$(patsubst lib%,%,$(patsubst %.so,%,$(notdir $(y)))))

#generate object files
OBJS := $(SRCCS:%=%.o) #OBJS=$(patsubst %,%.o,$(SRCCS))

#add dynamic lib name suffix and static lib name suffix.
target_dynamic := $(if $(LOCAL_TARGET_DYNAMIC),$(addsuffix .so,$(LOCAL_TARGET_DYNAMIC)),)
target_prebuild_dynamics := $(patsubst %,out/%.so,$(patsubst %.so,%,$(LOCAL_TARGET_PREBUILD_DYNAMICS)))
target_static := $(if $(LOCAL_TARGET_STATIC),$(addsuffix .a,$(LOCAL_TARGET_STATIC)),)
target_prebuild_statics := $(patsubst %,out/%.a,$(patsubst %.a,%,$(LOCAL_TARGET_PREBUILD_STATICS)))

#generate exe file.
.PHONY: all
all: $(target_static) $(target_prebuild_statics) $(target_dynamic) $(target_prebuild_dynamics)
	@echo ===================================
	@echo build eyesee-mpp-middleware-media-LIBRARY-libVideoStabilization done
	@echo ===================================

$(target_static): $(OBJS)
	$(AR) -rcs -o $@ $+
	@echo ----------------------------
	@echo "finish target: $@"
#	@echo "object files:  $+"
#	@echo "source files:  $(SRCCS)"
	@echo ----------------------------

$(target_prebuild_statics): out/%: $(LOCAL_PREBUILD_STATIC_LIBS_PATH)/%
	mkdir -p out
	cp -f $< $@
	@echo ----------------------------
	@echo "finish target: $@"
#	@echo "object files:  $+"
#	@echo "source files:  $(SRCCS)"
	@echo ----------------------------

$(target_dynamic): $(OBJS)
	$(CXX) $+ $(LOCAL_DYNAMIC_LDFLAGS) -o $@
	@echo ----------------------------
	@echo "finish target: $@"
#	@echo "object files:  $+"
#	@echo "source files:  $(SRCCS)"
	@echo ----------------------------

$(target_prebuild_dynamics): out/%: $(LOCAL_PREBUILD_DYNAMIC_LIBS_PATH)/%
	mkdir -p out
	cp -f $< $@
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
	-rm -f $(OBJS) $(OBJS:%=%.d) $(target_static) $(target_dynamic)
	-rm -rf $(CUR_PATH)/out

#add *.h prerequisites
-include $(OBJS:%=%.d)

