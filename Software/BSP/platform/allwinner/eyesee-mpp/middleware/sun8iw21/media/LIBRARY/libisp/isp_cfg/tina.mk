# Makefile for eyesee-mpp/middleware/media/LIBRARY/libisp/isp_cfg
CUR_PATH := .
PACKAGE_TOP := $(PACKAGE_TOP)
EYESEE_MPP_INCLUDE:=$(STAGING_DIR)/usr/include/eyesee-mpp
EYESEE_MPP_LIBDIR:=$(STAGING_DIR)/usr/lib/eyesee-mpp
# STAGING_DIR is exported in rules.mk, so it can be used directly here.
# STAGING_DIR:=.../tina-v316/out/v316-perfnor/staging_dir/target

include $(PACKAGE_TOP)/config/mpp_config.mk

#set source files here.
SRCCS := \
    $(wildcard ./*.c)

#include directories
INCLUDE_DIRS := \
    $(CUR_PATH) \
    $(EYESEE_MPP_INCLUDE)/system/public/include \
    $(EYESEE_MPP_INCLUDE)/system/public/include/utils \
    $(CUR_PATH)/SENSOR_H \
    $(CUR_PATH)/../iniparser/src/ \
    $(CUR_PATH)/../include/ \
    $(CUR_PATH)/../isp_dev/

LOCAL_SHARED_LIBS :=

LOCAL_STATIC_LIBS :=

#set dst file name: shared library, static library, execute bin.
ifeq ($(MPPCFG_COMPILE_DYNAMIC_LIB), Y)
LOCAL_TARGET_DYNAMIC := libisp_ini
endif
LOCAL_TARGET_STATIC := libisp_ini
LOCAL_TARGET_BIN :=

#generate include directory flags for gcc.
inc_paths := $(foreach inc,$(filter-out -I%,$(INCLUDE_DIRS)),$(addprefix -I, $(inc))) \
                $(filter -I%, $(INCLUDE_DIRS))
#Extra flags to give to the C compiler
LOCAL_CFLAGS := $(CFLAGS) $(inc_paths) -fPIC -Wall
#Extra flags to give to the C++ compiler
LOCAL_CXXFLAGS := $(CXXFLAGS) $(inc_paths) -fPIC -Wall
ifeq ($(findstring imx386,$(SENSOR_NAME)), imx386)
    LOCAL_CFLAGS += -DSENSOR_IMX386=1
    LOCAL_CXXFLAGS += -DSENSOR_IMX386=1
endif
ifeq ($(findstring gc4663,$(SENSOR_NAME)), gc4663)
    LOCAL_CFLAGS += -DSENSOR_GC4663=1
    LOCAL_CXXFLAGS += -DSENSOR_GC4663=1
endif
ifeq ($(findstring gc2053,$(SENSOR_NAME)), gc2053)
    LOCAL_CFLAGS += -DSENSOR_GC2053=1
    LOCAL_CXXFLAGS += -DSENSOR_GC2053=1
endif
ifeq ($(findstring sc4336,$(SENSOR_NAME)), sc4336)
    LOCAL_CFLAGS += -DSENSOR_SC4336=1
    LOCAL_CXXFLAGS += -DSENSOR_SC4336=1
endif

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
	@echo build eyesee-mpp-middleware-media-LIBRARY-libisp-isp_cfg done
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
	$(CXX) $(LOCAL_CXXFLAGS) $(LOCAL_CPPFLAGS) -MD -MP -MF $(patsubst %,%.d,$@) -c -o $@ $<
%.cc.o: %.cc
	$(CXX) $(LOCAL_CXXFLAGS) $(LOCAL_CPPFLAGS) -MD -MP -MF $(patsubst %,%.d,$@) -c -o $@ $<

%.c.o: %.c
	$(CC) $(LOCAL_CFLAGS) $(LOCAL_CPPFLAGS) -MD -MP -MF $(patsubst %,%.d,$@) -c -o $@ $<

# clean all
.PHONY: clean
clean:
	-rm -f $(OBJS) $(OBJS:%=%.d) $(target_dynamic) $(target_static)

#add *.h prerequisites
-include $(OBJS:%=%.d)

