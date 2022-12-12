# Makefile for eyesee-mpp/middleware/media/LIBRARY/lib_aw_ai_algo
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
LOCAL_PREBUILD_LIBS :=
ifeq ($(MPPCFG_COMPILE_DYNAMIC_LIB), Y)
LOCAL_PREBUILD_LIBS += $(wildcard lib/lib_aw_ai_algo.so)
endif
ifeq ($(MPPCFG_COMPILE_STATIC_LIB), Y)
LOCAL_PREBUILD_LIBS += $(wildcard lib/lib_aw_ai_algo.a)
endif
#set dst file name: shared library, static library, execute bin.
LOCAL_TARGET_DYNAMIC :=
LOCAL_TARGET_STATIC :=
LOCAL_TARGET_BIN :=
LOCAL_TARGET_PREBUILD_LIBS := $(notdir $(LOCAL_PREBUILD_LIBS))

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
target_prebuild_libs := $(patsubst %,out/%,$(LOCAL_TARGET_PREBUILD_LIBS))

#generate exe file.
.PHONY: all
all: $(target_prebuild_libs)
	@echo ===================================
	@echo build eyesee-mpp-middleware-media-LIBRARY-lib_aw_ai_algo done
	@echo ===================================

$(target_prebuild_libs): out/%: lib/%
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
	-rm -f $(OBJS) $(target_prebuild_libs)
	-rm -rf $(CUR_PATH)/out
