############################################################################################
# 			eyesee-mpp-framework for tina(OpenWrt) Linux
#
#	eyesee-mpp is designed for CDR/SDV product, focus on video/audio capturing and encoding, 
# it also can support video/audio decode.
#   eyesee-mpp-framework is designed for our apps. It wraps the use of mpp-components, 
#   providing simple classes such as camera, recorder and player to apps to use.
#
# Version: v1.0
# Date   : 2019-2-18
# Author : PDC-PD5
############################################################################################
all:
	@echo ==================================================
	@echo build eyesee-mpp-framework
	@echo ==================================================
	make -C utils -f tina.mk                  all
	make -C media/camera -f tina.mk           all
ifeq ($(MPPCFG_ISE),Y)
	make -C media/ise -f tina.mk              all
endif
ifeq ($(MPPCFG_EIS),Y)
	make -C media/eis -f tina.mk              all
endif
	make -C media/recorder -f tina.mk         all
	make -C media/player -f tina.mk           all
	make -C media/thumbretriever -f tina.mk   all
	make -C media/motion -f tina.mk           all
ifeq ($(MPPCFG_UVC),Y)
	make -C media/usbcamera -f tina.mk        all
endif
ifeq ($(MPPFRAMEWORKCFG_VIDEORESIZER),Y)
	make -C media/videoresizer -f tina.mk     all
endif
ifeq ($(MPPCFG_BDII),Y)
	make -C media/bdii -f tina.mk             all
endif
#	make -C demo/sample_ADAS  -f tina.mk      all
#	make -C demo/sample_AudioEncode -f tina.mk all
#	make -C demo/sample_Camera -f tina.mk     all
#	make -C demo/sample_EncodeResolutionChange -f tina.mk all
#	make -C demo/sample_Player -f tina.mk     all
#	make -C demo/sample_RecordMultiStream -f tina.mk all
#	make -C demo/sample_OSD -f tina.mk all
#	make -C demo/sample_RecorderCallbackOut -f tina.mk all
#	make -C demo/sample_RecordSwitchFileNormal -f tina.mk all
#	make -C demo/sample_RecorderSegment -f tina.mk all
#	make -C demo/sample_RecordLowDelay -f tina.mk all
	@echo build eyesee-mpp-framework done!

clean:
	@echo ==================================================
	@echo clean eyesee-mpp-framework
	@echo ==================================================
	make -C utils -f tina.mk                  clean
	make -C media/camera -f tina.mk           clean
	make -C media/ise -f tina.mk              clean
	make -C media/eis -f tina.mk              clean
	make -C media/recorder -f tina.mk         clean
	make -C media/player -f tina.mk           clean
	make -C media/thumbretriever -f tina.mk   clean
	make -C media/motion -f tina.mk           clean
	make -C media/usbcamera -f tina.mk        clean
	make -C media/videoresizer -f tina.mk     clean
	make -C media/bdii -f tina.mk             clean
#	make -C demo/sample_ADAS  -f tina.mk	  clean
#	make -C demo/sample_AudioEncode -f tina.mk clean
#	make -C demo/sample_Camera -f tina.mk     clean
#	make -C demo/sample_EncodeResolutionChange -f tina.mk clean
#	make -C demo/sample_Player -f tina.mk     clean
#	make -C demo/sample_RecordMultiStream -f tina.mk clean
#	make -C demo/sample_OSD -f tina.mk clean
#	make -C demo/sample_RecorderCallbackOut -f tina.mk clean
#	make -C demo/sample_RecordSwitchFileNormal -f tina.mk clean
#	make -C demo/sample_RecorderSegment -f tina.mk clean
#	make -C demo/sample_RecordLowDelay -f tina.mk clean
	@echo clean eyesee-mpp-framework done!
