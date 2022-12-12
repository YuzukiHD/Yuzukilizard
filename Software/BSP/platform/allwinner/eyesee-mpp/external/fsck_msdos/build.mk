TARGET_PATH := $(call my-dir)

include $(ENV_CLEAR)

TARGET_SRC := boot.c check.c dir.c fat.c main.c

TARGET_INC := external/fsck_msdos/

TARGET_CFLAGS += -fPIC -Wall -Wno-unused-function -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-label \
    -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64

TARGET_MODULE := fsck_msdos
TARGET_SHARED_LIB := 

include $(BUILD_BIN)
