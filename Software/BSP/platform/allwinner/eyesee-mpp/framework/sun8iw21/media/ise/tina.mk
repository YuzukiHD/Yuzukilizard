# Makefile for eyesee-mpp/framework/media/ise
CUR_PATH := .
PACKAGE_TOP := ../..
EYESEE_MPP_INCLUDE:=$(STAGING_DIR)/usr/include/eyesee-mpp
EYESEE_MPP_LIBDIR:=$(STAGING_DIR)/usr/lib/eyesee-mpp
# STAGING_DIR is exported in rules.mk, so it can be used directly here.
# STAGING_DIR:=.../tina-v316/out/v316-perfnor/staging_dir/target

-include $(EYESEE_MPP_INCLUDE)/middleware/config/mpp_config.mk

#set source files here.
SRCCS := \
    EyeseeISE.cpp \
    ISEChannel.cpp \
    ISEDevice.cpp

#include directories
INCLUDE_DIRS := \
    $(CUR_PATH) \
    $(PACKAGE_TOP)/include \
    $(PACKAGE_TOP)/include/media \
    $(PACKAGE_TOP)/include/media/camera \
    $(PACKAGE_TOP)/include/media/ise \
    $(PACKAGE_TOP)/include/utils \
    $(EYESEE_MPP_INCLUDE)/middleware/config \
    $(EYESEE_MPP_INCLUDE)/middleware/include \
    $(EYESEE_MPP_INCLUDE)/middleware/include/utils \
    $(EYESEE_MPP_INCLUDE)/middleware/include/media \
    $(EYESEE_MPP_INCLUDE)/middleware/media/include/utils \
    $(EYESEE_MPP_INCLUDE)/middleware/media/LIBRARY/libisp/include \
    $(EYESEE_MPP_INCLUDE)/middleware/media/LIBRARY/libisp/include/V4l2Camera \
    $(EYESEE_MPP_INCLUDE)/middleware/media/LIBRARY/libisp/isp_tuning \
    $(EYESEE_MPP_INCLUDE)/middleware/media/LIBRARY/include_ai_common \
    $(EYESEE_MPP_INCLUDE)/middleware/media/LIBRARY/include_eve_common \
    $(EYESEE_MPP_INCLUDE)/middleware/media/LIBRARY/libaiMOD/include \
    $(EYESEE_MPP_INCLUDE)/middleware/media/LIBRARY/libeveface/include \
    $(EYESEE_MPP_INCLUDE)/middleware/media/LIBRARY/libVLPR/include \
    $(EYESEE_MPP_INCLUDE)/middleware/media/LIBRARY/libcedarc/include \
    $(EYESEE_MPP_INCLUDE)/middleware/media/LIBRARY/libISE/include \
	$(EYESEE_MPP_INCLUDE)/middleware/media/LIBRARY/libADAS/include \
    $(EYESEE_MPP_INCLUDE)/system/public/include \
    $(EYESEE_MPP_INCLUDE)/system/public/include/utils \
    $(EYESEE_MPP_INCLUDE)/system/public/include/vo \
    $(EYESEE_MPP_INCLUDE)/system/public/libion/include \
    $(LINUX_USER_HEADERS)/include

LOCAL_SHARED_LIBS :=

LOCAL_STATIC_LIBS :=

#set dst file name: shared library, static library, execute bin.
LOCAL_TARGET_DYNAMIC := 
LOCAL_TARGET_STATIC := libise
LOCAL_TARGET_BIN :=

#generate include directory flags for gcc.
inc_paths := $(foreach inc,$(filter-out -I%,$(INCLUDE_DIRS)),$(addprefix -I, $(inc))) \
                $(filter -I%, $(INCLUDE_DIRS))
#Extra flags to give to the C compiler
LOCAL_CFLAGS := $(CFLAGS) $(CEDARX_EXT_CFLAGS) $(inc_paths) -fPIC -Wall -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-label
#Extra flags to give to the C++ compiler
LOCAL_CXXFLAGS := $(CXXFLAGS) $(CEDARX_EXT_CFLAGS) $(inc_paths) -fPIC -Wall -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-label
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
target_static := $(if $(LOCAL_TARGET_STATIC),$(addsuffix .a,$(LOCAL_TARGET_STATIC)),)

#generate exe file.
.PHONY: all
all: $(target_dynamic) $(target_static)
	@echo ===================================
	@echo build eyesee-mpp-framework-media-ise done
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
$(filter %.cpp.o %.cc.o, $(OBJS)): %.o: %
	$(CXX) $(LOCAL_CXXFLAGS) $(LOCAL_CPPFLAGS) -MD -MP -MF $(@:%=%.d) -c -o $@ $<
$(filter %.c.o, $(OBJS)): %.o: %
	$(CC) $(LOCAL_CFLAGS) $(LOCAL_CPPFLAGS) -MD -MP -MF $(@:%=%.d) -c -o $@ $<

# clean all
.PHONY: clean
clean:
	-rm -f $(OBJS) $(OBJS:%=%.d) $(target_dynamic) $(target_static)

#add *.h prerequisites
-include $(OBJS:%=%.d)

