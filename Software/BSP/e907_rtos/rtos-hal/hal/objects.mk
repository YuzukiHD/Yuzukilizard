#DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_EFUSE) += $(BUILD_DIR)/drivers/chip-hal/hal_efuse.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_EFUSE) += $(BUILD_DIR)/drivers/hal/source/efuse/efuse.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_EFUSE) += $(BUILD_DIR)/drivers/hal/source/efuse/hal_efuse.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_SPI) += $(BUILD_DIR)/drivers/hal/source/spi/hal_spi.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_DBI) += $(BUILD_DIR)/drivers/hal/source/dbi/hal_dbi.o $(BUILD_DIR)/drivers/hal/source/dbi/common_dbi.o $(BUILD_DIR)/drivers/hal/source/dbi/platform/dbi_sun20iw2.o

DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_SPINOR) += \
	$(BUILD_DIR)/drivers/hal/source/spinor/core.o \
	$(BUILD_DIR)/drivers/hal/source/spinor/factory_gd.o \
	$(BUILD_DIR)/drivers/hal/source/spinor/factory_mxic.o \
	$(BUILD_DIR)/drivers/hal/source/spinor/factory_winbond.o \
	$(BUILD_DIR)/drivers/hal/source/spinor/factory_esmt.o \
	$(BUILD_DIR)/drivers/hal/source/spinor/factory_xtx.o \
	$(BUILD_DIR)/drivers/hal/source/spinor/factory_fm.o \
	$(BUILD_DIR)/drivers/hal/source/spinor/factory_xmc.o \
	$(BUILD_DIR)/drivers/hal/source/spinor/factory_puya.o \
	$(BUILD_DIR)/drivers/hal/source/spinor/factory_zetta.o \
	$(BUILD_DIR)/drivers/hal/source/spinor/factory_boya.o

# FIXME
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_SPINOR) += $(BUILD_DIR)/drivers/hal/source/spinor/hal_spinor.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_FLASHC) += $(BUILD_DIR)/drivers/hal/source/spinor/hal_spinor.o

DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_FLASHC) += $(BUILD_DIR)/drivers/hal/source/flashc/hal_flash.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_FLASHC) += $(BUILD_DIR)/drivers/hal/source/flash_mcu/hal_flash_rom.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_FLASHC) += $(BUILD_DIR)/drivers/hal/source/flash_mcu/hal_flashctrl.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_FLASHC) += $(BUILD_DIR)/drivers/hal/source/flash_mcu/hal_flashctrl_rom.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_FLASHC) += $(BUILD_DIR)/drivers/hal/source/flash_mcu/hal_xip_rom.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_FLASHC) += $(BUILD_DIR)/drivers/hal/source/flash_mcu/flashchip/flash_chip.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_FLASHC) += $(BUILD_DIR)/drivers/hal/source/flash_mcu/flashchip/flash_chip_rom.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_FLASHC) += $(BUILD_DIR)/drivers/hal/source/flash_mcu/flashchip/flash_default.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_FLASHC) += $(BUILD_DIR)/drivers/hal/source/flash_mcu/flashchip/flash_EN25QHXXA.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_FLASHC) += $(BUILD_DIR)/drivers/hal/source/flash_mcu/flashchip/flash_XM25QHXXA.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_FLASHC) += $(BUILD_DIR)/drivers/hal/source/flash_mcu/flashchip/flash_P25QXXH.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_FLASHC) += $(BUILD_DIR)/drivers/hal/source/flash_mcu/flashchip/flash_XT25FXXB.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_FLASHC) += $(BUILD_DIR)/drivers/hal/source/flash_mcu/flashchip/flash_chip_cfg.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_FLASHC) += $(BUILD_DIR)/drivers/hal/source/flashc/flashc_drv.o

DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_SPINOR_CACHE) += $(BUILD_DIR)/drivers/hal/source/spinor/cache.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_SPINOR_WRITE_LOCK) += $(BUILD_DIR)/drivers/hal/source/spinor/wrlock.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_LEDC) += $(BUILD_DIR)/drivers/hal/source/ledc/hal_ledc.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_LRADC) += $(BUILD_DIR)/drivers/hal/source/lradc/hal_lradc.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_GPADC) += $(BUILD_DIR)/drivers/hal/source/gpadc/hal_gpadc.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_PWM) += $(BUILD_DIR)/drivers/hal/source/pwm/hal_pwm.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_SMARTCARD) += $(BUILD_DIR)/drivers/hal/source/smartcard/scr_hal.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_GPIO) += $(BUILD_DIR)/drivers/hal/source/gpio/hal_gpio.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_CIR) += $(BUILD_DIR)/drivers/hal/source/cir/hal_cir.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_CIR_TX) += $(BUILD_DIR)/drivers/hal/source/cir_tx/hal_cir_tx.o
ifdef CONFIG_ARCH_SUN8IW18P1
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_GPIO) += $(BUILD_DIR)/drivers/hal/source/gpio/sun8iw18/gpio-sun8iw18.o
endif
ifdef CONFIG_ARCH_SUN8IW20P1
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_GPIO) += $(BUILD_DIR)/drivers/hal/source/gpio/sun8iw20/gpio-sun8iw20.o
endif
ifdef CONFIG_ARCH_SUN20IW2P1
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_GPIO) += $(BUILD_DIR)/drivers/hal/source/gpio/sun20iw2/gpio-sun20iw2.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_PRCM) += $(BUILD_DIR)/drivers/hal/source/prcm/prcm-sun20iw2/prcm.o
endif
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_DMA) += $(BUILD_DIR)/drivers/hal/source/dma/hal_dma.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_REGULATOR) += $(BUILD_DIR)/drivers/hal/source/regulator/sun8iw18p1/core.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_THERMAL) += $(BUILD_DIR)/drivers/hal/source/thermal/hal_thermal.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_UART) += $(BUILD_DIR)/drivers/hal/source/uart/hal_uart.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_LPUART) += $(BUILD_DIR)/drivers/hal/source/lpuart/hal_lpuart.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_RTC) += $(BUILD_DIR)/drivers/hal/source/rtc/hal_rtc.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_RTC) += $(BUILD_DIR)/drivers/hal/source/rtc/rtc-lib.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_WATCHDOG) += $(BUILD_DIR)/drivers/hal/source/watchdog/hal_watchdog.o

DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_CCMU) += $(BUILD_DIR)/drivers/hal/source/ccmu/hal_clk.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_CCMU) += $(BUILD_DIR)/drivers/hal/source/ccmu/hal_reset.o

DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_SUNXI_CCU) += $(BUILD_DIR)/drivers/hal/source/ccmu/sunxi-ng/clk.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_SUNXI_CCU) += $(BUILD_DIR)/drivers/hal/source/ccmu/sunxi-ng/ccu.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_SUNXI_CCU) += $(BUILD_DIR)/drivers/hal/source/ccmu/sunxi-ng/ccu_mux.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_SUNXI_CCU) += $(BUILD_DIR)/drivers/hal/source/ccmu/sunxi-ng/ccu_nm.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_SUNXI_CCU) += $(BUILD_DIR)/drivers/hal/source/ccmu/sunxi-ng/ccu_common.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_SUNXI_CCU) += $(BUILD_DIR)/drivers/hal/source/ccmu/sunxi-ng/ccu_reset.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_SUNXI_CCU) += $(BUILD_DIR)/drivers/hal/source/ccmu/sunxi-ng/ccu_div.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_SUNXI_CCU) += $(BUILD_DIR)/drivers/hal/source/ccmu/sunxi-ng/ccu_frac.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_SUNXI_CCU) += $(BUILD_DIR)/drivers/hal/source/ccmu/sunxi-ng/ccu_gate.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_SUNXI_CCU) += $(BUILD_DIR)/drivers/hal/source/ccmu/sunxi-ng/ccu_mp.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_SUNXI_CCU) += $(BUILD_DIR)/drivers/hal/source/ccmu/sunxi-ng/ccu_mult.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_SUNXI_CCU) += $(BUILD_DIR)/drivers/hal/source/ccmu/sunxi-ng/ccu_nk.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_SUNXI_CCU) += $(BUILD_DIR)/drivers/hal/source/ccmu/sunxi-ng/ccu_nkm.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_SUNXI_CCU) += $(BUILD_DIR)/drivers/hal/source/ccmu/sunxi-ng/ccu_nkmp.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_SUNXI_CCU) += $(BUILD_DIR)/drivers/hal/source/ccmu/sunxi-ng/ccu_sdm.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_SUNXI_CCU) += $(BUILD_DIR)/drivers/hal/source/ccmu/sunxi-ng/clk-fixed-factor.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_SUNXI_CCU) += $(BUILD_DIR)/drivers/hal/source/ccmu/sunxi-ng/clk-fixed-rate.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_SUNXI_CCU) += $(BUILD_DIR)/drivers/hal/source/ccmu/sunxi-ng/clk-divider.o

DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_HWSPINLOCK) += $(BUILD_DIR)/drivers/hal/source/hwspinlock/hal_hwspinlock.o

DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_SUNXI_CLK) += $(BUILD_DIR)/drivers/hal/source/ccmu/sunxi/clk.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_SUNXI_CLK) += $(BUILD_DIR)/drivers/hal/source/ccmu/sunxi/clk_factors.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_SUNXI_CLK) += $(BUILD_DIR)/drivers/hal/source/ccmu/sunxi/clk_periph.o
ifdef CONFIG_ARCH_SUN8IW18P1
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_SUNXI_CLK) += $(BUILD_DIR)/drivers/hal/source/ccmu/sunxi/sun8iw18p1/clk_sun8iw18.o
endif
ifdef CONFIG_ARCH_SUN8IW20P1
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_SUNXI_CCU) += $(BUILD_DIR)/drivers/hal/source/ccmu/sunxi-ng/ccu-sun8iw20.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_SUNXI_CCU) += $(BUILD_DIR)/drivers/hal/source/ccmu/sunxi-ng/ccu-sun8iw20-r.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_SUNXI_CCU) += $(BUILD_DIR)/drivers/hal/source/ccmu/sunxi-ng/ccu-sun8iw20-rtc.o
endif
ifdef CONFIG_ARCH_SUN20IW2P1
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_SUNXI_CCU) += $(BUILD_DIR)/drivers/hal/source/ccmu/sunxi-ng/ccu-sun20iw2.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_SUNXI_CCU) += $(BUILD_DIR)/drivers/hal/source/ccmu/sunxi-ng/ccu-sun20iw2-r.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_SUNXI_CCU) += $(BUILD_DIR)/drivers/hal/source/ccmu/sunxi-ng/ccu-sun20iw2-aon.o
endif
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_TIMER) += $(BUILD_DIR)/drivers/hal/source/timer/hal_timer.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_TIMER) += $(BUILD_DIR)/drivers/hal/source/timer/sunxi_timer.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_TIMER) += $(BUILD_DIR)/drivers/hal/source/timer/sunxi_wuptimer.o

# sdmmc
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_SDMMC) += $(BUILD_DIR)/drivers/hal/source/sdmmc/core.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_SDMMC) += $(BUILD_DIR)/drivers/hal/source/sdmmc/hal_sdpin.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_SDMMC) += $(BUILD_DIR)/drivers/hal/source/sdmmc/hal_sdhost.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_SDMMC) += $(BUILD_DIR)/drivers/hal/source/sdmmc/mmc.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_SDMMC) += $(BUILD_DIR)/drivers/hal/source/sdmmc/quirks.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_SDMMC) += $(BUILD_DIR)/drivers/hal/source/sdmmc/sd.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_SDMMC) += $(BUILD_DIR)/drivers/hal/source/sdmmc/sdio.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_SDMMC) += $(BUILD_DIR)/drivers/hal/source/sdmmc/sdio_irq.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_SDMMC) += $(BUILD_DIR)/drivers/hal/source/sdmmc/test.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_SDMMC) += $(BUILD_DIR)/drivers/hal/source/sdmmc/cmd/cmd_sd.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_SDMMC) += $(BUILD_DIR)/drivers/hal/source/sdmmc/osal/os/FreeRTOS/os_debug.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_SDMMC) += $(BUILD_DIR)/drivers/hal/source/sdmmc/osal/os/FreeRTOS/os_mutex.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_SDMMC) += $(BUILD_DIR)/drivers/hal/source/sdmmc/osal/os/FreeRTOS/os_queue.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_SDMMC) += $(BUILD_DIR)/drivers/hal/source/sdmmc/osal/os/FreeRTOS/os_semaphore.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_SDMMC) += $(BUILD_DIR)/drivers/hal/source/sdmmc/osal/os/FreeRTOS/os_thread.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_SDMMC) += $(BUILD_DIR)/drivers/hal/source/sdmmc/osal/os/FreeRTOS/os_timer.o

DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_CE) += $(BUILD_DIR)/drivers/hal/source/ce/hal_ce.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_CE) += $(BUILD_DIR)/drivers/hal/source/ce/ce_common.o

DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_TRNG) += $(BUILD_DIR)/drivers/hal/source/trng/drv_trng.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_TRNG) += $(BUILD_DIR)/drivers/hal/source/trng/hal_trng.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_RCCAL) += $(BUILD_DIR)/drivers/hal/source/rccal/hal_rcosc_cali.o

DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_MSGBOX_SX) += $(BUILD_DIR)/drivers/hal/source/msgbox/msgbox_sx/hal_msgbox_sx.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_MSGBOX_SX) += $(BUILD_DIR)/drivers/hal/source/msgbox/msgbox_sx/msgbox_adapt.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_MSGBOX_SX) += $(BUILD_DIR)/drivers/hal/source/msgbox/msgbox_sx/msgbox_sx.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_MSGBOX_AMP) += $(BUILD_DIR)/drivers/hal/source/msgbox/msgbox_amp/msgbox_amp.o

DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_MBUS) += $(BUILD_DIR)/drivers/hal/source/mbus/hal_mbus.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_MBUS) += $(BUILD_DIR)/drivers/hal/source/mbus/mbus.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_AHB) += $(BUILD_DIR)/drivers/hal/source/mbus/ahb.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_AHB) += $(BUILD_DIR)/drivers/hal/source/mbus/hal_ahb.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_HTIMER) += $(BUILD_DIR)/drivers/hal/source/timer/hal_htimer.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_HTIMER) += $(BUILD_DIR)/drivers/hal/source/timer/sunxi_htimer.o
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_TWI) += $(BUILD_DIR)/drivers/hal/source/twi/hal_twi.o

#csi
DRIVERS_HAL_OBJECTS-$(CONFIG_DRIVERS_CSI) += $(BUILD_DIR)/drivers/hal/source/csi/hal_csi_jpeg.o
#DRIVERS_HAL_OBJECTS-$(CONFIG_CSI_CAMERA) += $(BUILD_DIR)/drivers/hal/source/csi/csi_camera/camera.o
DRIVERS_HAL_OBJECTS-$(CONFIG_CSI_CAMERA) += $(BUILD_DIR)/drivers/hal/source/csi/csi_camera/csi.o
DRIVERS_HAL_OBJECTS-$(CONFIG_CSI_CAMERA) += $(BUILD_DIR)/drivers/hal/source/csi/csi_camera/csi_reg/csi_reg.o
DRIVERS_HAL_OBJECTS-$(CONFIG_CSI_CAMERA) += $(BUILD_DIR)/drivers/hal/source/csi/csi_camera/sensor/sensor_helper.o

DRIVERS_HAL_OBJECTS-$(CONFIG_CSI_CAMERA_GC0308) += $(BUILD_DIR)/drivers/hal/source/csi/csi_camera/sensor/drv_gc0308.o
DRIVERS_HAL_OBJECTS-$(CONFIG_CSI_CAMERA_GC0328c) += $(BUILD_DIR)/drivers/hal/source/csi/csi_camera/sensor/drv_gc0328c.o
DRIVERS_HAL_OBJECTS-$(CONFIG_CSI_CAMERA_OV7670) += $(BUILD_DIR)/drivers/hal/source/csi/csi_camera/sensor/drv_ov7670.o
DRIVERS_HAL_OBJECTS-$(CONFIG_JPEG_ENCODER) += $(BUILD_DIR)/drivers/hal/source/csi/jpeg/jpegenc.o
DRIVERS_HAL_OBJECTS-$(CONFIG_JPEG_ENCODER) += $(BUILD_DIR)/drivers/hal/source/csi/jpeg/jpeglib.o
DRIVERS_HAL_OBJECTS-$(CONFIG_JPEG_ENCODER) += $(BUILD_DIR)/drivers/hal/source/csi/jpeg/hal_jpeg.o
DRIVERS_HAL_OBJECTS-$(CONFIG_JPEG_ENCODER) += $(BUILD_DIR)/drivers/hal/source/csi/jpeg/jpeg_reg/jpeg_reg.o

DRIVERS_HAL_OBJECTS-$(CONFIG_HAL_TEST_LEDC) += $(BUILD_DIR)/drivers/hal/test/ledc/test_ledc.o
DRIVERS_HAL_OBJECTS-$(CONFIG_HAL_TEST_SMARTCARD) += $(BUILD_DIR)/drivers/hal/test/smartcard/scr_test.o
DRIVERS_HAL_OBJECTS-$(CONFIG_HAL_TEST_HWSPINLOCK) += $(BUILD_DIR)/drivers/hal/test/hwspinlock/test_hwspinlock.o
DRIVERS_HAL_OBJECTS-$(CONFIG_HAL_TEST_CCU) += $(BUILD_DIR)/drivers/hal/test/ccmu/test_ng_ccmu.o
DRIVERS_HAL_OBJECTS-$(CONFIG_HAL_TEST_CCMU) += $(BUILD_DIR)/drivers/hal/test/ccmu/test_ccmu.o
DRIVERS_HAL_OBJECTS-$(CONFIG_HAL_TEST_CE) += $(BUILD_DIR)/drivers/hal/test/ce/test_ce.o
DRIVERS_HAL_OBJECTS-$(CONFIG_HAL_TEST_DMA) += $(BUILD_DIR)/drivers/hal/test/dma/test_dma.o
DRIVERS_HAL_OBJECTS-$(CONFIG_HAL_TEST_WATCHDOG) += $(BUILD_DIR)/drivers/hal/test/watchdog/test_watchdog.o
DRIVERS_HAL_OBJECTS-$(CONFIG_HAL_TEST_MBUS) += $(BUILD_DIR)/drivers/hal/test/mbus/test_mbus.o
DRIVERS_HAL_OBJECTS-$(CONFIG_HAL_TEST_AHB) += $(BUILD_DIR)/drivers/hal/test/mbus/test_ahb.o
DRIVERS_HAL_OBJECTS-$(CONFIG_HAL_TEST_GPADC) += $(BUILD_DIR)/drivers/hal/test/gpadc/test_gpadc.o
DRIVERS_HAL_OBJECTS-$(CONFIG_HAL_TEST_GPIO) += $(BUILD_DIR)/drivers/hal/test/gpio/test_gpio.o
DRIVERS_HAL_OBJECTS-$(CONFIG_HAL_TEST_PWM) += $(BUILD_DIR)/drivers/hal/test/pwm/test_pwm.o
DRIVERS_HAL_OBJECTS-$(CONFIG_HAL_TEST_EFUSE) += $(BUILD_DIR)/drivers/hal/test/efuse/test_efuse.o
DRIVERS_HAL_OBJECTS-$(CONFIG_HAL_TEST_SPI) += $(BUILD_DIR)/drivers/hal/test/spi/test_spi.o
DRIVERS_HAL_OBJECTS-$(CONFIG_HAL_TEST_DBI) += $(BUILD_DIR)/drivers/hal/test/dbi/test_dbi.o
DRIVERS_HAL_OBJECTS-$(CONFIG_HAL_TEST_SPINOR) += $(BUILD_DIR)/drivers/hal/test/spinor/test_spinor.o
DRIVERS_HAL_OBJECTS-$(CONFIG_HAL_TEST_THERMAL) += $(BUILD_DIR)/drivers/hal/test/thermal/test_thermal.o
DRIVERS_HAL_OBJECTS-$(CONFIG_HAL_TEST_TWI) += $(BUILD_DIR)/drivers/hal/test/twi/test_twi.o
DRIVERS_HAL_OBJECTS-$(CONFIG_HAL_TEST_UART) += $(BUILD_DIR)/drivers/hal/test/uart/test_uart.o
DRIVERS_HAL_OBJECTS-$(CONFIG_HAL_TEST_LPUART) += $(BUILD_DIR)/drivers/hal/test/lpuart/test_lpuart.o
DRIVERS_HAL_OBJECTS-$(CONFIG_HAL_TEST_RTC) += $(BUILD_DIR)/drivers/hal/test/rtc/test_rtc.o
DRIVERS_HAL_OBJECTS-$(CONFIG_HAL_TEST_RTC) += $(BUILD_DIR)/drivers/hal/test/rtc/tools_rtc.o
DRIVERS_HAL_OBJECTS-$(CONFIG_HAL_TEST_UDC) += $(BUILD_DIR)/drivers/hal/test/usb/udc/usb_msg.o
DRIVERS_HAL_OBJECTS-$(CONFIG_HAL_TEST_UDC) += $(BUILD_DIR)/drivers/hal/test/usb/udc/ed_test.o
DRIVERS_HAL_OBJECTS-$(CONFIG_HAL_TEST_UDC) += $(BUILD_DIR)/drivers/hal/test/usb/udc/main.o
DRIVERS_HAL_OBJECTS-$(CONFIG_HAL_TEST_TIMER) += $(BUILD_DIR)/drivers/hal/test/timer/test_timer.o
DRIVERS_HAL_OBJECTS-$(CONFIG_HAL_TEST_HCI) += $(BUILD_DIR)/drivers/hal/test/usb/host/test_hci.o
DRIVERS_HAL_OBJECTS-$(CONFIG_HAL_TEST_TRNG) += $(BUILD_DIR)/drivers/hal/test/trng/test_trng.o
DRIVERS_HAL_OBJECTS-$(CONFIG_HAL_TEST_MSGBOX) += $(BUILD_DIR)/drivers/hal/test/msgbox/test_msgbox.o
DRIVERS_HAL_OBJECTS-$(CONFIG_HAL_TEST_CSI) += $(BUILD_DIR)/drivers/hal/test/csi/test_csi_offline.o
DRIVERS_HAL_OBJECTS-$(CONFIG_HAL_TEST_CIR) += $(BUILD_DIR)/drivers/hal/test/cir/test_cir.o
DRIVERS_HAL_OBJECTS-$(CONFIG_HAL_TEST_CIR_TX) += $(BUILD_DIR)/drivers/hal/test/cir_tx/test_cir_tx.o
DRIVERS_HAL_OBJECTS-y += $(BUILD_DIR)/drivers/hal/source/common/dma_alloc.o

$(DRIVERS_HAL_OBJECTS-y):MODULE_NAME="Driver-Hal"

$(DRIVERS_HAL_OBJECTS-y):CFLAGS += -I $(BASE)/include/hal/
$(DRIVERS_HAL_OBJECTS-y):CFLAGS += -I $(BASE)/include/drivers/
$(DRIVERS_HAL_OBJECTS-y):CFLAGS += -I $(BASE)/include/freertos/
$(DRIVERS_HAL_OBJECTS-y):CFLAGS += -I $(BASE)/drivers/chip-src/uart/
$(DRIVERS_HAL_OBJECTS-y):CFLAGS += -I $(BASE)/drivers/chip-src/ccmu/

# sdmmc
$(DRIVERS_HAL_OBJECTS-y):CFLAGS += -I $(BASE)/include/hal/sdmmc/
$(DRIVERS_HAL_OBJECTS-y):CFLAGS += -I $(BASE)/include/hal/sdmmc/hal/
$(DRIVERS_HAL_OBJECTS-y):CFLAGS += -I $(BASE)/include/hal/sdmmc/sys/
$(DRIVERS_HAL_OBJECTS-y):CFLAGS += -I $(BASE)/include/hal/sdmmc/osal/
$(DRIVERS_HAL_OBJECTS-y):CFLAGS += -I $(BASE)/include/hal/sdmmc/osal/FreeRTOS/

# csi
$(DRIVERS_HAL_OBJECTS-y):CFLAGS += -I $(BASE)/components/aw/

$(DRIVERS_HAL_OBJECTS-y):CFLAGS += -I $(BASE)/drivers/hal/source/csi/include/
$(DRIVERS_HAL_OBJECTS-y):CFLAGS += -I $(BASE)/drivers/hal/source/csi/
$(DRIVERS_HAL_OBJECTS-y):CFLAGS += -I $(BASE)/drivers/hal/source/csi/csi_camera/csi_reg/
$(DRIVERS_HAL_OBJECTS-y):CFLAGS += -I $(BASE)/drivers/hal/source/csi/utility/
$(DRIVERS_HAL_OBJECTS-y):CFLAGS += -I $(BASE)/drivers/hal/source/csi/jpeg/jpeg_reg/
$(DRIVERS_HAL_OBJECTS-y):CFLAGS += -I $(BASE)/drivers/hal/include/hal/csi/

$(DRIVERS_HAL_OBJECTS-y):CFLAGS += -I $(BASE)/components/thirdparty/opus/silk
$(DRIVERS_HAL_OBJECTS-y):CFLAGS += -I $(BASE)/components/thirdparty/opus/include

ifdef CONFIG_DRIVERS_SOUND
include $(BASE)/drivers/hal/source/sound/objects.mk
endif

include $(BASE)/drivers/hal/source/nand_flash/objects.mk
include $(BASE)/drivers/hal/source/usb/objects.mk

OBJECTS += $(DRIVERS_HAL_OBJECTS-y)

