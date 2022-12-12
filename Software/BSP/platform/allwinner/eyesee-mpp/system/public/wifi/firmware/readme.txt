
固件和驱动方面: ap6255 和 ap6335 的驱动是同一个. 区别在于两个芯片的固件是不同的.

配置方面:  ap6255 和 ap6335 在station模式下, wpa_supplicant操作是相同的. 主要区别在于AP模式下的HOSTAPD配置:
           ignore_broadcast_ssid=0
           wpa=2
           rsn_pairwise=CCMP


///////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////  ap6255  station 模式设置  //////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

step 1: 加载驱动
insmod /lib/modules/3.10.65/bcmdhd.ko

step 2: 设置固件路径 并 察看设置结果
echo "/etc/firmware/ap6255/fw_bcm43455c0_ag.bin " > /sys/module/bcmdhd/parameters/firmware_path
echo "/etc/firmware/ap6255/nvram_ap6255.txt" >  /sys/module/bcmdhd/parameters/nvram_path

cat /sys/module/bcmdhd/parameters/firmware_path
cat /sys/module/bcmdhd/parameters/nvram_path

step 3: 启动无线网卡并配置wpa_supplicant
ifconfig wlan0 up

mkdir -p /tmp/run/wpa_supplicant
echo 'ctrl_interface=/tmp/run/wpa_supplicant' > /tmp/run/wpa_supplicant/wpa.conf
wpa_passphrase "PD2-IPC-test" "12345678"    >> /tmp/run/wpa_supplicant/wpa.conf
wpa_passphrase "AP19" "12345678"    >> /tmp/run/wpa_supplicant/wpa.conf
wpa_passphrase "guixingHUAWEI8" "12345678"    >> /tmp/run/wpa_supplicant/wpa.conf
wpa_passphrase "BU3-IPC-AP-5G" "awt.1235"    >> /tmp/run/wpa_supplicant/wpa.conf
wpa_passphrase "AW2" "1qaz@WSX"    >> /tmp/run/wpa_supplicant/wpa.conf

注意:以上配置中的ssid和密码任选其一.

wpa_supplicant -Dnl80211 -iwlan0 -c/tmp/run/wpa_supplicant/wpa.conf &

udhcpc -i wlan0

step 4: 启动wpa客户端,进行其它操作,如搜索热点列表等
wpa_cli -iwlan0 -p /tmp/run/wpa_supplicant 


///////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////  ap6255  AP 模式设置  ///////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

step 1: 加载驱动
insmod /lib/modules/3.10.65/bcmdhd.ko

step 2: 设置固件路径 并 察看设置结果
echo "/etc/firmware/ap6255/fw_bcm43455c0_ag_apsta.bin " > /sys/module/bcmdhd/parameters/firmware_path
echo "/etc/firmware/ap6255/nvram_ap6255.txt" >  /sys/module/bcmdhd/parameters/nvram_path

cat /sys/module/bcmdhd/parameters/firmware_path
cat /sys/module/bcmdhd/parameters/nvram_path

step 3: 启动并配置hostapd
/* 以下为 2.4G 模式 802.11g协议 */
mkdir -p /var/run/hostapd/
echo "interface=wlan0" >   /var/run/hostapd/hostapd.conf
echo "driver=nl80211" >>  /var/run/hostapd/hostapd.conf
echo "ctrl_interface=/var/run/hostapd" >>  /var/run/hostapd/hostapd.conf
echo "ssid=ipc_guixing" >>  /var/run/hostapd/hostapd.conf
echo "channel=6" >>  /var/run/hostapd/hostapd.conf
echo "hw_mode=g" >>  /var/run/hostapd/hostapd.conf
echo "ieee80211n=1" >>  /var/run/hostapd/hostapd.conf
echo "ignore_broadcast_ssid=0" >>  /var/run/hostapd/hostapd.conf
echo "wpa=2" >>  /var/run/hostapd/hostapd.conf
echo "rsn_pairwise=CCMP" >>  /var/run/hostapd/hostapd.conf
echo "wpa_passphrase=12345678" >>  /var/run/hostapd/hostapd.conf

/* 以下为 5G 模式 802.11g协议 */
mkdir -p /var/run/hostapd/
echo "interface=wlan0" >   /var/run/hostapd/hostapd.conf
echo "driver=nl80211" >>  /var/run/hostapd/hostapd.conf
echo "ctrl_interface=/var/run/hostapd" >>  /var/run/hostapd/hostapd.conf
echo "ssid=ipc_guixing" >>  /var/run/hostapd/hostapd.conf
echo "channel=149" >>  /var/run/hostapd/hostapd.conf
echo "hw_mode=a" >>  /var/run/hostapd/hostapd.conf
echo "ieee80211n=1" >>  /var/run/hostapd/hostapd.conf
echo "ignore_broadcast_ssid=0" >>  /var/run/hostapd/hostapd.conf
echo "wpa=2" >>  /var/run/hostapd/hostapd.conf
echo "rsn_pairwise=CCMP" >>  /var/run/hostapd/hostapd.conf
echo "wpa_passphrase=12345678" >>  /var/run/hostapd/hostapd.conf

hostapd -B /var/run/hostapd/hostapd.conf

killall udhcpc

ifconfig wlan0 192.168.10.1

udhcpd /etc/udhcpd.conf



///////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////  ap6335/ap6255 监听模式 和 信道切换实验  /////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

killall udhcpc

ifconfig eth0 down

mount -t nfs -o nolock -o tcp  192.168.205.200:/nfs/wangguixing/v40_ipc  /mnt/extsd ;

cp -rf /mnt/extsd/dhd_priv  /tmp/

cp -rf /mnt/extsd/raw_read  /tmp/

insmod /lib/modules/3.10.65/bcmdhd.ko

//set ap6335 chip, if ap6255 chip need load different firmware.
echo "/etc/firmware/ap6335/fw_bcm4339a0_ag.bin " > /sys/module/bcmdhd/parameters/firmware_path
echo "/etc/firmware/ap6335/nvram_ap6335.txt" >  /sys/module/bcmdhd/parameters/nvram_path

cat /sys/module/bcmdhd/parameters/firmware_path
cat /sys/module/bcmdhd/parameters/nvram_path

ifconfig wlan0 up

// change channel
./dhd_priv set_channel 3    

// setting monitor mode, 1:enable 0:disable
./dhd_priv monitor 1

./dhd_priv monitor 0


// read raw date for socket
./raw_read > /mnt/extsd/info_ch3.txt



///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////  bt6355 easy_steup test  ///////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
mount -t nfs -o nolock -o tcp  192.168.205.200:/nfs/wangguixing/v40_ipc  /mnt/extsd ;

cp -rf /mnt/extsd/wifi_smartlink/fw_bcmdhd-4339a0-easy_steup.bin /tmp/fw_bcm4339a0_ag.bin
cp -rf /mnt/extsd/wifi_smartlink/setup /tmp

cd /tmp



insmod /lib/modules/3.10.65/bcmdhd.ko

echo "/tmp/fw_bcm4339a0_ag.bin" > /sys/module/bcmdhd/parameters/firmware_path
echo "/etc/firmware/ap6335/nvram_ap6335.txt" >  /sys/module/bcmdhd/parameters/nvram_path

cat /sys/module/bcmdhd/parameters/firmware_path
cat /sys/module/bcmdhd/parameters/nvram_path


ifconfig wlan0 up

