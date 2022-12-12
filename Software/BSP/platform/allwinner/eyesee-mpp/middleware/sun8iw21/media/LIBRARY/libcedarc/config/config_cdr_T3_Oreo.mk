############################################
## configurations: T3-8.x
############################################
include $(MODULE_TOP)/config/config_common.mk

## 1. linux kernel version
LOCAL_CFLAGS += -DCONF_KERNEL_VERSION_4_9

## 2. gpu
LOCAL_CFLAGS += -DCONF_MALI_GPU
#LOCAL_CFLAGS += -DCONF_IMG_GPU

## 3. new display framework
NEW_DISPLAY = yes

## 3. support iommu
##LOCAL_CFLAGS += -DCONF_USE_IOMMU

##9. DI process enable
#LOCAL_CFLAGS += -DCONF_ENABLE_OPENMAX_DI_FUNCTION
##10. config kernel bitwide value as 32 explicitly FOR DI process.
LOCAL_CFLAGS += -DCONF_KERN_BITWIDE=32
