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

all:
	@echo ==================================================
	@echo build eyesee-mpp-middleware-libVideoStabilization
	@echo ==================================================
#	make -C algo/lib_ISE_EISE     -f tina.mk        all
	make -C .                     -f tina_libeis.mk all
	@echo build eyesee-mpp-middleware-libEIS done!

clean:
	@echo ==================================================
	@echo clean eyesee-mpp-middleware-libEIS
	@echo ==================================================
#	make -C algo/lib_ISE_EISE     -f tina.mk        clean
	make -C .                     -f tina_libeis.mk clean
	@echo clean eyesee-mpp-middleware-libVideoStabilization done!

