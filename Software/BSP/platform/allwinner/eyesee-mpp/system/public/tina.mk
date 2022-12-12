############################################################################################
# 			eyesee-mpp-system for tina(OpenWrt) Linux
#
#	eyesee-mpp is designed for CDR/SDV product, focus on video/audio capturing and encoding, 
# it also can support video/audio decode.
#   eyesee-mpp-system is lower level for providing basic libraries such as libion, liblog,
# libDisplay, etc.
#
# Version: v1.0
# Date   : 2019-1-18
# Author : PDC-PD5
############################################################################################

all:
	@echo ==================================================
	@echo build eyesee-mpp-system
	@echo ==================================================
	make -C liblog -f tina.mk              all
#	make -C logcat -f tina.mk              all
	make -C libion -f tina.mk              all
#	make -C libcutils -f tina.mk            all
ifeq ($(MPPCFG_VO), Y)
	make -C display -f tina.mk             all
endif
ifeq ($(MPPSYSTEMCFG_NEWFS_MSDOS), Y)
	make -C newfs_msdos -f tina.mk         all
endif
ifeq ($(MPPSYSTEMCFG_REBOOT_EFEX), Y)
	make -C reboot_efex -f tina.mk         all
endif
ifeq ($(MPPSYSTEMCFG_LUACONFIG), Y)
	make -C luaconfig -f tina.mk           all
endif
ifeq ($(MPPSYSTEMCFG_WIFI), Y)
	make -C wifi/wpa_supplicant -f tina.mk all
	make -C wifi -f tina.mk      all
endif
ifeq ($(MPPSYSTEMCFG_SMARTLINK), Y)
	make -C smartlink -f tina.mk           all
endif
ifeq ($(MPPSYSTEMCFG_RGB_CTRL), Y)
	make -C rgb_ctrl -f tina.mk            all
endif
ifeq ($(MPPSYSTEMCFG_NTPCLIENT), Y)
	make -C ntpclient -f tina.mk           all
endif
	@echo build eyesee-mpp-system done!

clean:
	@echo ==================================================
	@echo clean eyesee-mpp-system
	@echo ==================================================
	make -C liblog -f tina.mk              clean
#	make -C logcat -f tina.mk              clean
	make -C libion -f tina.mk              clean
#	make -C libcutils -f tina.mk           clean
	make -C display -f tina.mk             clean

	make -C newfs_msdos -f tina.mk         clean
	make -C reboot_efex -f tina.mk         clean
	make -C luaconfig -f tina.mk           clean
	make -C wifi/wpa_supplicant -f tina.mk clean
	make -C wifi -f tina.mk -f tina.mk     clean
	make -C smartlink -f tina.mk           clean
	make -C rgb_ctrl -f tina.mk            clean
	make -C ntpclient -f tina.mk           clean
	@echo clean eyesee-mpp-system done!
