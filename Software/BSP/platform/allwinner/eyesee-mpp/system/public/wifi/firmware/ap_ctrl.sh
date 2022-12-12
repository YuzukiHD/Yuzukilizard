#!/bin/sh
###############################################################################
DEVICE_NAME=""
DRIVER_PATH=
DRIVER_PATH1=
DRIVER_PATH2=
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
	8189ftv)
		DRIVER_PATH=/lib/modules/$(uname -r)/8189fs.ko
		CONFIG_PATH="etc/firmware/8189ftv/"
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


ap_load ()
{
    # insmod /lib/modules/$(uname -r)/sunxi-wlan.ko
	case ${DEVICE_NAME} in
	ap6335)
		insmod ${DRIVER_PATH}
		usleep 2000
		echo "/etc/firmware/ap6335/fw_bcm4339a0_ag_apsta.bin " > /sys/module/bcmdhd/parameters/firmware_path
		echo "/etc/firmware/ap6335/nvram_ap6335.txt" >  /sys/module/bcmdhd/parameters/nvram_path
		mkdir -p /var/run/hostapd/
		touch /var/run/udhcpd.leases
		usleep 2000
		ifconfig wlan0 up
		usleep 2000
		;;
	ap6255)
		insmod ${DRIVER_PATH}
		usleep 2000
		echo "/etc/firmware/ap6255/fw_bcm43455c0_ag_apsta.bin " > /sys/module/bcmdhd/parameters/firmware_path
		echo "/etc/firmware/ap6255/nvram_ap6255.txt" >  /sys/module/bcmdhd/parameters/nvram_path
		mkdir -p /var/run/hostapd/
		touch /var/run/udhcpd.leases
		usleep 2000
		ifconfig wlan0 up
		usleep 2000
		;;
	ap6181)
		insmod ${DRIVER_PATH}
		usleep 2000
		echo "/etc/firmware/ap6181/fw_bcm40181a2_apsta.bin " > /sys/module/bcmdhd/parameters/firmware_path
		echo "/etc/firmware/ap6181/nvram_ap6210.txt" >  /sys/module/bcmdhd/parameters/nvram_path
		mkdir -p /var/run/hostapd/
		touch /var/run/udhcpd.leases
		usleep 2000
		ifconfig wlan0 up
		usleep 2000
		;;
	8189ftv)
		insmod ${DRIVER_PATH}
		# usleep 2000
		mkdir -p /var/run/hostapd/
		touch /var/run/udhcpd.leases
		# usleep 2000
		# ifconfig wlan0 up
		usleep 2000
		;;
    xr819)
        insmod ${DRIVER_PATH}
        usleep 2000
        insmod ${DRIVER_PATH1}
        usleep 2000
        insmod ${DRIVER_PATH2}
        usleep 2000
        mkdir -p /var/run/hostapd/
        touch /var/run/udhcpd.leases
        usleep 2000
        ;;
	*)
		echo "Device:${DEVICE_NAME} is not supported!"
		exit -1;
		;;
	esac
}


ap_unload ()
{
	case ${DEVICE_NAME} in
	ap6255 | ap6335 | ap6181 | 8189ftv)
		# ifconfig wlan0 down
		# usleep 6000
		rmmod ${DRIVER_PATH}
		usleep 2000
		;;
    xr819)
        rmmod ${DRIVER_PATH2}
        usleep 2000
        rmmod ${DRIVER_PATH1}
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


ap_operate ()
{
	ini_check_param;
	
	case ${OPERATE} in
	load)
		ap_load
		;;
	unload)
		ap_unload
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
	ap_operate
else
	do_help
fi
