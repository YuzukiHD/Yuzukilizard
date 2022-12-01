#
# Copyright (C) 2015-2016 Allwinner
#
# This is free software, licensed under the GNU General Public License v2.
# See /build/LICENSE for more information.

define KernelPackage/vin-v4l2
  SUBMENU:=$(VIDEO_MENU)
  TITLE:=Video input support (staging)
  DEPENDS:=
  FILES:=$(LINUX_DIR)/drivers/media/v4l2-core/videobuf2-core.ko
  FILES+=$(LINUX_DIR)/drivers/media/v4l2-core/videobuf2-dma-contig.ko
  FILES+=$(LINUX_DIR)/drivers/media/v4l2-core/videobuf2-memops.ko
  FILES+=$(LINUX_DIR)/drivers/media/v4l2-core/videobuf2-v4l2.ko
  FILES+=$(LINUX_DIR)/drivers/media/platform/sunxi-vin/vin_io.ko
  FILES+=$(LINUX_DIR)/drivers/media/platform/sunxi-vin/modules/sensor/gc2053_mipi.ko
#  FILES+=$(LINUX_DIR)/drivers/media/platform/sunxi-vin/modules/sensor_power/sensor_power.ko
  FILES+=$(LINUX_DIR)/drivers/media/platform/sunxi-vin/vin_v4l2.ko
  AUTOLOAD:=$(call AutoProbe,videobuf2-core videobuf2-dma-contig videobuf2-memops videobuf2-v4l2 vin_io gc2053_mipi vin_v4l2)
endef

define KernelPackage/vin-v4l2/description
 Kernel modules for video input support
endef

$(eval $(call KernelPackage,vin-v4l2))

define KernelPackage/EISE-ISE
  SUBMENU:=$(VIDEO_MENU)
  TITLE:=Video ISE&EISE support (staging)
  DEPENDS:=
#  FILES:=$(LINUX_DIR)/drivers/media/platform/sunxi-ise/sunxi_ise.ko
  FILES+=$(LINUX_DIR)/drivers/media/platform/sunxi-eise/sunxi_eise.ko
  AUTOLOAD:=$(call AutoProbe,sunxi_ise sunxi_eise)
endef

define KernelPackage/EISE-ISE/description
 Kernel modules for video ISE&EISE support
endef

$(eval $(call KernelPackage,EISE-ISE))


define KernelPackage/miscellaneous
  SUBMENU:=$(OTHER_MENU)
  TITLE:=miscellaneous modules
  DEPENDS:=
  KCONFIG:=
  DEFAULT:=n
  FILES+=$(LINUX_DIR)/drivers/char/sunxi_nna_vip/vipcore.ko
  AUTOLOAD:=$(call AutoProbe,vipcore)
endef

define KernelPackage/miscellaneous/description
 miscellaneous Kernel modules, e.g. nna support
endef

$(eval $(call KernelPackage,miscellaneous))

define KernelPackage/sunxi_nna_galcore
  SUBMENU:=$(NNA_GALCORE_MENU)
  TITLE:=nna galcore modules
  DEPENDS:=
  KCONFIG:=
  DEFAULT:=n
  FILES+=$(LINUX_DIR)/drivers/char/sunxi_nna_galcore/galcore.ko
  AUTOLOAD:=$(call AutoProbe,galcore)
endef

define KernelPackage/sunxi_nna_galcore/description
 sunxi_nna_galcore Kernel modules, e.g. nna support
endef

$(eval $(call KernelPackage,sunxi_nna_galcore))

define KernelPackage/sunxi-g2d
  SUBMENU:=$(VIDEO_MENU)
  TITLE:=sunxi-g2d support
  KCONFIG:=\
	  CONFIG_SUNXI_G2D=y \
	  CONFIG_SUNXI_G2D_MIXER=y \
	  CONFIG_SUNXI_G2D_ROTATE=y \
	  CONFIG_SUNXI_SYNCFENCE=n
  FILES+=$(LINUX_DIR)/drivers/char/sunxi_g2d/g2d_sunxi.ko
  AUTOLOAD:=$(call AutoLoad,20,g2d_sunxi,1)
endef

define KernelPackage/sunxi-g2d/description
  Kernel modules for sunxi-g2d support
endef

$(eval $(call KernelPackage,sunxi-g2d))

#define KernelPackage/sunxi-rf-wlan
#  SUBMENU:=$(WIRELESS_MENU)
#  TITLE:=sunxi rfkill wlan support (staging)
#  DEPENDS:=
#  KCONFIG:= CONFIG_RFKILL \
#	  CONFIG_SUNXI_RFKILL
#  FILES:=$(LINUX_DIR)/drivers/misc/sunxi-rf/sunxi-wlan.ko
#  AUTOLOAD:=$(call AutoProbe, sunxi-rf-wlan)
#endef
#
#define KernelPackage/sunxi-rf-wlan/description
# Kernel modules for sunx-wlan support
#endef
#
#$(eval $(call KernelPackage,sunxi-rf-wlan))
#
#define KernelPackage/sunxi-rf-bluetooth
#  SUBMENU:=$(WIRELESS_MENU)
#  TITLE:=sunxi rfkill bluetooth support (staging)
#  DEPENDS:=
#  KCONFIG:= CONFIG_RFKILL \
#	  CONFIG_SUNXI_RFKILL
#  FILES:=$(LINUX_DIR)/drivers/misc/sunxi-rf/sunxi-bluetooth.ko
#  AUTOLOAD:=$(call AutoProbe, sunxi-rf-bluetooth)
#endef
#
#define KernelPackage/sunxi-rf-bluetooth/description
# Kernel modules for sunx-bluetooth support
#endef
#
#$(eval $(call KernelPackage,sunxi-rf-bluetooth))
#
#define KernelPackage/net-xr819
#  SUBMENU:=$(WIRELESS_MENU)
#  TITLE:=xr819 support (staging)
#  DEPENDS:= +kmod-sunxi-rf-wlan +kmod-cfg80211
#  FILES:=$(LINUX_DIR)/drivers/net/wireless/xr819/wlan/xradio_core.ko
#  FILES+=$(LINUX_DIR)/drivers/net/wireless/xr819/wlan/xradio_wlan.ko
#  FILES+=$(LINUX_DIR)/drivers/net/wireless/xr819/umac/xradio_mac.ko
#  AUTOLOAD:=$(call AutoProbe, xradio_mac xradio_core xradio_wlan)
#endef
#
#define KernelPackage/net-xr819/description
# Kernel modules for xr819 support
#endef
#
#$(eval $(call KernelPackage,net-xr819))
#
#define KernelPackage/touchscreen-ft6236
#  SUBMENU:=$(INPUT_MODULES_MENU)
#  TITLE:= FT6236 I2C touchscreen
#  DEPENDS:= +kmod-input-core
#  KCONFIG:= CONFIG_INPUT_TOUCHSCREEN \
#	  CONFIG_TOUCHSCREEN_PROPERTIES \
#	CONFIG_TOUCHSCREEN_FT6236
#  FILES:=$(LINUX_DIR)/drivers/input/touchscreen/ft6236.ko
#  FILES+=$(LINUX_DIR)/drivers/input/touchscreen/of_touchscreen.ko
#  AUTOLOAD:=$(call AutoProbe,of_touchscreen ft6236)
#endef
#
#define KernelPackage/touchscreen-ft6236/description
# Enable support for Focaltech 6236 touchscreen port.
#endef
#
#$(eval $(call KernelPackage,touchscreen-ft6236))
#
#define KernelPackage/sunxi-gpadc
#  SUBMENU:=$(INPUT_MODULES_MENU)
#  TITLE:= FT6236 I2C touchscreen
#  DEPENDS:= +kmod-input-core +kmod-input-evdev
#  KCONFIG:= CONFIG_INPUT_TOUCHSCREEN \
#	  CONFIG_TOUCHSCREEN_PROPERTIES \
#	CONFIG_TOUCHSCREEN_FT6236
#  FILES:=$(LINUX_DIR)/drivers/input/sensor/sunxi_gpadc.ko
#  AUTOLOAD:=$(call AutoProbe,sunxi_gpadc.ko)
#endef
#
#define KernelPackage/sunxi-gpadc/description
# Enable support for Focaltech 6236 touchscreen port.
#endef
#
#$(eval $(call KernelPackage,sunxi-gpadc))
#
#define KernelPackage/iio-mpu6xxx-i2c
#  SUBMENU:=$(IIO_MENU)
#  TITLE:=MPU6050/6500/9150  inertial measurement sensor (I2C)
#  DEPENDS:=+kmod-i2c-core +kmod-iio-core
#  KCONFIG:=CONFIG_INV_MPU6050_I2C \
#	  CONFIG_INV_MPU6050_IIO
#  FILES:=$(LINUX_DIR)/drivers/iio/imu/inv_mpu6050/inv-mpu6050.ko  \
#	  $(LINUX_DIR)/drivers/iio/imu/inv_mpu6050/inv-mpu6050-i2c.ko
#  AUTOLOAD:=$(call AutoProbe,inv-mpu6050 inv-mpu6050-i2c)
#endef
#
#define KernelPackage/iio-mpu6xxx-i2c/description
#  This driver supports the Invensense MPU6050/6500/9150 and ICM20608
#  motion tracking devices over I2C.
#  This driver can be built as a module. The module will be called
#  inv-mpu6050-i2c.
#endef
#
#$(eval $(call KernelPackage,iio-mpu6xxx-i2c))
#
#define KernelPackage/iio-mpu6xxx-spi
#  SUBMENU:=$(IIO_MENU)
#  TITLE:=MPU6050/6500/9150  inertial measurement sensor (SPI)
#  DEPENDS:=+kmod-spi-bitbang +kmod-iio-core
#  KCONFIG:=CONFIG_INV_MPU6050_SPI CONFIG_INV_MPU6050_IIO
#  FILES:=$(LINUX_DIR)/drivers/iio/imu/inv_mpu6050/inv-mpu6050.ko
#  FILES+=$(LINUX_DIR)/drivers/iio/imu/inv_mpu6050/inv-mpu6050-spi.ko
#  AUTOLOAD:=$(call AutoProbe,inv-mpu6050 inv-mpu6050-spi)
#endef
#
#define KernelPackage/iio-mpu6xxx-spi/description
#  This driver supports the Invensense MPU6050/6500/9150 and ICM20608
#  motion tracking devices over SPI.
#  This driver can be built as a module. The module will be called
#  inv-mpu6050-spi.
#endef
#
#$(eval $(call KernelPackage,iio-mpu6xxx-spi))
#
