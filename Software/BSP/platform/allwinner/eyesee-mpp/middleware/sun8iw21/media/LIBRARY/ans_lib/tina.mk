# Makefile for eyesee-mpp/middleware/media/LIBRARY/AudioLib/lib
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

LOCAL_STATIC_LIBS :=

LOCAL_PREBUILD_LIBS_PATH :=
ifeq ($(MPPCFG_ANS_LIB), libwebrtc)
	ifeq ($(MPPCFG_TOOLCHAIN_LIBC), musl)
		LOCAL_PREBUILD_LIBS_PATH += $(CUR_PATH)/libwebrtc/musl
	else ifeq ($(MPPCFG_TOOLCHAIN_LIBC), glibc)
		LOCAL_PREBUILD_LIBS_PATH += $(CUR_PATH)/libwebrtc/glibc
	endif
else ifeq ($(MPPCFG_ANS_LIB), liblstm)
	ifeq ($(MPPCFG_TOOLCHAIN_LIBC), musl)
		LOCAL_PREBUILD_LIBS_PATH += $(CUR_PATH)/liblstm//musl
	else ifeq ($(MPPCFG_TOOLCHAIN_LIBC), glibc)
		LOCAL_PREBUILD_LIBS_PATH += $(CUR_PATH)/liblstm/glibc
	endif
endif

ANS_LIB_CFG_CHK = $(findstring $(1),$(MPPCFG_ANS_LIB))

$(eval $(if $(call ANS_LIB_CFG_CHK,libwebrtc),\
     ANS_LIB_SUB_DIR := libwebrtc,\
	 $(if $(call ANS_LIB_CFG_CHK,liblstm),\
	       ANS_LIB_SUB_DIR := liblstm,\
		   ANS_LIB_SUB_DIR :=)))


#set dst file name: shared library, static library, execute bin.
LOCAL_TARGET_DYNAMIC := $(basename $(notdir $(wildcard $(CUR_PATH)/$(LOCAL_PREBUILD_LIBS_PATH)/*.so)))
LOCAL_TARGET_STATIC := $(basename $(notdir $(wildcard $(CUR_PATH)/$(LOCAL_PREBUILD_LIBS_PATH)/*.a)))
LOCAL_TARGET_BIN :=


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

LOCAL_DYNAMIC_LDFLAGS := $(LOCAL_LDFLAGS) -shared \
    -L $(EYESEE_MPP_LIBDIR) \
    -Wl,-Bstatic \
    -Wl,--start-group $(foreach n, $(LOCAL_STATIC_LIBS), -l$(patsubst lib%,%,$(patsubst %.a,%,$(notdir $(n))))) -Wl,--end-group \
    -Wl,-Bdynamic \
    $(foreach y, $(LOCAL_SHARED_LIBS), -l$(patsubst lib%,%,$(patsubst %.so,%,$(notdir $(y)))))

#generate object files
OBJS := $(SRCCS:%=%.o) #OBJS=$(patsubst %,%.o,$(SRCCS))

#add dynamic lib name suffix and static lib name suffix.
target_dynamic := $(patsubst %,out/%.so,$(patsubst %.so,%,$(LOCAL_TARGET_DYNAMIC)))
target_static := $(patsubst %,out/%.a,$(patsubst %.a,%,$(LOCAL_TARGET_STATIC)))

LOCAL_DIRS := include out
#generate exe file.
.PHONY: all
all: $(target_dynamic) $(target_static)
	@echo ===================================
	@echo build eyesee-mpp-middleware-media-LIBRARY-AudioLib-lib done
	@echo ===================================
$(target_dynamic) $(target_static): out/%: $(LOCAL_PREBUILD_LIBS_PATH)/% | $(LOCAL_DIRS)
	cp -f $(ANS_LIB_SUB_DIR)/ans_lib.h include/
	cp -f $< $@
	@echo ----------------------------
	@echo "finish target: $@"
#	@echo "object files:  $+"
#	@echo "source files:  $(SRCCS)"
	@echo ----------------------------
$(LOCAL_DIRS):
	@echo 'prepare dirs:$@'
	mkdir -p $@
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
	-rm -rf $(CUR_PATH)/out
	-rm -rf $(CUR_PATH)/include

#add *.h prerequisites
-include $(OBJS:%=%.d)

