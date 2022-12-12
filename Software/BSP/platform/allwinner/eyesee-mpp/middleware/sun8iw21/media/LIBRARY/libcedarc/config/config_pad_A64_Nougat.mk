############################################
## configurations: PAD-A64-5.0
############################################
include $(MODULE_TOP)/config/config_common.mk

## 1. linux kernel version
#LOCAL_CFLAGS += -DCONF_KERNEL_VERSION_3_4
LOCAL_CFLAGS += -DCONF_KERNEL_VERSION_3_10

## 2. gpu
LOCAL_CFLAGS += -DCONF_MALI_GPU
#LOCAL_CFLAGS += -DCONF_IMG_GPU

## 3. new display framework
NEW_DISPLAY = yes

## 4. suporrt 4K scaledown
LOCAL_CFLAGS += -DCONF_OMX_BIG_SIZE_SCALEDOWN

## 5. audio decoder soundtouch
#LOCAL_CFLAGS += -CONFIG_ADECODER_SUPPORT_SOUNDTOUCH

## 6. ve ipc
#LOCAL_CFLAGS += -DCONFIG_VE_IPC_ENABLE

