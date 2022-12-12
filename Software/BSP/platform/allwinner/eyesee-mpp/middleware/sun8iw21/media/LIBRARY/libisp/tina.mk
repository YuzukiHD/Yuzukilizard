############################################################################################
# 			eyesee-mpp-middleware for tina(OpenWrt) Linux
#
#	eyesee-mpp is designed for CDR/SDV product, focus on video/audio capturing and encoding, 
# it also can support video/audio decode.
#   eyesee-mpp-middleware provides basic libraries of mpp.
# libDisplay, etc.
#
# Version: v1.0
# Date   : 2019-1-23
# Author : PDC-PD5
############################################################################################
CUR_PATH := $(shell pwd)
include $(PACKAGE_TOP)/config/mpp_config.mk

all:
	@echo ==================================================
	@echo build eyesee-mpp-middleware-libISP
	@echo ==================================================
	make -C isp_math              -f tina.mk all
	make -C out                   -f tina.mk all
	make -C iniparser             -f tina.mk all
	make -C isp_cfg               -f tina.mk all
	make -C isp_dev               -f tina.mk all
	make -C .                     -f tina_libISP.mk all
	make -C tuning_app			  -f tina.mk all
#	make -f isp_server.mk          all
	@echo build eyesee-mpp-middleware-libISP done!

clean:
	@echo ==================================================
	@echo clean eyesee-mpp-middleware-libISP
	@echo ==================================================
	make -C isp_math              -f tina.mk clean
	make -C out                   -f tina.mk clean
	make -C iniparser             -f tina.mk clean
	make -C isp_cfg               -f tina.mk clean
	make -C isp_dev               -f tina.mk clean
	make -C .                     -f tina_libISP.mk clean
	make -C tuning_app            -f tina.mk clean
#	make -f isp_server.mk          clean
	@echo clean eyesee-mpp-middleware-libISP done!
