TARGET_PATH :=$(call my-dir)
TOP_PATH := $(TARGET_PATH)

#########################################
include $(ENV_CLEAR)

include $(TARGET_TOP)/middleware/config/mpp_config.mk

TARGET_SRC := \
    TestUvoice.c \
    SaveFile.c

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
    $(TARGET_TOP)/external/uvoice
    # $(TARGET_TOP)/middleware/media/LIBRARY/libisp/isp_tuning \
    # $(TARGET_TOP)/middleware/media/LIBRARY/libcedarc/include \

ifeq ($(MPPCFG_COMPILE_DYNAMIC_LIB), Y)
TARGET_SHARED_LIB := \
    libpthread \
    liblog \
    libmedia_utils \
    libmedia_mpp \
    libmpp_component \
    libsample_confparser \

TARGET_STATIC_LIB :=

else ifeq ($(MPPCFG_COMPILE_STATIC_LIB), Y)
TARGET_SHARED_LIB := \
    libdl \
    libpthread \
    liblog \
    libsample_confparser \
    libcdx_base \
    libcdx_parser \
    libcdx_stream \
    libcdx_common \
    libasound

TARGET_STATIC_LIB := \
    libaw_mpp \
    libcedarxdemuxer \
    libmuxers \
    libmp4_muxer \
    libraw_muxer \
    libmpeg2ts_muxer \
    libaac_muxer \
    libmp3_muxer \
    libffavutil \
    libFsWriter \
    libcedarxstream \
    libadecoder \
    libcedarx_aencoder \
    libaacenc \
    libadpcmenc \
    libg711enc \
    libg726enc \
    libmp3enc \
    libvdecoder \
    libvideoengine \
    libawh264 \
    libawh265 \
    libawmjpegplus \
    libvencoder \
    libMemAdapter \
    libVE \
    libcdc_base \
    libISP \
    libisp_dev \
    libmedia_utils \
    libhwdisplay \
    libion \
    libcutils

ifeq ($(MPPCFG_USE_KFC),Y)
TARGET_STATIC_LIB += lib_hal
endif

ifeq ($(MPPCFG_ISE),Y)
TARGET_STATIC_LIB += \
    libise \
    lib_ise_mo
endif

endif

TARGET_SHARED_LIB += libuvoice_wakeup libremove_click
TARGET_STATIC_LIB += libuvoice libuv_se_bf

#TARGET_CPPFLAGS += -fPIC -Wall -Wno-unused-but-set-variable
TARGET_CFLAGS += -fPIC -Wall -Wno-unused-but-set-variable
#TARGET_LDFLAGS += -Wl,-rpath=$(TARGET_OUT)/target/lib

TARGET_MODULE := TestUvoice

include $(BUILD_BIN)
#########################################
