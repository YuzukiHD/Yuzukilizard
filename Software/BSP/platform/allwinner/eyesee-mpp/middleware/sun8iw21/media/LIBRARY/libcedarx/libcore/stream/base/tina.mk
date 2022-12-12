# Makefile for eyesee-mpp/middleware/media/LIBRARY/libcedarx/libcore/stream/base
CUR_PATH := .
PACKAGE_TOP := ../../../../../..
EYESEE_MPP_INCLUDE:=$(STAGING_DIR)/usr/include/eyesee-mpp
EYESEE_MPP_LIBDIR:=$(STAGING_DIR)/usr/lib/eyesee-mpp
# STAGING_DIR is exported in rules.mk, so it can be used directly here.
# STAGING_DIR:=.../tina-v316/out/v316-perfnor/staging_dir/target

include $(PACKAGE_TOP)/config/mpp_config.mk
LIB_ROOT:=$(CUR_PATH)/../..
include $(LIB_ROOT)/../config.mk
include $(LIB_ROOT)/stream/config.mk

#set source files here.
SRCCS := \
    $(notdir $(wildcard $(CUR_PATH)/*.c))

#include directories
INCLUDE_DIRS := \
    $(LIB_ROOT)/base/include \
    $(LIB_ROOT)/stream/include  \
    $(LIB_ROOT)/..

LOCAL_SHARED_LIBS := \
    libdl \
    libcdx_base

LOCAL_STATIC_LIBS := \
    libcdx_file_stream
#	libcdx_rtsp_stream \
#	libcdx_tcp_stream \
#	libcdx_http_stream \
#	libcdx_udp_stream \
#	libcdx_customer_stream \
#	libcdx_ssl_stream \
#	libcdx_aes_stream \
#	libcdx_bdmv_stream \
#	libcdx_widevine_stream \
#	libcdx_videoResize_stream \
#	libcdx_datasource_stream \
#	libcdx_dtmb_stream 	
#	libcdx_mms_stream \
#	libcdx_rtmp_stream \
#	libcdx_dtmb_stream \

#set dst file name: shared library, static library, execute bin.
ifeq ($(MPPCFG_COMPILE_DYNAMIC_LIB), Y)
LOCAL_TARGET_DYNAMIC := libcdx_stream
endif
ifeq ($(MPPCFG_COMPILE_STATIC_LIB), Y)
LOCAL_TARGET_STATIC := libcdx_stream
endif
LOCAL_TARGET_BIN :=

#generate include directory flags for gcc.
inc_paths := $(foreach inc,$(filter-out -I%,$(INCLUDE_DIRS)),$(addprefix -I, $(inc))) \
                $(filter -I%, $(INCLUDE_DIRS))
#Extra flags to give to the C compiler
LOCAL_CFLAGS := $(CFLAGS) $(CEDARX_CFLAGS) $(CDX_CFLAGS) $(inc_paths) -fPIC -Wall
#Extra flags to give to the C++ compiler
LOCAL_CXXFLAGS := $(CXXFLAGS) $(CEDARX_CFLAGS) $(CDX_CFLAGS) $(inc_paths) -fPIC -Wall
#Extra flags to give to the C preprocessor and programs that use it (the C and Fortran compilers).
LOCAL_CPPFLAGS := $(CPPFLAGS)
#target device arch: x86, arm
LOCAL_TARGET_ARCH := $(ARCH)
#Extra flags to give to compilers when they are supposed to invoke the linker,‘ld’.
LOCAL_LDFLAGS := $(LDFLAGS)

LIB_SEARCH_PATHS := \
    $(EYESEE_MPP_LIBDIR) \
    $(LIB_ROOT)/base \
    $(LIB_ROOT)/stream/file

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
	@echo build eyesee-mpp-middleware-media-LIBRARY-libcedarx-libcore-stream-base done
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

