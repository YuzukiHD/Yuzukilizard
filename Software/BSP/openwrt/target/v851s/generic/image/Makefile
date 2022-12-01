#
# Copyright (C) 2013-2016 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#
include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/image.mk
include $(INCLUDE_DIR)/host-build.mk

FAT32_BLOCK_SIZE=1024
ifneq ($(CONFIG_SUNXI_SD_BOOT_PARTSIZE),)
	FAT32_BLOCKS=$(shell echo $$(($(CONFIG_SUNXI_SD_BOOT_PARTSIZE)*1024*1024/$(FAT32_BLOCK_SIZE))))
endif
ifneq ($(CONFIG_SUNXI_RECOVERY_INITRAMFS_PARTSIZE),)
	RECOVERY_INITRAMFS_BLOCKS=$(shell echo $$(($(CONFIG_SUNXI_RECOVERY_INITRAMFS_PARTSIZE)*1024*1024/$(FAT32_BLOCK_SIZE))))
endif

IMAGE_NAME_SUFFIX:=
ifneq ($(CONFIG_SUNXI_BOOT_IMAGE_NAME_SUFFIX_RECOVERY),)
	IMAGE_NAME_SUFFIX=_recovery
else ifneq ($(CONFIG_SUNXI_BOOT_IMAGE_NAME_SUFFIX_RAMFS),)
	IMAGE_NAME_SUFFIX=_ramfs
endif

ROOTFS_IMAGE_NAME_SUFFIX:=
ifneq ($(CONFIG_SUNXI_ROOTFS_IMAGE_NAME_SUFFIX_RECOVERY),)
	ROOTFS_IMAGE_NAME_SUFFIX=_recovery
else ifneq ($(CONFIG_SUNXI_ROOTFS_IMAGE_NAME_SUFFIX_RAMFS),)
	ROOTFS_IMAGE_NAME_SUFFIX=_ramfs
endif

LOAD_ADDRESS:=0x40008000
ENTRY_POINT:=0x40008000
BASE_ADDRESS:=0x40000000
KERNEL_OFFSET:=0x8000
RAMDISK_OFFSET:=0x01000000
IMAGE_DATA:=zImage
BOOTIMG_IMAGE_DATA:=zImage
RAMDISK:=ramdisk.img
UIMAGE_NAME:=uImage
BOOTIMG_NAME:=boot.img
TARCH:=$(ARCH)
COMP:=none
ifeq ($(ARCH), aarch64)
TARCH=arm64
endif
TARGET_BOOT_IMG:=boot${IMAGE_NAME_SUFFIX}.img
TARGET_BOOT_INITRAMFS_IMG:=boot_initramfs${IMAGE_NAME_SUFFIX}.img
TARGET_ROOTFS_IMG:=rootfs${ROOTFS_IMAGE_NAME_SUFFIX}.img
TARGET_USR_IMG:=usr${ROOTFS_IMAGE_NAME_SUFFIX}.img

ifneq ($(CONFIG_TARGET_ROOTFS_INITRAMFS),)
UIMAGE_NAME=uImage-initramfs
BOOTIMG_NAME=boot-initramfs.img
endif

ifeq ($(ARCH), aarch64)
LOAD_ADDRESS=0x40080000
ENTRY_POINT=0x40080000
BASE_ADDRESS=0x40000000
KERNEL_OFFSET=0x80000
RAMDISK_OFFSET:=0x02000000
IMAGE_DATA=Image.gz
BOOTIMG_IMAGE_DATA=bImage
COMP=gzip
ifneq ($(CONFIG_KERNEL_KERNEL_LZO),)
IMAGE_DATA=Image.lzo
COMP=lzo
endif
endif

ifeq ($(ARCH), riscv)
LOAD_ADDRESS=0x40080000
ENTRY_POINT=0x40080000
BASE_ADDRESS=0x40200000
KERNEL_OFFSET=0x0
RAMDISK_OFFSET:=0x01000000
IMAGE_DATA=Image.gz
BOOTIMG_IMAGE_DATA=Image
COMP=gzip
endif

ifneq (,$(findstring $(TARGET_BOARD_PLATFORM), r6 c200s v133 c600))
LOAD_ADDRESS=0x80008000
ENTRY_POINT=0x80008000
BASE_ADDRESS=0x80000000
KERNEL_OFFSET=0x8000
endif

define mkBootimg
mkbootimg --kernel $(1) \
	--ramdisk $(2) \
	--board $(3) \
	--base $(4) \
	--kernel_offset $(5) \
	--ramdisk_offset $(6) \
	-o $(7)
endef

define mkBootimg_without_ramdisk
mkbootimg --kernel $(1) \
	--board $(2) \
	--base $(3) \
	--kernel_offset $(4) \
	-o $(5)
endef

define mkuImage
	mkimage -A $(TARCH) -O linux -T kernel -C $(1) -a $(2) -e $(3) \
		-n '$(call toupper,$(ARCH)) OpenWrt Linux-$(LINUX_VERSION)' -d $(4) $(5)
endef

define Image/BuildKernel
	$(CP) $(KDIR)/$(IMAGE_DATA) $(TARGET_OUT_DIR)/$(IMG_PREFIX)-$(IMAGE_DATA)
	$(call mkuImage, \
		$(COMP), $(LOAD_ADDRESS), $(ENTRY_POINT), \
		$(KDIR)/$(IMAGE_DATA), $(TARGET_OUT_DIR)/$(IMG_PREFIX)-$(UIMAGE_NAME) \
		)

ifneq ($(CONFIG_SUNXI_MKBOOTIMG_WITH_COMPRESS_KERNEL),)
	$(call mkBootimg_without_ramdisk, \
		$(KDIR)/$(IMAGE_DATA), \
		$(shell echo $(IMG_PREFIX) | cut -c 1-16 ), \
		${BASE_ADDRESS}, \
		${KERNEL_OFFSET}, \
		$(TARGET_OUT_DIR)/$(IMG_PREFIX)-$(BOOTIMG_NAME),\
		)
else
	$(call mkBootimg, \
		$(KDIR)/$(BOOTIMG_IMAGE_DATA), \
		$(RAMDISK), \
		$(shell echo $(IMG_PREFIX) | cut -c 1-16 ), \
		${BASE_ADDRESS}, \
		${KERNEL_OFFSET}, \
		${RAMDISK_OFFSET}, \
		$(TARGET_OUT_DIR)/$(IMG_PREFIX)-$(BOOTIMG_NAME),\
		)
endif
endef

define Image/Build/Initramfs
	$(CP) $(KDIR)/$(IMAGE_DATA)-initramfs $(TARGET_OUT_DIR)/$(IMG_PREFIX)-$(IMAGE_DATA)-initramfs
	echo -ne '\x00\x00\x00\x00' >> $(TARGET_OUT_DIR)/$(IMG_PREFIX)-$(IMAGE_DATA)-initramfs
	$(call mkuImage, \
		$(COMP), $(LOAD_ADDRESS), $(ENTRY_POINT), \
		$(TARGET_OUT_DIR)/$(IMG_PREFIX)-$(IMAGE_DATA)-initramfs, \
		$(TARGET_OUT_DIR)/$(IMG_PREFIX)-$(UIMAGE_NAME) \
		)

	$(CP) $(KDIR)/$(BOOTIMG_IMAGE_DATA)-initramfs $(TARGET_OUT_DIR)/$(IMG_PREFIX)-$(BOOTIMG_IMAGE_DATA)-initramfs
	echo -ne '\x00\x00\x00\x00' >> $(TARGET_OUT_DIR)/$(IMG_PREFIX)-$(BOOTIMG_IMAGE_DATA)-initramfs
ifneq ($(CONFIG_SUNXI_MKBOOTIMG_WITH_COMPRESS_KERNEL),)
	$(call mkBootimg, \
		$(TARGET_OUT_DIR)/$(IMG_PREFIX)-$(IMAGE_DATA)-initramfs, \
		$(RAMDISK), \
		$(shell echo $(IMG_PREFIX) | cut -c 1-16 ), \
		${BASE_ADDRESS}, \
		${KERNEL_OFFSET}, \
		${RAMDISK_OFFSET}, \
		$(TARGET_OUT_DIR)/$(IMG_PREFIX)-$(BOOTIMG_NAME),\
		)
else
	$(call mkBootimg, \
		$(TARGET_OUT_DIR)/$(IMG_PREFIX)-$(BOOTIMG_IMAGE_DATA)-initramfs, \
		$(RAMDISK), \
		$(shell echo $(IMG_PREFIX) | cut -c 1-16 ), \
		${BASE_ADDRESS}, \
		${KERNEL_OFFSET}, \
		${RAMDISK_OFFSET}, \
		$(TARGET_OUT_DIR)/$(IMG_PREFIX)-$(BOOTIMG_NAME),\
		)
endif
endef

ifneq ($(CONFIG_TARGET_ROOTFS_INITRAMFS),)
define Image/InstallKernel
	rm -f $(TARGET_OUT_DIR)/${TARGET_BOOT_IMG}
	rm -f $(TARGET_OUT_DIR)/${TARGET_BOOT_INITRAMFS_IMG}
ifneq ($(CONFIG_SUNXI_RECOVERY_INITRAMFS_PARTSIZE),)
	mkdosfs $(TARGET_OUT_DIR)/${TARGET_BOOT_INITRAMFS_IMG} -C $(RECOVERY_INITRAMFS_BLOCKS)

ifneq ($(CONFIG_SUNXI_RECOVERY_INITRAMFS_KERNEL_FORMAT_UIMAGE),)
	mcopy -i $(TARGET_OUT_DIR)/${TARGET_BOOT_INITRAMFS_IMG} $(TARGET_OUT_DIR)/$(IMG_PREFIX)-$(UIMAGE_NAME) ::uImage
else ifneq ($(CONFIG_SUNXI_RECOVERY_INITRAMFS_KERNEL_FORMAT_BOOTIMG),)
	mcopy -i $(TARGET_OUT_DIR)/${TARGET_BOOT_INITRAMFS_IMG} $(TARGET_OUT_DIR)/$(IMG_PREFIX)-$(BOOTIMG_NAME) ::boot.img
else
	# Compatible with old config
	mcopy -i $(TARGET_OUT_DIR)/${TARGET_BOOT_INITRAMFS_IMG} $(TARGET_OUT_DIR)/$(IMG_PREFIX)-$(UIMAGE_NAME) ::uImage
endif

else

ifneq ($(CONFIG_SUNXI_RECOVERY_INITRAMFS_KERNEL_FORMAT_UIMAGE),)
	$(CP) $(TARGET_OUT_DIR)/$(IMG_PREFIX)-$(UIMAGE_NAME) $(TARGET_OUT_DIR)/${TARGET_BOOT_INITRAMFS_IMG}
else ifneq ($(CONFIG_SUNXI_RECOVERY_INITRAMFS_KERNEL_FORMAT_BOOTIMG),)
	$(CP) $(TARGET_OUT_DIR)/$(IMG_PREFIX)-$(BOOTIMG_NAME) $(TARGET_OUT_DIR)/${TARGET_BOOT_INITRAMFS_IMG}
else
	# Compatible with old config
	$(CP) $(TARGET_OUT_DIR)/$(IMG_PREFIX)-$(UIMAGE_NAME) $(TARGET_OUT_DIR)/${TARGET_BOOT_INITRAMFS_IMG}
endif

endif
endef

else

define Image/InstallKernel
	rm -f $(TARGET_OUT_DIR)/${TARGET_BOOT_IMG}
	rm -f $(TARGET_OUT_DIR)/${TARGET_BOOT_INITRAMFS_IMG}

ifneq ($(CONFIG_SUNXI_SD_BOOT_PARTSIZE),)
	mkdosfs $(TARGET_OUT_DIR)/${TARGET_BOOT_IMG} -C $(FAT32_BLOCKS)

ifneq ($(CONFIG_SUNXI_SD_BOOT_KERNEL_FORMAT_UIMAGE),)
	mcopy -i $(TARGET_OUT_DIR)/${TARGET_BOOT_IMG} $(TARGET_OUT_DIR)/$(IMG_PREFIX)-$(UIMAGE_NAME) ::uImage
else ifneq ($(CONFIG_SUNXI_SD_BOOT_KERNEL_FORMAT_BOOTIMG),)
	mcopy -i $(TARGET_OUT_DIR)/${TARGET_BOOT_IMG} $(TARGET_OUT_DIR)/$(IMG_PREFIX)-$(BOOTIMG_NAME) ::boot.img
else
	# Compatible with old config
	mcopy -i $(TARGET_OUT_DIR)/${TARGET_BOOT_IMG} $(TARGET_OUT_DIR)/$(IMG_PREFIX)-$(UIMAGE_NAME) ::uImage
endif

else

ifneq ($(CONFIG_SUNXI_SD_BOOT_KERNEL_FORMAT_UIMAGE),)
	$(CP) $(TARGET_OUT_DIR)/$(IMG_PREFIX)-$(UIMAGE_NAME) $(TARGET_OUT_DIR)/${TARGET_BOOT_IMG}
else ifneq ($(CONFIG_SUNXI_SD_BOOT_KERNEL_FORMAT_BOOTIMG),)
	$(CP) $(TARGET_OUT_DIR)/$(IMG_PREFIX)-$(BOOTIMG_NAME) $(TARGET_OUT_DIR)/${TARGET_BOOT_IMG}
else
	# Compatible with old config
	$(CP) $(TARGET_OUT_DIR)/$(IMG_PREFIX)-$(UIMAGE_NAME) $(TARGET_OUT_DIR)/${TARGET_BOOT_IMG}
endif

endif
endef
endif

define Image/Build
	rm -f $(TARGET_OUT_DIR)/$(TARGET_ROOTFS_IMG)
	rm -f $(TARGET_OUT_DIR)/$(TARGET_USR_IMG)
	$(call Image/Build/$(1),$(1))
	dd if=$(KDIR)/root.$(1) of=$(TARGET_OUT_DIR)/$(TARGET_ROOTFS_IMG) bs=128k conv=sync
ifneq ($(CONFIG_SUNXI_SMALL_STORAGE_OTA),)
	dd if=$(KDIR)/usr.$(1) of=$(TARGET_OUT_DIR)/$(TARGET_USR_IMG) bs=128k conv=sync
endif
endef

$(eval $(call BuildImage))
