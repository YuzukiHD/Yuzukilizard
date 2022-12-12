############################################################################################
# 			libcedarc in eyesee-mpp for tina(OpenWrt) Linux
#
#	compile libcedarc in eyesee-mpp of tina(OpenWrt).
#
# Version: v1.0
# Date   : 2019-3-11
# Author : PDC-PD5
############################################################################################
include $(PACKAGE_TOP)/config/mpp_config.mk

all:
	@echo ==================================================
	@echo build eyesee-mpp-middleware-libcedarc
	@echo ==================================================
#libcedarc debug: ve, vencoder/libcodec/build.mk, vdecoder/videoengine, vdecoder/videoengine/h264, vdecoder/videoengine/h265, vdecoder/videoengine/mjpeg
#libcedarc release: library.
	make -C base                    -f tina.mk  all
##		make -C ve                      -f tina.mk  all
	make -C library                 -f tina.mk  all
	make -C memory                  -f tina.mk  all
ifeq ($(MPPCFG_VENC),Y)
	make -C vencoder/base           -f tina.mk  all
##		make -C vencoder/libcodec               -f tina.mk  all
	make -C vencoder                -f tina.mk  all
endif
ifeq ($(MPPCFG_VDEC),Y)
##		make -C vdecoder/videoengine            -f tina.mk  all
	make -C vdecoder                -f tina.mk  all
##		make -C vdecoder/videoengine/h264       -f tina.mk  all
##		make -C vdecoder/videoengine/h265       -f tina.mk  all
##		make -C vdecoder/videoengine/mjpeg  -f tina.mk  all
endif
	@echo build eyesee-mpp-middleware-libcedarc done!

clean:
	@echo ==================================================
	@echo clean eyesee-mpp-middleware-libcedarc
	@echo ==================================================
#libcedarc debug: ve, vencoder/libcodec/build.mk, vdecoder/videoengine, vdecoder/videoengine/h264, vdecoder/videoengine/h265, vdecoder/videoengine/mjpeg
#libcedarc release: library.
	make -C base     -f tina.mk     clean
##		make -C ve       -f tina.mk     clean
	make -C library -f tina.mk  clean
	make -C memory -f tina.mk   clean
	make -C vencoder/base -f tina.mk clean
##		make -C vencoder/libcodec           -f tina.mk  clean
	make -C vencoder -f tina.mk clean
##		make -C vdecoder/videoengine        -f tina.mk  clean
	make -C vdecoder -f tina.mk clean
##		make -C vdecoder/videoengine/h264       -f tina.mk  clean
##		make -C vdecoder/videoengine/h265       -f tina.mk  clean
##		make -C vdecoder/videoengine/mjpeg  -f tina.mk  clean
	@echo clean eyesee-mpp-middleware-libcedarc done!
