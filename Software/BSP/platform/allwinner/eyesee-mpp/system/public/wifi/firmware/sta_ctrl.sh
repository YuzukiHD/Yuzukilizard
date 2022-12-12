#!/bin/sh
###############################################################################
DEVICE_NAME=""
DRIVER_PATH=
CONFIG_PATH=
WORK_MODE=
OPERATE=
###############################################################################

ini_check_param ()
{
	case ${DEVICE_NAME} in
	ap6335)
		DRIVER_PATH=/lib/modules/$(uname -r)/bcmdhd.ko
		CONFIG_PATH="etc/firmware/ap6335/"
		;;
	ap6255)
		DRIVER_PATH=/lib/modules/$(uname -r)/bcmdhd.ko
		CONFIG_PATH="etc/firmware/ap6255/"
		;;
	ap6181)
		DRIVER_PATH=/lib/modules/$(uname -r)/bcmdhd.ko
		CONFIG_PATH="etc/firmware/ap6181/"
		;;
	xr819)
		DRIVER_PATH=/lib/modules/$(uname -r)/xradio_mac.ko
		DRIVER_PATH1=/lib/modules/$(uname -r)/xradio_core.ko
		DRIVER_PATH2=/lib/modules/$(uname -r)/xradio_wlan.ko
		CONFIG_PATH="etc/firmware/xr819/"
		;;
	*)
		echo "Device:${DEVICE_NAME} is not supported!"
		exit -1;
		;;
	esac
	
	case ${OPERATE} in
	load)
		;;
	unload)
		;;
	*)
		echo "Don't support this:${OPERATE} operate!"
		exit -1;
		;;
	esac
	
}


sta_load ()
{
    insmod /lib/modules/$(uname -r)/sunxi-wlan.ko
	case ${DEVICE_NAME} in
	ap6335)
		insmod ${DRIVER_PATH}
		usleep 2000
		echo "/etc/firmware/ap6335/fw_bcm4339a0_ag.bin " > /sys/module/bcmdhd/parameters/firmware_path
		echo "/etc/firmware/ap6335/nvram_ap6335.txt" >  /sys/module/bcmdhd/parameters/nvram_path
		usleep 2000
		mkdir -p /var/run/wpa_supplicant
		echo 'ctrl_interface=/var/run/wpa_supplicant' > /var/run/wpa_supplicant/wpa.conf
		#/usr/sbin/wpa_supplicant -Dnl80211 -iwlan0 -c/var/run/wpa_supplicant/wpa.conf &
		usleep 6000
		#udhcpc -i wlan0
		;;
	ap6255)
		insmod ${DRIVER_PATH}
		usleep 2000
		echo "/etc/firmware/ap6255/fw_bcm43455c0_ag.bin " > /sys/module/bcmdhd/parameters/firmware_path
		echo "/etc/firmware/ap6255/nvram_ap6255.txt" >  /sys/module/bcmdhd/parameters/nvram_path
		usleep 2000
		mkdir -p /var/run/wpa_supplicant
		echo 'ctrl_interface=/var/run/wpa_supplicant' > /var/run/wpa_supplicant/wpa.conf
		#/usr/sbin/wpa_supplicant -Dnl80211 -iwlan0 -c/var/run/wpa_supplicant/wpa.conf &
		usleep 6000
		#udhcpc -i wlan0
		;;
	ap6181)
		insmod ${DRIVER_PATH}
		usleep 2000
		echo "/etc/firmware/ap6181/fw_bcm40181a2.bin " > /sys/module/bcmdhd/parameters/firmware_path
		echo "/etc/firmware/ap6181/nvram_ap6210.txt" >  /sys/module/bcmdhd/parameters/nvram_path
		usleep 2000
		mkdir -p /var/run/wpa_supplicant
		echo 'ctrl_interface=/var/run/wpa_supplicant' > /var/run/wpa_supplicant/wpa.conf
		#/usr/sbin/wpa_supplicant -Dnl80211 -iwlan0 -c/var/run/wpa_supplicant/wpa.conf &
		usleep 6000
		#udhcpc -i wlan0
		;;
	xr819)
		#insmod ${DRIVER_PATH}
		#usleep 2000
		#echo "/etc/firmware/ap6181/fw_xr819.bin " > /sys/module/bcmdhd/parameters/firmware_path
		#echo "/etc/firmware/ap6181/nvram_ap6210.txt" >  /sys/module/bcmdhd/parameters/nvram_path
		#usleep 2000
        insmod ${DRIVER_PATH}
        usleep 2000
        insmod ${DRIVER_PATH1}
        usleep 2000
        insmod ${DRIVER_PATH2}
        usleep 2000
        mkdir -p /var/run/wpa_supplicant
		echo 'ctrl_interface=/var/run/wpa_supplicant' > /var/run/wpa_supplicant/wpa.conf
		#/usr/sbin/wpa_supplicant -Dnl80211 -iwlan0 -c/var/run/wpa_supplicant/wpa.conf &
		usleep 6000
		#udhcpc -i wlan0
		;;
	*)
		echo "Device:${DEVICE_NAME} is not supported!"
		exit -1;
		;;
	esac
}


sta_unload ()
{
	case ${DEVICE_NAME} in
	ap6255 | ap6335 | ap6181)
		killall wpa_supplicant
		usleep 8000
		ifconfig wlan0 down
		usleep 2000
		rmmod ${DRIVER_PATH}
		usleep 2000
		;;
	*)
		echo "Device:${DEVICE_NAME} is not supported!"
		exit -1;
		;;
	esac
}


sta_operate ()
{
	ini_check_param;
	
	case ${OPERATE} in
	load)
		sta_load
		;;
	unload)
		sta_unload
		;;
	*)
		echo "Don't support this:${OPERATE} operate!"
		exit -1;
		;;
	esac
}


do_help ()
{
	self=`basename $0`
	echo "Usage:"
	echo "  ./$self device operate"
	echo "    device    -  wifi device name. .e.g ap6335, rt3070, rtl8188"
	echo "    operate   -  such as load, unload"
	echo "Example:"
	echo "  ./$self ap6335 load"
	echo "  ./$self ap6335 unload"
	exit 0
}

###############################################################################
if [ $# = 2 ]; then
	DEVICE_NAME="$1"
	OPERATE="$2"
	sta_operate
else
	do_help
fi
