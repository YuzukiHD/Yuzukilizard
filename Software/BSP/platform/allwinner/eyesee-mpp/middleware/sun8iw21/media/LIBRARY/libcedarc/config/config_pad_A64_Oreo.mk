############################################
## configurations: PAD-A64-8.1
############################################
include $(MODULE_TOP)/config/config_common.mk

## 1. linux kernel version
#LOCAL_CFLAGS += -DCONF_KERNEL_VERSION_3_10
LOCAL_CFLAGS += -DCONF_KERNEL_VERSION_4_9

## 2. gpu
LOCAL_CFLAGS += -DCONF_MALI_GPU
#LOCAL_CFLAGS += -DCONF_IMG_GPU

## 3. new display framework
NEW_DISPLAY = yes

## 3. support 4K scaledown
LOCAL_CFLAGS += -DCONF_OMX_BIG_SIZE_SCALEDOWN

##9. DI process enable
#LOCAL_CFLAGS += -DCONF_ENABLE_OPENMAX_DI_FUNCTION



LOCAL_CFLAGS += -DCONF_KERN_BITWIDE=64



