############################################
## configurations: BOX-H3-7.0
############################################
include $(TOP)/frameworks/av/media/libcedarc/config/config_common.mk

## 1. linux kernel version
#LOCAL_CFLAGS += -DCONF_KERNEL_VERSION_3_4
#LOCAL_CFLAGS += -DCONF_KERNEL_VERSION_3_10
LOCAL_CFLAGS += -DCONF_KERNEL_VERSION_4_4

## 2. gpu
LOCAL_CFLAGS += -DCONF_MALI_GPU
#LOCAL_CFLAGS += -DCONF_IMG_GPU

## 3. new display framework
NEW_DISPLAY = yes

## 4. suporrt 4K 30fps recoder
#LOCAL_CFLAGS += -DCONF_SUPPORT_4K_30FPS_RECORDER

## 5. audio decoder soundtouch
#LOCAL_CFLAGS += -CONFIG_ADECODER_SUPPORT_SOUNDTOUCH

## 6. ve ipc
#LOCAL_CFLAGS += -DCONFIG_VE_IPC_ENABLE

## 7.
#LOCAL_CFLAGS += -DCONF_OMX_BIG_SIZE_SCALEDOWN

##8. omx drop B frame
LOCAL_CFLAGS += -DOMX_DROP_B_FRAME_4K
##9. DI process enable
LOCAL_CFLAGS += -DCONF_ENABLE_OPENMAX_DI_FUNCTION
##10. config kernel bitwide value as 32 explicitly FOR DI process.
LOCAL_CFLAGS += -DCONF_KERN_BITWIDE=32
## 11. DI process 3 input pictures. Otherwise, 2 input pictures would be processed anyway.
##LOCAL_CFLAGS += -DCONF_DI_PROCESS_3_PICTURE