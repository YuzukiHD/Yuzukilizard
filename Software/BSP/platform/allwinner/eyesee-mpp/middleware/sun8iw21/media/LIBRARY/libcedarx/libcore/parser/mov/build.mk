TARGET_PATH:= $(call my-dir)

include $(ENV_CLEAR)

CEDARX_ROOT=$(TARGET_PATH)/../../../
include $(CEDARX_ROOT)/config.mk

TARGET_SRC = \
                $(notdir $(wildcard $(TARGET_PATH)/*.c))

TARGET_INC:= \
    $(CEDARX_ROOT)/ \
    $(CEDARX_ROOT)/libcore \
    $(CEDARX_ROOT)/libcore/include \
    $(CEDARX_ROOT)/libcore/base/include \
    $(CEDARX_ROOT)/libcore/parser/include \
	$(CEDARX_ROOT)/libcore/parser/base/id3base \
    $(CEDARX_ROOT)/libcore/stream/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/libcedarc/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/libcedarc/vdecoder/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/libcedarc/sdecoder/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/AudioLib/osal \
    $(TARGET_TOP)/middleware/media/LIBRARY/AudioLib/midware/decoding/include \
    $(CEDARX_ROOT)/libcore/parser/base/id3base
    
#$(TARGET_TOP)/external/zlib \


TARGET_CFLAGS += -fPIC -Wall -Wno-psabi -D__OS_LINUX

TARGET_SHARED_LIB += libz

TARGET_MODULE := libcdx_mov_parser


include $(BUILD_STATIC_LIB)


