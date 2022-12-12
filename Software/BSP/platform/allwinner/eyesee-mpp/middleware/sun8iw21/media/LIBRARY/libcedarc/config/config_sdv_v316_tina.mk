############################################
## configurations: sdv-v316-tina
############################################
include $(MODULE_TOP)/config/config_common.mk

## 1. linux kernel version
#LOCAL_CFLAGS += -DCONF_KERNEL_VERSION_3_4
LOCAL_CFLAGS += -DCONF_KERNEL_VERSION_3_10

## 2. gpu
#LOCAL_CFLAGS += -DCONF_MALI_GPU
#LOCAL_CFLAGS += -DCONF_IMG_GPU

## 3. new display framework
#NEW_DISPLAY = yes

## 4. suporrt 4K 30fps recoder
#LOCAL_CFLAGS += -DCONF_SUPPORT_4K_30FPS_RECORDER

## 5. audio decoder soundtouch
#LOCAL_CFLAGS += -CONFIG_ADECODER_SUPPORT_SOUNDTOUCH

## 6. ve ipc
#LOCAL_CFLAGS += -DCONFIG_VE_IPC_ENABLE

## 7. for decoder to decrease vbv buffer size
LOCAL_CFLAGS += -DCONF_SIMPLE_PARSER_CASE
# compile static lib (for debug)
include $(MODULE_TOP)/config/config_static_compile.mk

CEDARC_CFLAGS := ${LOCAL_CFLAGS}
