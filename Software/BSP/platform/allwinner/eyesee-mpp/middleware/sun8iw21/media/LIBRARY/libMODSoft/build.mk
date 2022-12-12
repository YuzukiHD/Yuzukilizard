#
# 1. Set the path and clear environment
# 	TARGET_PATH := $(call my-dir)
# 	include $(ENV_CLEAR)
#
# 2. Set the source files and headers files
#	TARGET_SRC := xxx_1.c xxx_2.c
#	TARGET_INc := xxx_1.h xxx_2.h
#
# 3. Set the output target
#	TARGET_MODULE := xxx
#
# 4. Include the main makefile
#	include $(BUILD_BIN)
#
# Before include the build makefile, you can set the compilaion
# flags, e.g. TARGET_ASFLAGS TARGET_CFLAGS TARGET_CPPFLAGS
#

TARGET_PATH :=$(call my-dir)
include $(ENV_CLEAR)

include $(TARGET_TOP)/middleware/config/mpp_config.mk

ifeq ($(MPPCFG_MOD),Y)

#TARGET_MODULE :=
#TARGET_PREBUILT_LIBS := $(wildcard $(TARGET_PATH)/lib/*.so)
#TARGET_CPPFLAGS += $(CEDARX_EXT_CFLAGS)
#TARGET_CFLAGS += $(CEDARX_EXT_CFLAGS)
#include $(BUILD_MULTI_PREBUILT)


#include $(ENV_CLEAR)
TARGET_CPPFLAGS += -fPIC $(CEDARX_EXT_CFLAGS)
TARGET_CFLAGS += -fPIC $(CEDARX_EXT_CFLAGS)
TARGET_LDFLAGS += -L$(TARGET_PATH)/lib \
	-Wl,--whole-archive \
	-law_cve_mod \
	-lclbr_cve \
	-ldtca_cve \
	-ldtcas_cve \
	-lzcvcore_cve \
	-lzcvxml_cve \
	-lzvaacve_cve \
	-lzvabgfg_cve \
	-lzvabgfgs_cve \
	-lzvaclbr_cve \
	-lzvacore_cve \
	-lzvadtca_cve \
	-lzvadtcas_cve \
	-Wl,--no-whole-archive
TARGET_MODULE := libai_MOD
include $(BUILD_SHARED_LIB)


$(info ---------copy MOD param file---------)
$(call copy-files-under, \
	$(TARGET_PATH)/ParamFile/*.xml, \
	$(TARGET_OUT)/target/etc/ai \
)

endif

ifeq ($(MPPCFG_COMPILE_STATIC_LIB), Y)
TARGET_PREBUILT_LIBS := $(TARGET_PATH)/lib/*.a
include $(BUILD_MULTI_PREBUILT)
endif
