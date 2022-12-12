############################################################################################
#            eyesee-mpp-system-public-wifi for tina(OpenWrt) Linux
#
# Version: v1.0
# Date   : 2019-3-14
# Author : PDC-PD5
############################################################################################
all:
	@echo ==================================================
	@echo build eyesee-mpp-system-public-wifi
	@echo ==================================================
	make -f tina_sta.mk               all
	make -f tina_ap.mk                all
	@echo build eyesee-mpp-system-public-wifi done!

clean:
	@echo ==================================================
	@echo clean eyesee-mpp-system-public-wifi
	@echo ==================================================
	make -f tina_sta.mk               clean
	make -f tina_ap.mk                clean
	@echo clean eyesee-mpp-system-public-wifi done!

