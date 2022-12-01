#
# Copyright (C) 2015-2016 Allwinner
#
# This is free software, licensed under the GNU General Public License v2.
# See /build/LICENSE for more information.
#
#define KernelPackage/sunxi-vfe
#  SUBMENU:=$(VIDEO_MENU)
#  TITLE:=sunxi-vfe support
#  FILES:=$(LINUX_DIR)/drivers/media/v4l2-core/videobuf2-core.ko
#  FILES+=$(LINUX_DIR)/drivers/media/v4l2-core/videobuf2-memops.ko
#  FILES+=$(LINUX_DIR)/drivers/media/v4l2-core/videobuf2-dma-contig.ko
#  FILES+=$(LINUX_DIR)/drivers/media/v4l2-core/videobuf2-v4l2.ko
#  FILES+=$(LINUX_DIR)/drivers/media/platform/sunxi-vfe/vfe_io.ko
#  FILES+=$(LINUX_DIR)/drivers/media/platform/sunxi-vfe/device/gc1034_mipi.ko
#  FILES+=$(LINUX_DIR)/drivers/media/platform/sunxi-vfe/vfe_v4l2.ko
#  KCONFIG:=\
#    CONFIG_VIDEO_SUNXI_VFE \
#    CONFIG_CSI_VFE \
#    CONFIG_CCI=m
#  AUTOLOAD:=$(call AutoLoad,90,videobuf2-core videobuf2-memops videobuf2-dma-contig videobuf2-v4l2 vfe_io gc1034_mipi vfe_v4l2)
#endef
#
#define KernelPackage/sunxi-vfe/description
#  Kernel modules for sunxi-vfe support
#endef
#
#$(eval $(call KernelPackage,sunxi-vfe))
#
#define KernelPackage/sunxi-sound
#  SUBMENU:=$(SOUND_MENU)
#  DEPENDS:=+kmod-sound-core
#  TITLE:=sun8iw8 sound support
#  FILES:=$(LINUX_DIR)/sound/soc/sunxi/audiocodec/sun8iw8_sndcodec.ko
#  FILES+=$(LINUX_DIR)/sound/soc/snd-soc-core.ko
#  FILES+=$(LINUX_DIR)/sound/soc/sunxi/audiocodec/sunxi_sndcodec.ko
#  FILES+=$(LINUX_DIR)/drivers/base/regmap/regmap-i2c.ko
#  FILES+=$(LINUX_DIR)/drivers/base/regmap/regmap-spi.ko
#  FILES+=$(LINUX_DIR)/sound/soc/sunxi/audiocodec/switch_hdset.ko
#  FILES+=$(LINUX_DIR)/sound/soc/sunxi/audiocodec/sunxi_codecdma.ko
#  FILES+=$(LINUX_DIR)/sound/soc/sunxi/audiocodec/sunxi_codec.ko
#  FILES+=$(LINUX_DIR)/sound/core/seq/snd-seq.ko
#  FILES+=$(LINUX_DIR)/sound/soc/snd-soc-core.ko
#  AUTOLOAD:=$(call AutoLoad,30,sun8iw12_sndcodec switch_hdset snd-soc-core sunxi_sndcodec sunxi_codecdma sunxi_codec snd-seq regmap-i2c   regmap-spi)
#endef
#
#define KernelPackage/sunxi-sound/description
#  Kernel modules for sun8iw8-sound support
#endef
#
#$(eval $(call KernelPackage,sunxi-sound))
#

define KernelPackage/net-rtl8189fs
  SUBMENU:=$(WIRELESS_MENU)
  TITLE:=RTL8189FS support (staging)
  DEPENDS:= +kmod-sunxi-rf-wlan +kmod-cfg80211 +kmod-mmc
  FILES:=$(LINUX_DIR)/net/wireless/cfg80211.ko
  FILES+=$(LINUX_DIR)/drivers/net/wireless/rtl8189fs/8189fs.ko
  FILES+=$(LINUX_DIR)/drivers/misc/sunxi-rf/sunxi-wlan.ko
  KCONFIG:=\
  CONFIG_RTL8189FS=m
  AUTOLOAD:=$(call AutoProbe, cfg80211.ko 8189fs.ko sunxi-wlan.ko)
endef

define KernelPackage/net-rtl8189fs/description
 Kernel modules for RealTek RTL8189FS support
endef

$(eval $(call KernelPackage,net-rtl8189fs))

define KernelPackage/net-xr819s-40M
  SUBMENU:=$(WIRELESS_MENU)
  TITLE:=xr819s support (staging)
  DEPENDS:= +xr819s-firmware +@IPV6 +@XR819S_USE_40M_SDD +@USES_XR819S +@PACKAGE_xr819s-rftest +@PACKAGE_xr819s-rftest
  KCONFIG:=\
	CONFIG_XR819S_WLAN=m \
	CONFIG_PM=y\
	CONFIG_BT=y \
	CONFIG_BT_BREDR=y \
	CONFIG_BT_RFCOMM=y \
	CONFIG_BT_RFCOMM_TTY=y \
	CONFIG_BT_DEBUGFS=y \
	CONFIG_XR_BT_LPM=y \
	CONFIG_XR_BT_FDI=y \
	CONFIG_BT_HCIUART=y \
	CONFIG_BT_HCIUART_H4=y \
	CONFIG_HFP_OVER_PCM=y \
	CONFIG_RFKILL=y \
	CONFIG_RFKILL_PM=y \
	CONFIG_RFKILL_GPIO=y

  FILES:=$(LINUX_DIR)/drivers/net/wireless/xr819s/wlan/xradio_core.ko
  FILES+=$(LINUX_DIR)/drivers/net/wireless/xr819s/wlan/xradio_wlan.ko
  FILES+=$(LINUX_DIR)/drivers/net/wireless/xr819s/umac/xradio_mac.ko
  AUTOLOAD:=$(call AutoProbe, xradio_mac xradio_core xradio_wlan)

  #FILES+=$(LINUX_DIR)/drivers/net/wireless/xr819s/xr819s.ko
  #AUTOLOAD:=$(call AutoProbe, xr819s)
endef

define KernelPackage/net-xr819s-40M/description
 Kernel modules for xr819s support
endef

$(eval $(call KernelPackage,net-xr819s-40M))

define KernelPackage/net-xr819s
  SUBMENU:=$(WIRELESS_MENU)
  TITLE:=xr819s support (staging)
  DEPENDS:= +xr819s-firmware +@IPV6 +@USES_XR819S +@PACKAGE_xr819s-rftest +@PACKAGE_xr819s-rftest
  KCONFIG:=\
	CONFIG_XR819S_WLAN=m \
	CONFIG_PM=y\
	CONFIG_BT=y \
	CONFIG_BT_BREDR=y \
	CONFIG_BT_RFCOMM=y \
	CONFIG_BT_RFCOMM_TTY=y \
	CONFIG_BT_DEBUGFS=y \
	CONFIG_XR_BT_LPM=y \
	CONFIG_XR_BT_FDI=y \
	CONFIG_BT_HCIUART=y \
	CONFIG_BT_HCIUART_H4=y \
	CONFIG_HFP_OVER_PCM=y \
	CONFIG_RFKILL=y \
	CONFIG_RFKILL_PM=y \
	CONFIG_RFKILL_GPIO=y

  FILES:=$(LINUX_DIR)/drivers/net/wireless/xr819s/wlan/xradio_core.ko
  FILES+=$(LINUX_DIR)/drivers/net/wireless/xr819s/wlan/xradio_wlan.ko
  FILES+=$(LINUX_DIR)/drivers/net/wireless/xr819s/umac/xradio_mac.ko
  AUTOLOAD:=$(call AutoProbe, xradio_mac xradio_core xradio_wlan)

  #FILES+=$(LINUX_DIR)/drivers/net/wireless/xr819s/xr819s.ko
  #AUTOLOAD:=$(call AutoProbe, xr819s)
endef

define KernelPackage/net-xr819s/description
 Kernel modules for xr819s support
endef

$(eval $(call KernelPackage,net-xr819s))

define KernelPackage/net-xr829-40M
  SUBMENU:=$(WIRELESS_MENU)
  TITLE:=xr829 support (staging)
  DEPENDS:= +xr829-firmware +@IPV6 +@XR829_USE_40M_SDD +@USES_XR829 +@PACKAGE_xr829-rftest
  KCONFIG:=\
	CONFIG_XR829_WLAN=m \
	CONFIG_PM=y\
	CONFIG_BT=y \
	CONFIG_BT_BREDR=y \
	CONFIG_BT_RFCOMM=y \
	CONFIG_BT_RFCOMM_TTY=y \
	CONFIG_BT_DEBUGFS=y \
	CONFIG_XR_BT_LPM=y \
	CONFIG_XR_BT_FDI=y \
	CONFIG_BT_HCIUART=y \
	CONFIG_BT_HCIUART_H4=y \
	CONFIG_HFP_OVER_PCM=y \
	CONFIG_RFKILL=y \
	CONFIG_RFKILL_PM=y \
	CONFIG_RFKILL_GPIO=y

  FILES:=$(LINUX_DIR)/drivers/net/wireless/xr829/wlan/xradio_core.ko
  FILES+=$(LINUX_DIR)/drivers/net/wireless/xr829/wlan/xradio_wlan.ko
  FILES+=$(LINUX_DIR)/drivers/net/wireless/xr829/umac/xradio_mac.ko
  AUTOLOAD:=$(call AutoProbe, xradio_mac xradio_core xradio_wlan)

  #FILES:=$(LINUX_DIR)/drivers/net/wireless/xr829/xr829.ko
  #AUTOLOAD:=$(call AutoProbe, xr829)
endef

define KernelPackage/net-xr829-40M/description
 Kernel modules for xr829 support
endef

$(eval $(call KernelPackage,net-xr829-40M))

define KernelPackage/net-xr829
  SUBMENU:=$(WIRELESS_MENU)
  TITLE:=xr829 support (staging)
  DEPENDS:= +xr829-firmware +@IPV6 +@USES_XR829 +@PACKAGE_xr829-rftest +@PACKAGE_xr829-rftest
  KCONFIG:=\
	CONFIG_XR829_WLAN=m \
	CONFIG_PM=y\
	CONFIG_BT=y \
	CONFIG_BT_BREDR=y \
	CONFIG_BT_RFCOMM=y \
	CONFIG_BT_RFCOMM_TTY=y \
	CONFIG_BT_DEBUGFS=y \
	CONFIG_XR_BT_LPM=y \
	CONFIG_XR_BT_FDI=y \
	CONFIG_BT_HCIUART=y \
	CONFIG_BT_HCIUART_H4=y \
	CONFIG_HFP_OVER_PCM=y \
	CONFIG_RFKILL=y \
	CONFIG_RFKILL_PM=y \
	CONFIG_RFKILL_GPIO=y

  FILES:=$(LINUX_DIR)/drivers/net/wireless/xr829/wlan/xradio_core.ko
  FILES+=$(LINUX_DIR)/drivers/net/wireless/xr829/wlan/xradio_wlan.ko
  FILES+=$(LINUX_DIR)/drivers/net/wireless/xr829/umac/xradio_mac.ko
  AUTOLOAD:=$(call AutoProbe, xradio_mac xradio_core xradio_wlan)

  #FILES+=$(LINUX_DIR)/drivers/net/wireless/xr829/xr829.ko
  #AUTOLOAD:=$(call AutoProbe, xr829)
endef

define KernelPackage/net-xr829/description
 Kernel modules for xr829 support
endef

$(eval $(call KernelPackage,net-xr829))
