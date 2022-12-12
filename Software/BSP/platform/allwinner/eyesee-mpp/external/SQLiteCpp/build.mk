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

include $(TARGET_TOP)/middleware/config/mpp_config.mk

ifeq ($(MPPCFG_COMPILE_DYNAMIC_LIB), Y)

########################################
include $(ENV_CLEAR)

TARGET_SRC := \
	src/Backup.cpp \
	src/Column.cpp \
	src/Database.cpp \
	src/Exception.cpp \
	src/Statement.cpp \
	src/Transaction.cpp \

TARGET_INC := \
    $(TARGET_PATH)/include

TARGET_CPPFLAGS += \
	-fPIC \
    -fexceptions

TARGET_LDFLAGS += \
	-lsqlite3 \
	-llog

TARGET_MODULE := libSQLiteCpp
include $(BUILD_SHARED_LIB)

else

#########################################
include $(ENV_CLEAR)

TARGET_SRC := \
	src/Backup.cpp \
	src/Column.cpp \
	src/Database.cpp \
	src/Exception.cpp \
	src/Statement.cpp \
	src/Transaction.cpp \

TARGET_INC := \
    $(TARGET_PATH)/include

TARGET_CPPFLAGS += \
    -fexceptions

TARGET_STATIC_LIB := \
	# libsqlite3

TARGET_SHARED_LIB := \
	liblog

TARGET_MODULE := libSQLiteCpp
include $(BUILD_STATIC_LIB)

endif

#########################################
# include $(ENV_CLEAR)

# TARGET_SRC := \
	# examples/example1/main.cpp \

# TARGET_INC := \
    # $(TARGET_PATH)/include

# TARGET_CPPFLAGS += \
    # -fexceptions

# TARGET_STATIC_LIB := \
	# # libsqlite3

# TARGET_SHARED_LIB := \
	# liblog \
    # libSQLiteCpp

# TARGET_MODULE := example1
# include $(BUILD_BIN)
