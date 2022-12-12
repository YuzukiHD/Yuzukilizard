#SRC_FILES := $(wildcard src/*.cpp

include makefile.common

COMMON_FILE := \
			src/core/XmlBuilder.cpp \
			src/core/tinystr.cpp \
			src/core/tinyxml.cpp \
			src/core/tinyxmlerror.cpp \
			src/core/tinyxmlparser.cpp \
			src/core/HttpCodec.cpp \
			src/core/SoapUtils.cpp \
			src/core/EventLoop.cpp \
			src/core/utils.cpp \
			# src/service/DiscoveryService.cpp \
			# src/service/OnvifConnector.cpp \
			# src/service/SoapService.cpp \
			# src/onvif_method/systemAndCodec.cpp \
			# src/onvif_method/osdAndAnalytic.cpp

##############################################################
include makefile.clear

SRC_FILES += example/testXmlBuilder.cpp

SRC_FILES += $(COMMON_FILE)

INC_DIRS += src/include

TARGET_NAME += testXmlBuilder
TARGET_DIR += .

LD_FLAGS +=
CC_FLAGS += -fPIC -DTIXML_USE_STL \
			-pg -g -fno-rtti -fno-exceptions

build-share := f
build-static := f
build-exe := t

include makefile.main

##############################################################

include makefile.clear

SRC_FILES += example/testXmlParser.cpp

SRC_FILES += $(COMMON_FILE)

INC_DIRS += \
			src/include

TARGET_NAME += testXmlParser
TARGET_DIR += .

LD_FLAGS +=
CC_FLAGS += -g -fPIC -DTIXML_USE_STL -fno-rtti -fno-exceptions -pg


build-share := f
build-static := f
build-exe := t

include makefile.main

##############################################################

include makefile.clear

SRC_FILES += example/testHttpCodec.cpp

SRC_FILES += $(COMMON_FILE)

INC_DIRS += \
			src/include

TARGET_NAME += testHttpCodec
TARGET_DIR += .

LD_FLAGS +=
CC_FLAGS += -g -fPIC -DTIXML_USE_STL -fno-rtti -fno-exceptions -pg

build-share := f
build-static := f
build-exe := t

include makefile.main

##############################################################

# include makefile.clear

# SRC_FILES += example/testOnvif.cpp

# SRC_FILES += $(COMMON_FILE)

# INC_DIRS += \
			# src/include

# TARGET_NAME += testOnvif
# TARGET_DIR += .

# LD_FLAGS +=
# CC_FLAGS += -g -fPIC -DTIXML_USE_STL

# build-share := f
# build-static := f
# build-exe := t

# include makefile.main

##############################################################
















