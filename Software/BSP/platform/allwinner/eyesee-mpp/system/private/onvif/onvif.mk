TARGET_PATH :=$(call my-dir)
TOP_PATH := $(TARGET_PATH)

# $(shell cp $(TARGET_PATH)/OnvifAdaptor.h $(TARGET_PATH)/src/include/onvif/)
$(info $(shell cp -v $(TARGET_TOP)/custom_aw/apps/newipc/source/bll_presenter/remote/interface/dev_ctrl_adapter.h $(TARGET_PATH)/src/include))
$(info $(shell cp -v $(TARGET_TOP)/custom_aw/apps/newipc/source/bll_presenter/remote/interface/remote_connector.h $(TARGET_PATH)/src/include))
$(info $(shell cp -v $(TARGET_TOP)/custom_aw/apps/newipc/source/bll_presenter/remote/onvif/onvif_param.h $(TARGET_PATH)/src/include/onvif/))

lib_version = "$(shell git --git-dir=$(TARGET_PATH)/.git log -n 1 | grep commit | cut -d ' ' -f 2)"
build_time = "$(shell date)"
who = "$(shell whoami)"

LOCAL_SRC := \
			src/core/XmlBuilder.cpp \
			src/core/tinystr.cpp \
			src/core/tinyxml.cpp \
			src/core/tinyxmlerror.cpp \
			src/core/tinyxmlparser.cpp \
			src/core/HttpCodec.cpp \
			src/core/SoapUtils.cpp \
			src/core/EventLoop.cpp \
			src/core/utils.cpp \
			src/service/DiscoveryService.cpp \
			src/service/OnvifConnector.cpp \
			src/service/SoapService.cpp \
			src/onvif_method/systemAndCodec.cpp \
			src/onvif_method/osdAndAnalytic.cpp \
			src/onvif_method/Packbits.cpp

LOCAL_INC := \
			$(TARGET_PATH)/src/include

#########################################
include $(ENV_CLEAR)

TARGET_SRC := $(LOCAL_SRC)
TARGET_INC := $(LOCAL_INC)

TARGET_CPPFLAGS += -DUSE_LOG_LIB_GLOG

TARGET_CPPFLAGS += -fPIC -DTIXML_USE_STL -g \
                   -DAW_ONVIF_LIB_VERSION=\"$(lib_version)\" \
                   -DBUILD_BY_WHO=\"$(who)\" \
                   -DBUILD_TIME=\"$(build_time)\" \


TARGET_ARFLAGS += -s
TARGET_LDFLAGS += -llog

TARGET_MODULE := libOnvif
include $(BUILD_STATIC_LIB)

#########################################
include $(ENV_CLEAR)

TARGET_SRC := $(LOCAL_SRC)
TARGET_INC := $(LOCAL_INC)

TARGET_CPPFLAGS += -DUSE_LOG_LIB_GLOG

TARGET_CPPFLAGS += -fPIC -DTIXML_USE_STL -g \
                   -DAW_ONVIF_LIB_VERSION=\"$(lib_version)\" \
                   -DBUILD_BY_WHO=\"$(who)\" \
                   -DBUILD_TIME=\"$(build_time)\" \

TARGET_LDFLAGS += -lpthread -llog -shared

TARGET_MODULE := libOnvif
include $(BUILD_SHARED_LIB)
