# Makefile for eyesee-mpp/system/public/libion
CUR_PATH := .
EYESEE_MPP_INCLUDE:=$(STAGING_DIR)/usr/include/eyesee-mpp
EYESEE_MPP_LIBDIR:=$(STAGING_DIR)/usr/lib/eyesee-mpp

lib_version = "$(shell git --git-dir=$(CUR_PATH)/.git log -n 1 | grep commit | cut -d ' ' -f 2)"
build_time = "$(shell date)"
who = "$(shell whoami)"

#set source files here.
SRCCS := \
        src/core/XmlBuilder.cpp \
        src/core/tinystr.cpp \
        src/core/tinyxml.cpp \
        src/core/tinyxmlerror.cpp \
        src/core/tinyxmlparser.cpp \
        src/core/HttpCodec.cpp \
        src/core/SoapUtils.cpp \
        src/core/EventLoop.cpp \
        src/core/utils.cpp \
        src/service/DiscoveryService.cpp \
        src/service/OnvifConnector.cpp \
        src/service/SoapService.cpp \
        src/onvif_method/systemAndCodec.cpp \
        src/onvif_method/osdAndAnalytic.cpp \
        src/onvif_method/Packbits.cpp

#include directories
INCLUDE_DIRS := \
    $(EYESEE_MPP_INCLUDE)/system/public/include \
    $(CUR_PATH)/src/include

ifeq ($(MPPCFG_COMPILE_DYNAMIC_LIB), Y)

LOCAL_SHARED_LIBS := \
    libpthread \
    liblog

LOCAL_STATIC_LIBS :=

else

LOCAL_SHARED_LIBS :=

LOCAL_STATIC_LIBS := \
    liblog

endif

#set dst file name: shared library, static library, execute bin.
ifeq ($(MPPCFG_COMPILE_DYNAMIC_LIB), Y)
LOCAL_TARGET_DYNAMIC := libOnvif
endif
ifeq ($(MPPCFG_COMPILE_STATIC_LIB), Y)
LOCAL_TARGET_STATIC := libOnvif
endif
LOCAL_TARGET_BIN :=

COMPILE_OPTS =	-DTIXML_USE_STL \
                -DAW_ONVIF_LIB_VERSION=\"$(lib_version)\" \
                -DBUILD_BY_WHO=\"$(who)\" \
                -DBUILD_TIME=\"$(build_time)\"

#generate include directory flags for gcc.
inc_paths := $(foreach inc,$(filter-out -I%,$(INCLUDE_DIRS)),$(addprefix -I, $(inc))) \
                $(filter -I%, $(INCLUDE_DIRS))
#Extra flags to give to the C compiler
LOCAL_CFLAGS := $(CFLAGS) $(inc_paths) -fPIC -Wall -Wno-error=format-security -fpermissive $(COMPILE_OPTS)
#Extra flags to give to the C++ compiler
LOCAL_CXXFLAGS := $(CXXFLAGS) $(inc_paths) -fPIC -Wall -Wno-error=format-security  -fpermissive $(COMPILE_OPTS)
#Extra flags to give to the C preprocessor and programs that use it (the C and Fortran compilers).
LOCAL_CPPFLAGS := $(CPPFLAGS)
#target device arch: x86, arm
LOCAL_TARGET_ARCH := $(ARCH)
#Extra flags to give to compilers when they are supposed to invoke the linker,‘ld’.
LOCAL_LDFLAGS := $(LDFLAGS)

LOCAL_DYNAMIC_LDFLAGS := $(LOCAL_LDFLAGS) -shared \
    -L $(CUR_PATH)/ \
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
	@echo build eyesee-mpp-system-private-onvif done
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
	$(CXX) $(LOCAL_CXXFLAGS) $(LOCAL_CPPFLAGS) -c -o $@ $<
%.cc.o: %.cc
	$(CXX) $(LOCAL_CXXFLAGS) $(LOCAL_CPPFLAGS) -c -o $@ $<

%.c.o: %.c
	$(CC) $(LOCAL_CFLAGS) $(LOCAL_CPPFLAGS) -c -o $@ $<

# clean all
.PHONY: clean
clean:
	-rm -f $(OBJS) $(target_dynamic) $(target_static)
