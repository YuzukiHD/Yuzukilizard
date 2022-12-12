LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

include $(ALIB_PATH)/config.mk

################################################################################
## build configures print.
################################################################################

$(warning "#################################################")
$(warning "#   Allwinner audio codec compile information    ")
$(warning "#                                                ")
$(warning "# MID_LAYER_DEBUG_CONFIG is $(MID_LAYER_DEBUG_CONFIG)")
$(warning "# MID_LAYER_CHECK_MEMORY_CONFIG is $(MID_LAYER_CHECK_MEMORY_CONFIG)")
$(warning "#                                                ")
$(warning "#################################################")

################################################################################
## set src files and inc path for compile
################################################################################

LOCAL_SRC_FILES := src/AudioDec_Decode.c \
				src/cedar_abs_packet_hdr.c \
				adecoder.c

LOCAL_C_INCLUDES := \
				$(LOCAL_PATH)/include \
				$(ALIB_PATH)osal

################################################################################
## set flags for compile and link
################################################################################
CONFIG_FOR_COMPILE = -D__OS_ANDROID -fPIC
CONFIG_FOR_LINK =
 
ifeq ($(MID_LAYER_DEBUG_CONFIG), true)
CONFIG_FOR_COMPILE += -D__AD_CEDAR_DBG_WRITEOUT_BITSTREAM
CONFIG_FOR_COMPILE += -D__AD_CEDAR_DBG_WRITEOUT_PCM_BUFFER
endif

ifeq ($(MID_LAYER_CHECK_MEMORY_CONFIG), true)
	CONFIG_FOR_COMPILE += -DMEMCHECK
	LOCAL_SRC_FILES  += tools/memcheck/memcheck.c
	LOCAL_C_INCLUDES += $(LOCAL_PATH)/tools/memcheck
endif

LOCAL_CFLAGS += $(CONFIG_FOR_COMPILE)

LOCAL_LDFLAGS += $(CONFIG_FOR_LINK)

LOCAL_MODULE_TAGS := optional

LOCAL_ARM_MODE := arm

LOCAL_PRELINK_MODULE := false

LOCAL_SHARED_LIBRARIES += \
        libdl \
        liblog \
        libcutils

LOCAL_MODULE := libadecoder

include $(BUILD_SHARED_LIBRARY)
