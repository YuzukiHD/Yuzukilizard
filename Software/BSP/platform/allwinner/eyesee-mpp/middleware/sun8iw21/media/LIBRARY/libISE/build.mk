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

#########################################
include $(ENV_CLEAR)
include $(TARGET_TOP)/middleware/config/mpp_config.mk



TARGET_PREBUILT_LIBS :=

ifeq ($(MPPCFG_ISE),Y)

TARGET_MODULE := lib_ise

ifeq ($(MPPCFG_COMPILE_DYNAMIC_LIB), Y)
ifeq ($(MPPCFG_ISE_MO),Y)
TARGET_PREBUILT_LIBS += \
	$(TARGET_PATH)/lib_ise_mo.so
endif
ifeq ($(MPPCFG_ISE_BI),Y)
TARGET_PREBUILT_LIBS += \
	$(TARGET_PATH)/lib_ise_bi.so
endif
ifeq ($(MPPCFG_ISE_BI_SOFT),Y)
TARGET_PREBUILT_LIBS += \
	$(TARGET_PATH)/lib_ise_bi_soft.so
endif
ifeq ($(MPPCFG_ISE_STI),Y)
TARGET_PREBUILT_LIBS += \
	$(TARGET_PATH)/lib_ise_sti.so
endif
endif

ifeq ($(MPPCFG_COMPILE_STATIC_LIB), Y)
ifeq ($(MPPCFG_ISE_MO),Y)
TARGET_PREBUILT_LIBS += \
	$(TARGET_PATH)/lib_ise_mo.a
endif
ifeq ($(MPPCFG_ISE_BI),Y)
TARGET_PREBUILT_LIBS += \
	$(TARGET_PATH)/lib_ise_bi.a
endif
ifeq ($(MPPCFG_ISE_STI),Y)
TARGET_PREBUILT_LIBS += \
	$(TARGET_PATH)/lib_ise_sti.a
endif
endif

include $(BUILD_MULTI_PREBUILT)

else
$(info ---------NO ISE LIB---------)

endif


