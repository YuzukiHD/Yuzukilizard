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
TOP_PATH := $(TARGET_PATH)

# ============================================================================
# start gcc sample_fish.c
#########################################
include $(ENV_CLEAR)
TARGET_SRC := sample_fish.c
TARGET_INC := \
        $(TARGET_TOP)/middleware/media/include \
        $(TARGET_TOP)/middleware/media/LIBRARY/libcedarc/include \
        $(TARGET_TOP)/middleware/media/LIBRARY/libisp/include/V4l2Camera \
        $(TARGET_TOP)/middleware/media/LIBRARY/libisp/include/device \
        $(TARGET_TOP)/middleware/media/LIBRARY/libisp/isp_dev \
        $(TARGET_TOP)/middleware/include/utils \
        $(TARGET_TOP)/middleware/include/media/ \
        $(TARGET_TOP)/middleware/include \
        $(TARGET_TOP)/middleware/media/LIBRARY/libISE \
        $(TARGET_TOP)/system/include \
        $(TARGET_TOP)/system/public/include \
        $(TARGET_TOP)/system/public/include/vo \
        $(TARGET_TOP)/system/public/libion/include \
        $(TARGET_TOP)/system/public/libion/kernel-headers \
        $(TARGET_TOP)/middleware/include/utils \
        $(TARGET_TOP)/middleware/include/media \
        $(TARGET_TOP)/middleware/include \
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
    
TARGET_SHARED_LIB := \
        librt \
        libpthread \
        liblog \
        libion \
        libcdx_common \
        libmedia_utils \
        libmedia_mpp \
        libmpp_vi \
        libmpp_vo \
        libmpp_isp \
        libmpp_ise \
        libmpp_component \
        libcdc_base \
        libMemAdapter \
        libvideoengine \
        lib_ise_mo \
        libsample_confparser 

TARGET_STATIC_LIB := \



TARGET_CFLAGS := -O2 -fPIC -Wall
#TARGET_LDFLAGS += \
#	-L $(TARGET_PATH)/LIBRARY/libisp/out \
#	-L $(TARGET_PATH)/LIBRARY/libisp \
#	-lisp_dev -lisp_base -lisp_math -lisp_ae -lisp_af -lisp_afs \
#	-lisp_awb -lisp_md -lisp_iso -lisp_gtm -lisp_ini -liniparser \
#	-lpthread -lrt -static
	TARGET_MODULE := sample_fish
	object += $(object) $(TARGET_MODULE)
	include $(BUILD_BIN)

#########################################
