
## 1. cpu arch
ifeq ($(TARGET_ARCH_VARIANT), armv8-a)
LOCAL_CFLAGS += -DCONF_ARMV8_A
endif

ifeq ($(TARGET_ARCH_VARIANT), armv7-a-neon)
os_version = $(shell echo $(PLATFORM_VERSION) | cut -c 1)
ifeq ($(os_version), 7)
LOCAL_CFLAGS +=
else
LOCAL_CFLAGS += -DCONF_ARMV7_A_NEON
endif
endif

## 2. Android Version
##    Auto detect Android version by SDK level
SDK_LEVEL_JB42 := 17
SDK_LEVEL_KK := 19
SDK_LEVEL_L := 21
SDK_LEVEL_M := 23
SDK_LEVEL_N := 24
SDK_LEVEL_O := 26
SDK_LEVEL_P := 28

ifeq ($(PLATFORM_SDK_VERSION),)
PLATFORM_SDK_VERSION = 0
endif

ifeq ($(word 1,$(sort $(PLATFORM_SDK_VERSION) $(SDK_LEVEL_P))), $(SDK_LEVEL_P))
PIE_AND_NEWER = yes
LOCAL_32_BIT_ONLY := true
LOCAL_CFLAGS += -Wno-error=unused-variable -Wno-error=unused-function -Wno-error=unused-label -Wno-error=unused-value \
             -Wno-error=unused-parameter -Wno-error=incompatible-pointer-types -Wno-error=implicit-function-declaration -Wno-error=format -Wno-error=return-type

else
PIE_AND_NEWER = no
endif

ifeq ($(word 1,$(sort $(PLATFORM_SDK_VERSION) $(SDK_LEVEL_O))), $(SDK_LEVEL_O))
OREO_AND_NEWER = yes
LOCAL_CFLAGS += -DCONF_OREO_AND_NEWER
LOCAL_CFLAGS += -DCONFIG_VE_IPC_ENABLE
LOCAL_32_BIT_ONLY := true
else
OREO_AND_NEWER = no
endif

ifeq ($(word 1,$(sort $(PLATFORM_SDK_VERSION) $(SDK_LEVEL_N))), $(SDK_LEVEL_N))
NOUGAT_AND_NEWER = yes
LOCAL_CFLAGS += -DCONF_NOUGAT_AND_NEWER
LOCAL_CFLAGS += -DCONFIG_VE_IPC_ENABLE
else
NOUGAT_AND_NEWER = no
endif

ifeq ($(word 1,$(sort $(PLATFORM_SDK_VERSION) $(SDK_LEVEL_M))), $(SDK_LEVEL_M))
MARSHMALLOW_AND_NEWER = yes
LOCAL_CFLAGS += -DCONF_MARSHMALLOW_AND_NEWER
LOCAL_CFLAGS += -DCONFIG_VE_IPC_ENABLE
else
MARSHMALLOW_AND_NEWER = no
endif

ifeq ($(word 1,$(sort $(PLATFORM_SDK_VERSION) $(SDK_LEVEL_L))), $(SDK_LEVEL_L))
LOLLIPOP_AND_NEWER = yes
LOCAL_CFLAGS += -DCONF_LOLLIPOP_AND_NEWER
LOCAL_CFLAGS += -DCONFIG_VE_IPC_ENABLE
else
LOLLIPOP_AND_NEWER = no
endif

ifeq ($(word 1,$(sort $(PLATFORM_SDK_VERSION) $(SDK_LEVEL_KK))), $(SDK_LEVEL_KK))
KITKAT_AND_NEWER = yes
LOCAL_CFLAGS += -DCONF_KITKAT_AND_NEWER
LOCAL_CFLAGS += -DCONFIG_VE_IPC_ENABLE
else
KITKAT_AND_NEWER = no
endif

ifeq ($(word 1,$(sort $(PLATFORM_SDK_VERSION) $(SDK_LEVEL_JB42))), $(SDK_LEVEL_JB42))
JB42_AND_NEWER = yes
LOCAL_CFLAGS += -DCONF_JB42_AND_NEWER
else
JB42_AND_NEWER = no
endif

## 3. secure os
#on semelis secure os, we transform phy addr to secure os to operate the buffer,
#but we adjust on optee secure os, just transform vir addr.
ifeq ($(BOARD_WIDEVINE_OEMCRYPTO_LEVEL), 1)
PLATFORM_SURPPORT_SECURE_OS = yes
LOCAL_CFLAGS +=-DPLATFORM_SURPPORT_SECURE_OS=1

    ifeq ($(SECURE_OS_OPTEE), yes)
        LOCAL_CFLAGS +=-DSECURE_OS_OPTEE=1
        LOCAL_CFLAGS +=-DADJUST_ADDRESS_FOR_SECURE_OS_OPTEE=1
    else
        LOCAL_CFLAGS +=-DSECURE_OS_OPTEE=0
        LOCAL_CFLAGS +=-DADJUST_ADDRESS_FOR_SECURE_OS_OPTEE=0
    endif

else
PLATFORM_SURPPORT_SECURE_OS = no
LOCAL_CFLAGS +=-DPLATFORM_SURPPORT_SECURE_OS=0
endif

#endif ## 'BOARD_WIDEVINE_OEMCRYPTO_LEVEL == 1'

# for debug afbc function
#LOCAL_CFLAGS += -DENABLE_AFBC=1

ifeq ($(USE_IOMMU), true)
LOCAL_CFLAGS += -DCONF_USE_IOMMU
endif

#LOCAL_CFLAGS += -DCONFIG_VE_IPC_ENABLE
