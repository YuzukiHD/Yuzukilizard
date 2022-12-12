TARGET_PATH :=$(call my-dir)
TOP_PATH := $(TARGET_PATH)

LOCAL_SRC := httpd.cpp

LOCAL_INC := \
    $(TARGET_PATH)/

$(shell cp $(TARGET_PATH)/httpd.h $(TARGET_TOP)/system/public/include/tinyhttp/)

#########################################
include $(ENV_CLEAR)

TARGET_SRC := $(LOCAL_SRC)
TARGET_INC := $(LOCAL_INC)

TARGET_CFLAGS += -O2 -g -W -Wall -fPIC -Wno-unused-parameter
TARGET_CPPFLAGS += -DSOCKLEN_T=socklen_t  \
                   -D_LARGEFILE_SOURCE=1 -D_FILE_OFFSET_BITS=64 \
                   -DSO_REUSEPORT -DALLOW_SERVER_PORT_REUSE \
                   -DAW_RTSP_LIB_VERSION=\"$(lib_version)\" \
                   -DBUILD_BY_WHO=\"$(who)\" \
                   -DBUILD_TIME=\"$(build_time)\" \
                   -DUSE_SIGNALS -DREUSE_FOR_TCP \
                   -Wall -DBSD=1 -O2 -g -W -Wall -fexceptions -fPIC -Wno-unused-parameter -Wno-unused-but-set-parameter

TARGET_LDFLAGS += -llog

TARGET_MODULE := libTinyHttpServer
include $(BUILD_STATIC_LIB)

#########################################
include $(ENV_CLEAR)

TARGET_SRC := $(LOCAL_SRC)
TARGET_INC := $(LOCAL_INC)

TARGET_CFLAGS += -O2 -g -W -Wall -fPIC
TARGET_CPPFLAGS += -DSOCKLEN_T=socklen_t  \
                   -D_LARGEFILE_SOURCE=1 -D_FILE_OFFSET_BITS=64 \
                   -DSO_REUSEPORT -DALLOW_SERVER_PORT_REUSE \
                   -DAW_RTSP_LIB_VERSION=\"$(lib_version)\" \
                   -DBUILD_BY_WHO=\"$(who)\" \
                   -DBUILD_TIME=\"$(build_time)\" \
                   -DUSE_SIGNALS -DREUSE_FOR_TCP \
                   -Wall -DBSD=1 -O2 -g -W -Wall -fexceptions -fPIC

TARGET_LDFLAGS += -shared
TARGET_LDFLAGS += -lpthread -llog -ldl -lc -lm -lstdc++ -lrt

TARGET_MODULE := libTinyHttpServer
include $(BUILD_SHARED_LIB)
