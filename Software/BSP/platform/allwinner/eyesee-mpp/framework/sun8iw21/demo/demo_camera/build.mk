TARGET_PATH :=$(call my-dir)

#########################################
include $(ENV_CLEAR)

include $(TARGET_TOP)/middleware/config/mpp_config.mk

TARGET_SRC := \
            demo_camera.cpp

TARGET_INC := \
    $(TARGET_TOP)/custom_aw/include \
    $(TARGET_TOP)/custom_aw/include/media \
    $(TARGET_TOP)/custom_aw/include/media/camera \
    $(TARGET_TOP)/custom_aw/include/utils \
    $(TARGET_TOP)/middleware/media/vi_api \
    $(TARGET_TOP)/middleware/include \
    $(TARGET_TOP)/middleware/include/utils \
    $(TARGET_TOP)/middleware/include/media \
    $(TARGET_TOP)/middleware/media/LIBRARY/libisp/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/libisp/include/V4l2Camera \
    $(TARGET_TOP)/middleware/media/LIBRARY/libisp/isp_tuning \
    $(TARGET_TOP)/middleware/media/LIBRARY/include_ai_common \
    $(TARGET_TOP)/middleware/media/LIBRARY/libaiMOD/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/libcedarc/include \
    $(TARGET_TOP)/system/include/vo \
    $(TARGET_TOP)/framework/include/utils \
    $(TARGET_TOP)/framework/include \
    $(TARGET_TOP)/framework/include/media/camera \
    $(TARGET_TOP)/middleware/media/LIBRARY/libVLPR/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/libeveface/include \
    $(TARGET_TOP)/middleware/media/LIBRARY/include_eve_common \


TARGET_SHARED_LIB := \
    liblog \
    libcustomaw_media_utils \
    libhwdisplay \
    libmedia_mpp \
    libMemAdapter \
    libvencoder \
    libmedia_utils

ifeq ($(MPPCFG_MOD),Y)
TARGET_SHARED_LIB += libai_MOD
endif
ifeq ($(MPPCFG_EVEFACE),Y)
TARGET_SHARED_LIB += libeve_event
endif
ifeq ($(MPPCFG_VLPR),Y)
TARGET_SHARED_LIB += libai_VLPR
endif

ifneq ($(MPPCFG_MOD)_$(MPPCFG_VLPR),N_N)
TARGET_SHARED_LIB += libCVEMiddleInterface
endif

TARGET_STATIC_LIB := \
    libcamera \

#    libisp_dev \
#    libisp_base \
#    libisp_math \
#    libisp_ae \
#    libisp_af \
#    libisp_afs \
#    libisp_awb \
#    libisp_md \
#    libisp_iso \
#    libisp_gtm \
#    libisp_ini \
#    libiniparser 

TARGET_CPPFLAGS += $(CEDARX_EXT_CFLAGS)
TARGET_CFLAGS += $(CEDARX_EXT_CFLAGS)

TARGET_CPPFLAGS += -O2 -fPIC -Wall

TARGET_LDFLAGS += 
#    -lisp_dev -lisp_base -lisp_math -lisp_ae -lisp_af -lisp_afs \
#	-lisp_awb -lisp_md -lisp_iso -lisp_gtm -lisp_ini -liniparser \
#	-lpthread -lrt

TARGET_MODULE := camtest

include $(BUILD_BIN)
#########################################

