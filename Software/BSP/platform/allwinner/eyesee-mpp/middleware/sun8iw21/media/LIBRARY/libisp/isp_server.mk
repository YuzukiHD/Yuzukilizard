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

#########################################
include $(ENV_CLEAR)
SRC_TAGS := \
	isp_events \
	isp_tuning \
	isp_manage \
	isp_helper
TARGET_SRC := $(call all-c-files-under, $(SRC_TAGS))
TARGET_SRC += isp.c
TARGET_SRC += isp_test.c

TARGET_INC := \
	$(TARGET_PATH)/include

TARGET_CFLAGS := -fPIC -Wall -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-value
TARGET_LDFLAGS += \
	-Wl,-Bstatic -lisp_dev -lisp_base -lisp_math -lisp_ae -lisp_af -lisp_afs \
	-lisp_awb -lisp_md -lisp_iso -lisp_gtm -lisp_ini -liniparser \
	-lisp_rolloff -lisp_pltm -Wl,-Bdynamic -lpthread -lrt

TARGET_MODULE := server
include $(BUILD_BIN)

#########################################
include $(ENV_CLEAR)
SRC_TAGS := \
	isp_events \
	isp_tuning \
	isp_manage \
	isp_helper
TARGET_SRC := $(call all-c-files-under, $(SRC_TAGS))
TARGET_SRC += isp.c
TARGET_SRC += isp_test.c

TARGET_INC := \
	$(TARGET_PATH)/include

TARGET_CFLAGS := -fPIC -Wall -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-value
TARGET_LDFLAGS += \
	-lisp_dev -lisp_base -lisp_math -lisp_ae -lisp_af -lisp_afs \
	-lisp_awb -lisp_md -lisp_iso -lisp_gtm -lisp_ini -liniparser \
	-lisp_rolloff -lisp_pltm -lpthread -lrt -shared
TARGET_MODULE := server_shared
include $(BUILD_BIN)

#########################################
