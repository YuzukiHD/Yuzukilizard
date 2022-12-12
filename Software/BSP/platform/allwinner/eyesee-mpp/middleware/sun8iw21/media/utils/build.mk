TARGET_PATH :=$(call my-dir)

#########################################
include $(ENV_CLEAR)

include $(TARGET_TOP)/middleware/config/mpp_config.mk

ifeq ($(MPPCFG_COMPILE_DYNAMIC_LIB), Y)

#TARGET_SRC := $(call all-c-files-under, $(TARGET_PATH))
#TARGET_SRC += $(call all-cpp-files-under, $(TARGET_PATH))
TARGET_SRC := \
            BITMAP_S.c \
            CedarXMetaData.cpp \
            cedarx_avs_counter.c \
            SystemBase.c \
            tmessage.c \
            tsemaphore.c \
            PIXEL_FORMAT_E_g2d_format_convert.c \
            EPIXELFORMAT_g2d_format_convert.c \
            VIDEO_FRAME_INFO_S.c \
            dup2SeldomUsedFd.c \
            media_common.c \
            mpi_videoformat_conversion.c \
            mm_comm_venc.c \
            video_buffer_manager.c

TARGET_INC := \
            $(TARGET_TOP)/system/public/include/kernel-headers \
            $(TARGET_TOP)/middleware/include \
            $(TARGET_TOP)/middleware/include/utils \
            $(TARGET_TOP)/middleware/include/media \
            $(TARGET_TOP)/middleware/include/media/utils \
            $(TARGET_TOP)/middleware/media/include \
            $(TARGET_TOP)/middleware/media/include/utils \
            $(TARGET_TOP)/middleware/media/include/component \
            $(TARGET_TOP)/middleware/media/LIBRARY/libcedarc/include \
            $(TARGET_TOP)/middleware/media/LIBRARY/AudioLib/osal \
            $(TARGET_TOP)/middleware/media/LIBRARY/AudioLib/midware/encoding/include \
            $(TARGET_TOP)/middleware/media/LIBRARY/AudioLib/midware/decoding/include \
            $(TARGET_TOP)/middleware/media/LIBRARY/libISE \
            $(TARGET_TOP)/middleware/media/LIBRARY/libisp/include/V4l2Camera \

TARGET_SHARED_LIB := \
    librt \

TARGET_STATIC_LIB :=

TARGET_CPPFLAGS += -fPIC -Wall -Wno-multichar -D__OS_LINUX
TARGET_CFLAGS += -fPIC -Wall -Wno-multichar -D__OS_LINUX
#TARGET_LDFLAGS += -lrt

TARGET_MODULE := libmedia_utils

include $(BUILD_SHARED_LIB)

endif
#########################################



#########################################
ifeq ($(MPPCFG_COMPILE_STATIC_LIB), Y)

include $(ENV_CLEAR)

#TARGET_SRC := $(call all-c-files-under, $(TARGET_PATH))
#TARGET_SRC += $(call all-cpp-files-under, $(TARGET_PATH))
TARGET_SRC := \
            BITMAP_S.c \
            CedarXMetaData.cpp \
            cedarx_avs_counter.c \
            SystemBase.c \
            tmessage.c \
            tsemaphore.c \
            PIXEL_FORMAT_E_g2d_format_convert.c \
            EPIXELFORMAT_g2d_format_convert.c \
            VIDEO_FRAME_INFO_S.c \
            dup2SeldomUsedFd.c \
            media_common.c \
            mpi_videoformat_conversion.c \
            mm_comm_venc.c \
            video_buffer_manager.c

TARGET_INC := \
            $(TARGET_TOP)/system/public/include/kernel-headers \
            $(TARGET_TOP)/middleware/include \
            $(TARGET_TOP)/middleware/include/utils \
            $(TARGET_TOP)/middleware/include/media \
            $(TARGET_TOP)/middleware/include/media/utils \
            $(TARGET_TOP)/middleware/media/include \
            $(TARGET_TOP)/middleware/media/include/utils \
            $(TARGET_TOP)/middleware/media/include/component \
            $(TARGET_TOP)/middleware/media/LIBRARY/libcedarc/include \
            $(TARGET_TOP)/middleware/media/LIBRARY/AudioLib/osal \
            $(TARGET_TOP)/middleware/media/LIBRARY/AudioLib/midware/encoding/include \
            $(TARGET_TOP)/middleware/media/LIBRARY/AudioLib/midware/decoding/include \
            $(TARGET_TOP)/middleware/media/LIBRARY/libISE \
            $(TARGET_TOP)/middleware/media/LIBRARY/libisp/include/V4l2Camera \

TARGET_SHARED_LIB :=
TARGET_STATIC_LIB :=

TARGET_CPPFLAGS += -fPIC -Wall -Wno-multichar -D__OS_LINUX
TARGET_CFLAGS += -fPIC -Wall -Wno-multichar -D__OS_LINUX
#TARGET_LDFLAGS += -lrt

TARGET_MODULE := libmedia_utils

include $(BUILD_STATIC_LIB)

endif
#########################################
