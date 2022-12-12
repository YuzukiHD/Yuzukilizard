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

TARGET_SRC := \
	hashmap.c \
	atomic.c.arm \
	native_handle.c \
	socket_inaddr_any_server.c \
	socket_local_client.c \
	socket_local_server.c \
	socket_loopback_client.c \
	socket_loopback_server.c \
	socket_network_client.c \
	sockets.c \
	config_utils.c \
	cpu_info.c \
	load_file.c \
	list.c \
	open_memstream.c \
	strdup16to8.c \
	strdup8to16.c \
	record_stream.c \
	process_name.c \
	properties.c \
	threads.c \
	sched_policy.c \
	iosched_policy.c \
	str_parms.c \
	fs.c \
	memory.c

#TARGET_SRC += arch-arm/memset32.S
TARGET_INC := ./include/cutils/
TARGET_CFLAGS += \
	-DHAVE_SYS_UIO_H \
	-DHAVE_IOCTL \
	-DANDROID_SMP=1 \
	-O2 -fPIC -Wall -Wno-unused-variable -Wno-unused-function -Wno-unused-but-set-variable

TARGET_LDFLAGS += \
	-lpthread -static
TARGET_MODULE := libcutils
include $(BUILD_STATIC_LIB)

#########################################
include $(ENV_CLEAR)

TARGET_SRC := \
	hashmap.c \
	atomic.c.arm \
	native_handle.c \
	socket_inaddr_any_server.c \
	socket_local_client.c \
	socket_local_server.c \
	socket_loopback_client.c \
	socket_loopback_server.c \
	socket_network_client.c \
	sockets.c \
	config_utils.c \
	cpu_info.c \
	load_file.c \
	list.c \
	open_memstream.c \
	strdup16to8.c \
	strdup8to16.c \
	record_stream.c \
	process_name.c \
	properties.c \
	threads.c \
	sched_policy.c \
	iosched_policy.c \
	str_parms.c \
	memory.c

#TARGET_SRC += arch-arm/memset32.S
TARGET_INC := ./include/cutils/
TARGET_CFLAGS += \
	-DHAVE_SYS_UIO_H \
	-DHAVE_IOCTL \
	-DANDROID_SMP=1
TARGET_CFLAGS := -O2 -fPIC -Wall -ansi -pedantic

TARGET_LDFLAGS += \
	-lpthread \
	-shared -Wl,-Bsymbolic -Wl,-rpath -Wl,/usr/lib -Wl,-rpath,/usr/lib
TARGET_MODULE := libcutils
include $(BUILD_SHARED_LIB)

