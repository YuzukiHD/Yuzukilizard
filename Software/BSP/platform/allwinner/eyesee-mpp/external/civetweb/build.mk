TARGET_PATH :=$(call my-dir)

include $(TARGET_TOP)/middleware/config/mpp_config.mk

ifeq ($(MPPCFG_COMPILE_DYNAMIC_LIB), Y)

########################################
include $(ENV_CLEAR)

TARGET_SRC := \
	src/civetweb.c \
	src/CivetServer.cpp \

TARGET_INC := \
    $(TARGET_PATH)/include

TARGET_CFLAGS += \
    -Wall -Os -Wextra -Wshadow -Wformat-security -Winit-self -Wmissing-prototypes \
    -DLINUX -DNO_SSL -DUSE_STACK_SIZE=102400 \
	-fPIC

TARGET_CPPFLAGS += \
    -Wall -Os -Wextra -Wshadow -Wformat-security -Winit-self \
    -DLINUX -DNO_SSL -DUSE_STACK_SIZE=102400 \
	-fPIC \
    -fexceptions \

TARGET_LDFLAGS += \
    -lpthread \
    -lrt \
    -ldl

TARGET_MODULE := libcivetweb
include $(BUILD_SHARED_LIB)

else

########################################
include $(ENV_CLEAR)
TARGET_SRC := \
	src/civetweb.c \
	src/CivetServer.cpp \

TARGET_INC := \
    $(TARGET_PATH)/include

TARGET_CFLAGS += \
    -Wall -Os -Wextra -Wshadow -Wformat-security -Winit-self -Wmissing-prototypes \
    -DLINUX -DNO_SSL -DUSE_STACK_SIZE=102400 \
	-fPIC

TARGET_CPPFLAGS += \
    -Wall -Os -Wextra -Wshadow -Wformat-security -Winit-self \
    -DLINUX -DNO_SSL -DUSE_STACK_SIZE=102400 \
	-fPIC \
    -fexceptions \

TARGET_LDFLAGS += \
    -lpthread \
    -lrt \
    -ldl

TARGET_MODULE := libcivetweb
include $(BUILD_STATIC_LIB)

endif

#########################################
include $(ENV_CLEAR)

TARGET_SRC := \
    src/civetweb.c \
    src/main.c \

TARGET_INC := \
    $(TARGET_PATH)/include

TARGET_CFLAGS += \
    -Wall -Os -Wextra -Wshadow -Wformat-security -Winit-self -Wmissing-prototypes \
    -DLINUX -DNO_SSL -DUSE_STACK_SIZE=102400

TARGET_LDFLAGS += \
    -lpthread \
    -lrt \
    -ldl

TARGET_MODULE := civetweb
include $(BUILD_BIN)

$(call copy-one-file, \
    $(TARGET_PATH)/resources/civetweb.conf, \
    $(TARGET_OUT)/target/etc/civetweb.conf \
)

$(call copy-one-file, \
    $(TARGET_PATH)/resources/civetweb_64x64.png, \
    $(TARGET_OUT)/target/var/www/civetweb_64x64.png \
)

$(call copy-one-file, \
    $(TARGET_PATH)/resources/itworks.html, \
    $(TARGET_OUT)/target/var/www/index.html \
)
