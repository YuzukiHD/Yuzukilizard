#!/bin/bash
#
# This script is a hook and it is be run by SUPPORT_PACK_OUT_OF_TINA function
#
# You can add commands here for copy some files or resource to aw_pack_src
# directory
#
# Show aw_pack_src directory name, you should copy file to correct directory
#
#./aw_pack_src
#|--aw_pack.sh  #执行此脚本即可在aw_pack_src/out/目录生成固件
#|--config      #打包配置文件
#|--image       #各种镜像文件，可替换，但不能改文件名
#|    |--boot0_nand.fex        #nand介质boot0镜像
#|    |--boot0_sdcard.fex      #SD卡boot0镜像
#|    |--boot0_spinor.fex      #nor介质boot0镜像
#|    |--boot0_spinor.fex      #nor介质boot0镜像
#|    |--boot_package.fex      #nand和SD卡uboot镜像
#|    |--boot_package_nor.fex  #nor介质uboot镜像
#|    |--env.fex               #env环境变量镜像
#|    |--boot.fex              #内核镜像
#|    |--rootfs.fex            #rootfs镜像
#|--other       #打包所需的其他文件
#|--out         #固件生成目录
#|--tmp         #打包使用的临时目录
#|--tools       #工具
#|--rootfs      #存放rootfs的tar.gz打包,给二次修改使用
#|--lib_aw      #拷贝全志方案的库文件，如多媒体组件eyesempp等,
#		给应用app编译链接使用(没有选择这些库，则可能是空文件).
#|--README      #关于板级方案的一些说明，例如分区布局等等(无说明则没有这个文件)

#
#NOTE: input parameter $1 is a path of aw_pack_src
#

#author: wuguanling@allwinnertech.com


#This script of this project need to copy eyesempp lib to aw_pack_src/lib_aw
if [ -d ${LICHEE_PACK_OUT_DIR}/../staging_dir/target/usr/lib/eyesee-mpp ]; then
	cp -rf  ${LICHEE_PACK_OUT_DIR}/../staging_dir/target/usr/lib/eyesee-mpp $1/lib_aw/lib
	cp -rf  ${LICHEE_PACK_OUT_DIR}/../staging_dir/target/usr/include/eyesee-mpp  $1/lib_aw/include
fi
