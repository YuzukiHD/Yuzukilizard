LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

CEDARX_ROOT=$(LOCAL_PATH)/../../../
include $(CEDARX_ROOT)/config.mk

ifeq ($(BOARD_USE_PLAYREADY), 1)

LOCAL_SRC_FILES = \
                $(notdir $(wildcard $(LOCAL_PATH)/*.c)) \
                ../asf/AsfParser.c \

LOCAL_C_INCLUDES:= \
	$(CEDARX_ROOT)/ \
    $(CEDARX_ROOT)/libcore \
    $(CEDARX_ROOT)/libcore/base/include \
    $(CEDARX_ROOT)/libcore/parser/include \
    $(CEDARX_ROOT)/libcore/parser/asf     \
    $(CEDARX_ROOT)/libcore/parser/env     \
    $(CEDARX_ROOT)/libcore/stream/include \
    $(CEDARX_ROOT)/external/include/adecoder \
    $(TOP)/frameworks/av/media/libcedarc/include \
    $(TOP)/frameworks/av/media/libcedarc/vdecoder/include \
    $(TOP)/frameworks/av/media/libcedarc/sdecoder/include


ifeq ($(PLAYREADY_DEBUG), 1)
PLAYREADY_DIR:= $(TOP)/vendor/playready
include $(PLAYREADY_DIR)/config.mk
LOCAL_CFLAGS += $(PLAYREADY_CFLAGS)
LOCAL_C_INCLUDES +=							\
	$(PLAYREADY_DIR)/source/inc                 \
	$(PLAYREADY_DIR)/source/oem/common/inc      \
	$(PLAYREADY_DIR)/source/oem/ansi/inc        \
	$(PLAYREADY_DIR)/source/results             \
	$(PLAYREADY_DIR)/source/tools/shared/common
else
include $(TOP)/hardware/aw/playready/config.mk
LOCAL_CFLAGS += $(PLAYREADY_CFLAGS)
LOCAL_C_INCLUDES +=							\
	$(TOP)/hardware/aw/playready/include/inc                 \
	$(TOP)/hardware/aw/playready/include/oem/common/inc      \
	$(TOP)/hardware/aw/playready/include/oem/ansi/inc        \
	$(TOP)/hardware/aw/playready/include/results             \
	$(TOP)/hardware/aw/playready/include/tools/shared/common
endif

LOCAL_CFLAGS += $(CDX_CFLAGS)

LOCAL_MODULE_TAGS := optional
LOCAL_PRELINK_MODULE := false

LOCAL_SHARED_LIBRARIES := \
        libcutils \
        libstagefright \
        libstagefright_foundation \
        libdrmframework \
        libdl \
        libutils \
        libplayreadypk \
        libcdx_common \
		libcdx_base

LOCAL_MODULE := libaw_env
ifeq ($(TARGET_ARCH),arm)
    LOCAL_CFLAGS += -Wno-psabi
endif
include $(BUILD_SHARED_LIBRARY)

else  ### BOARD_USE_PLAYREADY == 0

LOCAL_SRC_FILES = EnvParserItf.c
LOCAL_C_INCLUDES:= \
	$(CEDARX_ROOT)/ \
	$(CEDARX_ROOT)/libcore \
    $(CEDARX_ROOT)/libcore/base/include \
    $(CEDARX_ROOT)/libcore/parser/include \
    $(CEDARX_ROOT)/libcore/parser/asf     \
    $(CEDARX_ROOT)/libcore/stream/include \
    $(CEDARX_ROOT)/external/include/adecoder \
    $(TOP)/frameworks/av/media/libcedarc/include \
    $(TOP)/frameworks/av/media/libcedarc/vdecoder/include \
    $(TOP)/frameworks/av/media/libcedarc/sdecoder/include


LOCAL_MODULE := libaw_env
include $(BUILD_STATIC_LIBRARY)

endif
