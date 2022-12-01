############################################################################################
# 			v851s-lizard busybox-init-base-files for tina(OpenWrt) Linux
#
#	v851s-lizard busybox-init-base-files will generate shell script according to some 
# environmental variables. so tina_busybox-init-base-files.mk is needed.
#
# Version: v1.0
# Date   : 2019-3-19
# Author : PDC-PD5
############################################################################################
TARGET_DIR := $(CURDIR)/busybox-init-base-files
HOOKS := $(CURDIR)/busybox-init-base-files_generate/rootfs_hook_squash.sh
all:
	@echo ==================================================
	@echo target/allwinner/v851s-lizard/tina_busybox-init-basefiles.mk is called to generate shell scripts
	@echo ==================================================
	(${HOOKS} ${TARGET_DIR} >/dev/null) || { \
		echo "Execute the ${HOOKS} is failed"; \
		exit 1; \
	}
	@echo generate shell scripts done!

clean:
	@echo ==================================================
	@echo target/allwinner/v851s-lizard/tina_busybox-init-basefiles.mk is called to clean shell scripts
	@echo ==================================================
#	-rm -f ${TARGET_DIR}/etc/TZ
#	-rm -rf ${TARGET_DIR}/etc/cron.d
#	-rm -f ${TARGET_DIR}/etc/init.d/S00appdriver
#	-rm -f ${TARGET_DIR}/etc/init.d/S00kfc
#	-rm -f ${TARGET_DIR}/etc/init.d/S00part
#	-rm -f ${TARGET_DIR}/etc/init.d/S00mpp
#	-rm -f ${TARGET_DIR}/etc/init.d/S01logging
#	-rm -f ${TARGET_DIR}/etc/init.d/S10udev
#	-rm -f ${TARGET_DIR}/etc/init.d/S11dev
#	-rm -f ${TARGET_DIR}/etc/init.d/S20urandom
#	-rm -f ${TARGET_DIR}/etc/init.d/S40network
#	-rm -f ${TARGET_DIR}/etc/init.d/S41netparam
#	-rm -f ${TARGET_DIR}/etc/init.d/S50telnet
#	-rm -f ${TARGET_DIR}/etc/init.d/S50usb
#	-rm -f ${TARGET_DIR}/etc/init.d/S91vm
#	-rm -f ${TARGET_DIR}/etc/init.d/rcK
#	-rm -f ${TARGET_DIR}/etc/init.d/run_adbd
#	-rm -f ${TARGET_DIR}/etc/inittab
#	-rm -f ${TARGET_DIR}/etc/preinit
#	-rm -f ${TARGET_DIR}/etc/profile
#	-rm -rf ${TARGET_DIR}/etc/sysconfig
#	-rm -f ${TARGET_DIR}/etc/udhcpd.conf
#	-rm -f ${TARGET_DIR}/usr/bin/nfsmount
#	-rm -f ${TARGET_DIR}/usr/bin/run-cron
#	-rm -rf ${TARGET_DIR}/usr/share
	@echo clean shell scripts done!
