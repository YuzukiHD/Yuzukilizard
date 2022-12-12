TARGET_PATH :=$(call my-dir)

#########################################
include $(ENV_CLEAR)

include $(TARGET_TOP)/middleware/config/mpp_config.mk

ifeq ($(MPPCFG_COMPILE_DYNAMIC_LIB), Y)

#TARGET_SRC := $(call all-c-files-under, $(TARGET_PATH))
#TARGET_SRC += $(call all-cpp-files-under, $(TARGET_PATH))
TARGET_SRC := \
            CallbackDispatcher.cpp \
            EyeseeMessageQueue.cpp \
            EyeseeQueue.cpp \
            MediaStructConvert.cpp \
            OSAL_Mutex.c \
            OSAL_Queue.c \
            Thread.cpp \
            CMediaMemory.cpp

TARGET_INC := \
            $(TARGET_TOP)/framework/include \
            $(TARGET_TOP)/framework/include/utils \
            $(TARGET_TOP)/framework/include/media/camera \
            $(TARGET_TOP)/middleware/include \
            $(TARGET_TOP)/middleware/include/utils \
            $(TARGET_TOP)/middleware/include/media \
            $(TARGET_TOP)/middleware/media/LIBRARY/libisp/include/V4l2Camera \
            $(TARGET_TOP)/middleware/media/LIBRARY/libcedarc/include

TARGET_CPPFLAGS += -fPIC -Wall
TARGET_CFLAGS += -fPIC -Wall
TARGET_LDFLAGS +=

TARGET_MODULE := libcustomaw_media_utils

include $(BUILD_SHARED_LIB)

endif


#########################################
ifeq ($(MPPCFG_COMPILE_STATIC_LIB), Y)

include $(ENV_CLEAR)

TARGET_SRC := \
            CallbackDispatcher.cpp \
            EyeseeMessageQueue.cpp \
            EyeseeQueue.cpp \
            MediaStructConvert.cpp \
            OSAL_Mutex.c \
            OSAL_Queue.c \
            Thread.cpp \
            CMediaMemory.cpp

TARGET_INC := \
            $(TARGET_TOP)/framework/include \
            $(TARGET_TOP)/framework/include/utils \
            $(TARGET_TOP)/framework/include/media/camera \
            $(TARGET_TOP)/middleware/include \
            $(TARGET_TOP)/middleware/include/utils \
            $(TARGET_TOP)/middleware/include/media \
            $(TARGET_TOP)/middleware/media/LIBRARY/libisp/include/V4l2Camera \
            $(TARGET_TOP)/middleware/media/LIBRARY/libcedarc/include

TARGET_CPPFLAGS += -fPIC -Wall
TARGET_CFLAGS += -fPIC -Wall
TARGET_LDFLAGS +=

TARGET_MODULE := libcustomaw_media_utils

include $(BUILD_STATIC_LIB)

endif
#########################################

