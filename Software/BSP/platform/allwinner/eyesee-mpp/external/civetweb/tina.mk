############################################################################################
# 			eyesee-mpp-external-civetweb for tina(OpenWrt) Linux
#
#
# Version: v1.0
# Date   : 2019-3-5
# Author : PDC-PD5
############################################################################################
all:
	@echo ==================================================
	@echo build eyesee-mpp-external-civetweb
	@echo ==================================================
	make -f tina_lib.mk     all
	make -f tina_bin.mk     all
	@echo build eyesee-mpp-external-civetweb done!

clean:
	@echo ==================================================
	@echo clean eyesee-mpp-external-civetweb
	@echo ==================================================
	make -f tina_lib.mk     clean
	make -f tina_bin.mk     clean
	@echo clean eyesee-mpp-external-civetweb done!
