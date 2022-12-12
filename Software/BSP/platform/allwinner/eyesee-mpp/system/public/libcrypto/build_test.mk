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
TARGET_SRC := aes.c hmac-sha1.c hmac-sha256.c openssl_crypt.c\
	 root.c sha.c afalg.c aw_sample_hash.c aw_sample_rsa.c aw_sample_cipher.c
TARGET_INC := $(TARGET_PATH)/../include
TARGET_STATIC_LIB :=
TARGET_SHARED_LIB := libcrypto_aw lib${AF_ALG_V}
TARGET_CPPFLAGS += -g
TARGET_LDFLAGS += \
	-lpthread \
	-ldl\
	-lcrypto_aw\
	-lawcipher\
	-l${AF_ALG_V}

TARGET_MODULE := crypto_tapp
include $(BUILD_BIN)
