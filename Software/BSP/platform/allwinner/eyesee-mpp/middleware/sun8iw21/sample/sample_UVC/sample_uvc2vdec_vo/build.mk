TRGET_PATH :=$(call my-dir)
TOP_PATH := $(TARGET_PATH)

#########################################
include $(ENV_CLEAR)

include $(TARGET_TOP)/middleware/config/mpp_config.mk
ifeq ($(MPPCFG_COMPILE_DYNAMIC_LIB), Y)

TARGET_SRC := \
    sample_uvc2vdec_vo.c

TARGET_INC := \
    $(TARGET_TOP)/system/public/include \
    $(TARGET_TOP)/middleware/include \
    $(TARGET_TOP)/middleware/include/utils \
    $(TARGET_TOP)/middleware/include/media \
    $(TARGET_TOP)/middleware/media/include \
    $(TARGET_TOP)/middleware/media/include/utils \
    $(TARGET_TOP)/middleware/media/include/component \
    $(TARGET_TOP)/middleware/media/LIBRARY/libisp/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/libisp/include/V4l2Camera \
    $(TARGET_TOP)/middleware/media/LIBRARY/libisp/isp_tuning \
    $(TARGET_TOP)/middleware/media/LIBRARY/libAIE_Vda/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/include_stream \
    $(TARGET_TOP)/middleware/media/LIBRARY/include_FsWriter \
    $(TARGET_TOP)/middleware/media/LIBRARY/libcedarc/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/libcedarx/libcore/common/iniparser \
    $(TARGET_TOP)/middleware/sample/configfileparser \
    # $(TARGET_TOP)/middleware/media/LIBRARY/libisp/isp_tuning \
    # $(TARGET_TOP)/middleware/media/LIBRARY/libcedarc/include \

TARGET_SHARED_LIB := \
    libpthread \
    liblog \
    libMemAdapter \
    libvdecoder \
    libmedia_utils \
    libmedia_mpp \
    libmpp_vi \
    libmpp_uvc \
    libmpp_isp \
    libmpp_vo \
    libmpp_component \
    libsample_confparser \
    libISP \

TARGET_STATIC_LIB :=

#TARGET_CPPFLAGS += -fPIC -Wall -Wno-unused-but-set-variable
TARGET_CFLAGS += -fPIC -Wall -Wno-unused-but-set-variable

TARGET_MODULE := sample_uvc2vdec_vo

include $(BUILD_BIN)

else
    $(warning "warning: dynamic compile, but MPPCFG_COMPILE_DYNAMIC_LIB = $(MPPCFG_COMPILE_DYNAMIC_LIB)")
endif

#########################################
