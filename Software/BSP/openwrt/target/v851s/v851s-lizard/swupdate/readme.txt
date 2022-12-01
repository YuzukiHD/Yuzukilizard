OTA说明：

一、recovery系统升级
1、此目录的文件是OTA配置文件

2、sw-subimgs.cfg和sw-description文件是用于nor介质的recovery系统升级。
生成OTA包用命令：swupdate_pack_swu 不加参数

3、sw-subimgs-ubi.cfg和sw-description-ubi文件是用于nand介质的recovery系统升级。
生成OTA包使用命令：swupdate_pack_swu -ubi

4、sw-subimgs-emmc.cfg和sw-description-emmc文件是用于emmc介质的recovery系统升级。
生成OTA包使用命令：swupdate_pack_swu -emmc

二、AB系统升级
1、配置AB系统。将本目录的env_ab.cfg, sys_partition_ab.fex文件复制到device/config/chips/v853/configs/vision目录，
并分别替换为env.cfg，sys_partition.fex。打包：p; 然后烧录，此时小机端为A/B系统

2、sw-subimgs-ab.cfg和sw-description-ab文件是用于nand介质的AB系统升级。
生成OTA包使用命令：swupdate_pack_swu -ab
