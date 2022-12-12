############################################################################################
# 			eyesee-mpp-external for tina(OpenWrt) Linux
#
#	eyesee-mpp is designed for CDR/SDV product, focus on video/audio capturing and encoding, 
# it also can support video/audio decode.
#   eyesee-mpp-external contains some external libs' source code.
#
# Version: v1.0
# Date   : 2019-3-5
# Author : PDC-PD5
############################################################################################
all:
	@echo ==================================================
	@echo build eyesee-mpp-external
	@echo ==================================================
ifeq ($(MPPEXTERNALCFG_SQLITE), Y)
	make -C SQLiteCpp    -f tina.mk     all
endif
ifeq ($(MPPEXTERNALCFG_CIVETWEB), Y)
	make -C civetweb     -f tina.mk     all
endif
ifeq ($(MPPEXTERNALCFG_LZ4), Y)
	make -C lz4-1.7.5    -f tina.mk     all
endif
#	make -C fsck_msdos   -f tina.mk     all
ifeq ($(MPPEXTERNALCFG_UVOICE), Y)
	make -C uvoice       -f tina.mk     all
endif
ifeq ($(MPPEXTERNALCFG_JSONCPP), Y)
	make -C jsoncpp-0.8.0       -f tina.mk     all
endif
ifeq ($(MPPEXTERNALCFG_SOUND_CTR), Y)
	make -C sound_controler       -f tina.mk     all
endif
	@echo build eyesee-mpp-external done!

clean:
	@echo ==================================================
	@echo clean eyesee-mpp-external
	@echo ==================================================
	make -C SQLiteCpp    -f tina.mk     clean
	make -C civetweb     -f tina.mk     clean
	make -C lz4-1.7.5    -f tina.mk     clean
#	make -C fsck_msdos   -f tina.mk     clean
	make -C uvoice       -f tina.mk     clean
	make -C jsoncpp-0.8.0       -f tina.mk     clean
	make -C sound_controler       -f tina.mk     clean
	@echo clean eyesee-mpp-external done!
