# Makefile for eyesee-mpp/middleware/media/LIBRARY/libdemux
CUR_PATH := .
PACKAGE_TOP := ../../..
EYESEE_MPP_INCLUDE:=$(STAGING_DIR)/usr/include/eyesee-mpp
EYESEE_MPP_LIBDIR:=$(STAGING_DIR)/usr/lib/eyesee-mpp
# STAGING_DIR is exported in rules.mk, so it can be used directly here.
# STAGING_DIR:=.../tina-v316/out/v316-perfnor/staging_dir/target

include $(PACKAGE_TOP)/config/mpp_config.mk
include $(PACKAGE_TOP)/media/LIBRARY/libcedarx/config.mk
#set source files here.
SRCCS := \
    cedarx_demux.c \
    aw_demux.cpp \
    DemuxTypeMap.c

#include directories
INCLUDE_DIRS := \
	$(EYESEE_MPP_INCLUDE)/system/public/include \
	$(EYESEE_MPP_INCLUDE)/system/public/include/utils \
    $(PACKAGE_TOP)/include \
    $(PACKAGE_TOP)/include/media \
    $(PACKAGE_TOP)/include/utils \
    $(PACKAGE_TOP)/media/include \
    $(PACKAGE_TOP)/media/include/component \
    $(PACKAGE_TOP)/media/LIBRARY/libdemux \
    $(PACKAGE_TOP)/media/LIBRARY/include_stream \
    $(PACKAGE_TOP)/media/LIBRARY/include_demux \
    $(PACKAGE_TOP)/media/LIBRARY/include_utils \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarx \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarx/libcore/include \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarx/libcore/parser/include \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarx/libcore/stream/include \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarx/libcore/base/include \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarc/include \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarc/base/include \
    $(PACKAGE_TOP)/media/LIBRARY/AudioLib/osal \
    $(PACKAGE_TOP)/media/LIBRARY/AudioLib/midware/decoding/include

LOCAL_SHARED_LIBS :=

LOCAL_STATIC_LIBS :=

#set dst file name: shared library, static library, execute bin.
LOCAL_TARGET_DYNAMIC :=
LOCAL_TARGET_STATIC := libcedarxdemuxer
LOCAL_TARGET_BIN :=

#generate include directory flags for gcc.
inc_paths := $(foreach inc,$(filter-out -I%,$(INCLUDE_DIRS)),$(addprefix -I, $(inc))) \
                $(filter -I%, $(INCLUDE_DIRS))
#Extra flags to give to the C compiler
LOCAL_CFLAGS := $(CFLAGS) $(CEDARX_CFLAGS) $(CEDARX_EXT_CFLAGS) $(inc_paths) -fPIC -Wall -Wno-unused-but-set-variable -D__OS_LINUX
#Extra flags to give to the C++ compiler
LOCAL_CXXFLAGS := $(CXXFLAGS) $(CEDARX_CFLAGS) $(CEDARX_EXT_CFLAGS) $(inc_paths) -fPIC -Wall -Wno-unused-but-set-variable -D__OS_LINUX
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
target_dynamic := $(if $(LOCAL_TARGET_DYNAMIC),$(LOCAL_TARGET_DYNAMIC).so,)
target_static := $(if $(LOCAL_TARGET_STATIC),$(LOCAL_TARGET_STATIC).a,)

#generate exe file.
.PHONY: all
all: $(target_dynamic) $(target_static)
	@echo ===================================
	@echo build eyesee-mpp-middleware-media-LIBRARY-libdemux done
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

