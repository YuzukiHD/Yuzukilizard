############################################
## configurations: VR-A63-7.1
############################################
include $(MODULE_TOP)/config/config_common.mk

## 1. linux kernel version
LOCAL_CFLAGS += -DCONF_KERNEL_VERSION_3_10

## 2. gpu
LOCAL_CFLAGS += -DCONF_MALI_GPU
#LOCAL_CFLAGS += -DCONF_IMG_GPU

## 3. new display framework
NEW_DISPLAY = yes

## 3. support iommu
##LOCAL_CFLAGS += -DCONF_USE_IOMMU


