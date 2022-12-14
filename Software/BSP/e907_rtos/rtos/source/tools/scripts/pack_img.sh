#!/bin/bash
#
# pack/pack
# (c) Copyright 2013 - 2016 Allwinner
# Allwinner Technology Co., Ltd. <www.allwinnertech.com>
# James Deng <csjamesdeng@allwinnertech.com>
# Trace Wong <wangyaliang@allwinnertech.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
# If you see the following export command, do not change the sequence of
#
#
# the PATH, we must put our pack routine at the first, to avoid customer's
#     runing environment has some excutes which have the same name with out tools.
export PATH=${MELIS_BASE}/tools/packtool/:$PATH
############################ Notice #####################################
# a. Some config files priority is as follows:
#    - xxx_${platform}.{cfg|fex} > xxx.{cfg|fex}
#    - ${chip}/${board}/*.{cfg|fex} > ${chip}/default/*.{cfg|fex}
#    - ${chip}/default/*.cfg > common/imagecfg/*.cfg
#    - ${chip}/default/*.fex > common/partition/*.fex

source ${MELIS_BASE}/tools/scripts/shflags

SUPPORT_8M=1
SUPPORT_16M=0
SUPPORT_64M=0

enable_pause=0
align_size=256
fs_align_size=256

# define option, format:
#   'long option' 'default value' 'help message' 'short option'
DEFINE_string 'chip' '' 'chip to build, e.g. sun7i' 'c'
DEFINE_string 'platform' '' 'platform to build, e.g. linux, android, camdroid' 'p'
DEFINE_string 'board' '' 'board to build, e.g. evb' 'b'
DEFINE_string 'debug_mode' 'uart0' 'config debug mode, e.g. uart0, card0' 'd'
DEFINE_string 'signture' 'none' 'pack boot signture to do secure boot' 's'
DEFINE_string 'secure' 'none' 'pack secure boot with -v arg' 'v'
DEFINE_string 'mode' 'normal' 'pack dump firmware' 'm'
DEFINE_string 'topdir' 'none' 'sdk top dir' 't'
DEFINE_string 'programmer' '' 'creat programmer img or not' 'w'
DEFINE_string 'tar_image' '' 'creat downloadfile img .tar.gz or not' 'i'
DEFINE_string 'nor_volume' '' 'pack norflash volume , e.g. 8(MBytes)' 'n'
DEFINE_string 'storage_type' '' 'pack storage type , e.g. nor, sdcard, sdcard_product, nand' 'a'

# parse the command-line
FLAGS "$@" || exit $?
eval set -- "${FLAGS_ARGV}"

PACK_CHIP=${FLAGS_chip}
PACK_PLATFORM=${FLAGS_platform}
PACK_BOARD=${FLAGS_board}
PACK_DEBUG=${FLAGS_debug_mode}
PACK_SIG=${FLAGS_signture}
PACK_SECURE=${FLAGS_secure}
PACK_MODE=${FLAGS_mode}
PACK_PROGRAMMER=${FLAGS_programmer}
PACK_TAR_IMAGE=${FLAGS_tar_image}
PACK_TOPDIR=${FLAGS_topdir}
PACK_NORVOL=${FLAGS_nor_volume}
PACK_STORAGE_TYPE=${FLAGS_storage_type}
MULTI_CONFIG_INDEX=0
UART_PORT=$(cat ${PACK_TOPDIR}/projects/${PACK_BOARD}/configs/*_nor.fex | fgrep -w "uart_debug_port" | fgrep -v ";" | sed s/[[:space:]]//g | awk -F'=' '{ print  $2 }')

#if uart_port is not exit or uboot_nor_uartx.bin is nor exit. set uart is defult port
if [ ! $UART_PORT -o ! -f "${PACK_TOPDIR}/projects/${PACK_BOARD}/bin/u-boot_${PACK_CHIP}_nor_uart${UART_PORT}.bin" ];then
    UART_PORT=0
fi

# the size for user space is 114432 KB for nand
# the mbr size is 256K default
LOGICAL_PARTS_KB_FOR_128Mnand=$(( 114432 - 256 ))
LOGICAL_UDISK_PARTS_KB_REMAIN_FOR_128Mnand=0

LOGICAL_PARTS_KB_FOR_64M=$(( 64 * 1024 - 64))
LOGICAL_PARTS_KB_FOR_16M=$(( 16 * 1024 - 64))
LOGICAL_PARTS_KB_FOR_8M=$(( 8 * 1024 - 64))
LOGICAL_UDISK_PARTS_KB_REMAIN_FOR_64M=0
LOGICAL_UDISK_PARTS_KB_REMAIN_FOR_16M=0
LOGICAL_UDISK_PARTS_KB_REMAIN_FOR_8M=0
current_rtos_full_img_size=0

SUFFIX=""
storage_type=""
IMG_NAME=""

phoenixplugin_file_list=(
${PACK_TOPDIR}/tools/phoenixplugin/*.fex
)

configs_file_list=(
${PACK_TOPDIR}/projects/${PACK_BOARD}/configs/*.fex
${PACK_TOPDIR}/projects/${PACK_BOARD}/configs/*.cfg
${PACK_TOPDIR}/projects/${PACK_BOARD}/configs/rootfs.ini
${PACK_TOPDIR}/projects/${PACK_BOARD}/version/version_base.mk
)

img_storage_type=("nor" "card" "card_product" "nand" )

boot_file_list=(
${PACK_TOPDIR}/projects/${PACK_BOARD}/bin/boot0_${PACK_CHIP}_nand.bin:boot0_nand.fex
${PACK_TOPDIR}/projects/${PACK_BOARD}/bin/boot0_${PACK_CHIP}_nor.bin:boot0_nor.fex
${PACK_TOPDIR}/projects/${PACK_BOARD}/bin/boot0_${PACK_CHIP}_card.bin:boot0_card.fex
${PACK_TOPDIR}/projects/${PACK_BOARD}/bin/boot0_${PACK_CHIP}_card.bin:boot0_card_product.fex
${PACK_TOPDIR}/projects/${PACK_BOARD}/bin/fes1_${PACK_CHIP}.bin:fes1.fex
${PACK_TOPDIR}/projects/${PACK_BOARD}/bin/u-boot_${PACK_CHIP}_nand.bin:u-boot_nand.fex
${PACK_TOPDIR}/projects/${PACK_BOARD}/bin/u-boot_${PACK_CHIP}_card.bin:u-boot_card.fex
${PACK_TOPDIR}/projects/${PACK_BOARD}/bin/u-boot_${PACK_CHIP}_nor_uart${UART_PORT}.bin:u-boot_nor.fex
${PACK_TOPDIR}/projects/${PACK_BOARD}/epos.img:epos.img
)

boot_file_secure=(${PACK_TOPDIR}/projects/${PACK_BOARD}/bin/sboot_${PACK_CHIP}.bin:sboot.bin)

function get_char()
{
	SAVEDSTTY=`stty -g`
	stty -echo
	stty cbreak
	dd if=/dev/tty bs=1 count=1 2> /dev/null
	stty -raw
	stty echo
	stty $SAVEDSTTY
}

function pause()
{
	if [ "x$1" != "x" ] ;then
		echo $1
	fi
	if [ $enable_pause -eq 1 ] ; then
		echo "Press any key to continue!"
		char=`get_char`
	fi
}

function pack_error()
{
	echo -e "\033[47;31mERROR: $*\033[0m"
}

function pack_warn()
{
	echo -e "\033[47;34mWARN: $*\033[0m"
}

function pack_info()
{
	echo -e "\033[47;30mINFO: $*\033[0m"
}

function size() {
        ls -l $1 | awk '{print $5}'
}
# get_part_info <totol_KB_for_all_partitions> <path_of_sys_partition>
function get_part_info()
{
    sed -e '/^;/d' -e '/^ *;/d' -e 's/\r//g' $1 | awk -v total=$2 '
    BEGIN {
        i = 0
    }
    /^\[partition\]/ {
        info["name"] = "None"
        info["size"] = 0
        info["downloadfile"] = "\"None\""

        while (getline && $0 ~ /=/)
        {
            info[$1] = $3
        }
        info["size"] /= 2
        if (info["name"] != "UDISK")
            sum += info["size"]
        if (info["name"] == "UDISK")
            info["size"] = (int( total - sum))
        info[i] = info["name"] ":" info["size"] ":" info["downloadfile"]
        i++
    };
    END {
        for (j = 0; j < i; j++)
            print info[j]
    }'
}

function get_partition_downfile_size()
{
    local downloadfile_name=`echo $1 | awk -F '=' '{print $2}'`
    if [ ! -f ${downloadfile_name} ]; then
        echo "  file ${downloadfile_name} not find"
    else
        if [ -L ${downloadfile_name} ]; then
            local downloadfile_name_link=`readlink -f ${downloadfile_name}`
            local linkfile_name=${downloadfile_name_link##*/}
            echo "  ${downloadfile_name} -> ${downloadfile_name_link}"
            if [ ! -f ${downloadfile_name_link} ]; then
                echo "  link file ${linkfile_name} not find"
            else
                local linkfile_size=`ls -lh ${downloadfile_name_link} | awk '{print $5}'`
                echo "  ${linkfile_name} size : ${linkfile_size} byte"
            fi
        else
            local downloadfile_size=`ls -lh ${downloadfile_name} | awk '{print $5}'`
            echo "  ${downloadfile_name} size : ${downloadfile_size} byte"
        fi
    fi
}

function get_partition_mbr_size()
{
    local partition_size_name=`echo $1 | awk -F '=' '{print $1}' | sed 's/partition/mbr/g'`
    local partition_size=`echo $1 | awk -F '=' '{print $2}'`
    echo "  ${partition_size_name}  : ${partition_size} Kbyte"
}

function show_partition_message()
{
    grep -c '[mbr]' $1 > /dev/null
    if [ $? -eq 0 ]; then
        cp $1 ./show_sys_partition.tmp;
        sed -i '/^[\r;]/d' ./show_sys_partition.tmp;
        sed -i '/partition_start/d' ./show_sys_partition.tmp;
        sed -i '/user_type/d' ./show_sys_partition.tmp;
        sed -i 's/\[partition\]/------------------------------------/g' ./show_sys_partition.tmp;
        sed -i 's/[ "\r]//g' ./show_sys_partition.tmp;
        sed -i '/^[;]/d' ./show_sys_partition.tmp;
        sed -i 's/name/partition_name/g' ./show_sys_partition.tmp;
        sed -i 's/size/partition_size/g' ./show_sys_partition.tmp;
        echo ------------------------------------
        while read line
        do
            if [ "$line" == "------------------------------------" ];then
                echo $line
            else
                echo "  $line" | sed 's/=/  : /g'
                echo "  $line" | grep "mbr" >> /dev/null
                if [ $? -eq 0 ]; then
                    read line
                    get_partition_mbr_size $line
                fi
                echo $line | grep "downloadfile" >> /dev/null
                if [ $? -eq 0 ]; then
                    get_partition_downfile_size $line
                fi
            fi
        done < ./show_sys_partition.tmp
        echo ------------------------------------
        rm ./show_sys_partition.tmp
    else
        echo "==========input is not a partition file=========="
    fi
}

#mkspiffs <size_in_bytes> <input_directory> <output_file>
function mkspiffs()
{
    ${PACK_TOPDIR}/tools/scripts/spiffsgen.py --meta-len 0 --obj-name-len 128 --page-size 256 --block-size $((64 * 1024)) $1 $2 $3
}

function mklittlefs()
{
    echo "--- creating littlefs image ---"
    if [ ${storage_type} = "3" ]; then #nor
        ${PACK_TOPDIR}/tools/packtool/mklfs -c $2 -b $((4 * 1024)) -r 256 -p 256 -s $1 -i $3
    else #nand
        ${PACK_TOPDIR}/tools/packtool/mklfs -c $2 -b $((4 * 1024)) -r $((4 * 1024)) -p $((4 * 1024)) -s $1 -i $3
    fi
    echo "--- create littlefs image end ---"
}

function mkminfs()
{
    minfs make $1 $2 rootfs.ini
}

#parse the sys_partition.fex and only downloadfile name with string 'data' be made to filesystem image[see line 269].
function make_data_image()
{
    pack_info "running the function make_data_image for partition data"

    local part name size downloadfile
    echo `sed -e '/^ *;/d'  -e 's/\r//g' $1`

    for part in $(get_part_info ${1} ${2})
    do
        echo "*******"
        echo "=${part}="
        echo "*******"
        name="$(awk -F: '{print $1}' <<< "${part}")"
        size="$(awk -F: '{print $2}' <<< "${part}")"
        downloadfile="$(awk -F: '{print $3}' <<< "${part}" | sed 's/"//g')"

        if [ "x${name}" == "xUDISK" ] ; then
            part_name=${name}
            part_size=$(( ${size} * 1024 / 512 ))
            eval ${3}=${part_size}
		fi

        grep "^data_udisk.fex" <<< "${downloadfile}" || continue

        size=$(( ${size} * 1024 ))

        echo "try to create image: ${name} to ${downloadfile} with size ${size}"

        name="${PACK_TOPDIR}/projects/${PACK_BOARD}/data/UDISK"

        if [ ! -d ${name} ]; then
            pack_error "not found ${name} for creating ${downloadfile}"
            continue
        fi

        #downloadfile="${PACK_TOPDIR}/out/${PACK_BOARD}/image/${downloadfile}"
        #mkspiffs ${size} ${name} ${downloadfile}
        #echo `whereis mklittlefs`

        set -e

        if [ ! -z $(grep "^CONFIG_ROOTFS_LITTLEFS=y" "${MELIS_BASE}/.config") ] ;
            then mklittlefs ${size} ${name} ${downloadfile}
        fi

        if [ ! -z $(grep "^CONFIG_ROOTFS_MINFS=y" "${MELIS_BASE}/.config") ] ;
            then mkminfs    ${name}  ${downloadfile}
        fi

        set +e

        if [ x"${4}" == x"cut_zero" ]; then
            echo "cut zero to reduce fs size"
            mv ${downloadfile} ${downloadfile}.ori
            sed '$ s/\x00*$//' ${downloadfile}.ori > ${downloadfile}.cut
            dd if=${downloadfile}.cut of=${downloadfile} bs=$fs_align_size conv=sync
        fi
    done
}

function do_rootfs_ini_tmp()
{
    cp -f ${PACK_TOPDIR}/projects/${PACK_BOARD}/configs/rootfs.ini ${PACK_TOPDIR}/projects/${PACK_BOARD}/data/UDISK/rootfs_ini.tmp
    [ -f ${PACK_TOPDIR}/projects/${PACK_BOARD}/data/UDISK/rootfs_ini.tmp ] || {
        pack_warn "rootfs_ini.tmp not exist!"
        continue;
    }
    echo ${PACK_TOPDIR}/projects/${PACK_BOARD}/data/UDISK/rootfs_ini.tmp
}

function change_bootA_partition_size()
{
    #Set the compression mode according to the configuration
    fileSize=0
    if [ ! -z $(grep "^CONFIG_EPOS_COMPRESS_NONE=y" "${MELIS_BASE}/.config") ] ;
        then
        fileSize=`ls -l epos.img | awk '{print $5}'` #get filesize
    elif [ ! -z $(grep "^CONFIG_EPOS_COMPRESS_GZ=y" "${MELIS_BASE}/.config") ];
        then
        fileSize=`ls -l epos-gz.img | awk '{print $5}'` #get filesize
    elif [ ! -z $(grep "^CONFIG_EPOS_COMPRESS_LZ4=y" "${MELIS_BASE}/.config") ];
        then
        fileSize=`ls -l epos-lz4.img | awk '{print $5}'` #get filesize
    elif [ ! -z $(grep "^CONFIG_EPOS_COMPRESS_LZMA=y" "${MELIS_BASE}/.config") ];
        then
        fileSize=`ls -l epos.img.lzma | awk '{print $5}'` #get filesize
    fi

    fileSize=$(( ($fileSize / 1024) + 1)) #get k size
    fileSize=$(( ($fileSize/64 +1) * 64 * 2)) #align 64K
    echo "---------------------------------------booA size = $fileSize-----------------------------------"
    sed -i "/bootA/,/\"melis_pkg_nor\.fex\"/{s/.*size.*/    size = $fileSize/}" $1
}

function update_package_boot_cfg()
{
    #Cancel all compression methods
    sed -i "s/^item=freertos/;item=freertos/g" package_boot0.cfg
    sed -i "s/^item=melis-gz/;item=melis-gz/g" package_boot0.cfg
    sed -i "s/^item=melis-lz4/;item=melis-lz4/g" package_boot0.cfg
    sed -i "s/^item=melis-lzma/;item=melis-lzma/g" package_boot0.cfg

    #Set the compression mode according to the configuration
    echo "*******************************************************"
    if [ ! -z $(grep "^CONFIG_EPOS_COMPRESS_NONE=y" "${MELIS_BASE}/.config") ] ;
        then
        echo "epos compress mode is : none"
        sed -i "s/^;item=freertos/item=freertos/g" package_boot0.cfg
    elif [ ! -z $(grep "^CONFIG_EPOS_COMPRESS_GZ=y" "${MELIS_BASE}/.config") ];
        then
        echo "epos compress mode is : GZ"
        sed -i "s/^;item=melis-gz/item=melis-gz/g" package_boot0.cfg
    elif [ ! -z $(grep "^CONFIG_EPOS_COMPRESS_LZ4=y" "${MELIS_BASE}/.config") ];
        then
        echo "epos compress mode is : LZ4"
        sed -i "s/^;item=melis-lz4/item=melis-lz4/g" package_boot0.cfg
    elif [ ! -z $(grep "^CONFIG_EPOS_COMPRESS_LZMA=y" "${MELIS_BASE}/.config") ];
        then
        echo "epos compress mode is : LZMA"
        sed -i "s/^;item=melis-lzma/item=melis-lzma/g" package_boot0.cfg
    fi
    echo "*******************************************************"
}

function do_prepare()
{
    pack_info  "copying config/boot binary/phoenix plugin files"
    do_rootfs_ini_tmp

    for file in ${phoenixplugin_file_list[@]} ; do
        echo ${file} ; cp -f $file ./
    done

    for file in ${configs_file_list[@]} ; do
        echo ${file} ; cp -f $file ./
    done

    for file in ${boot_file_list[@]} ; do
        echo ${file} ; cp -f `echo $file | awk -F: '{print $1}'` `echo $file | awk -F: '{print $2}'` 2>/dev/null
    done

    if [ "x${PACK_SECURE}" = "xsecure" -o "x${PACK_SIG}" = "xsecure" -o  "x${PACK_SIG}" = "xprev_refurbish" ] ; then
        printf "copying secure boot file\n"
        for file in ${boot_file_secure[@]} ; do
            echo ${file} | sed 's/${MELIS_BASE}//g'; cp -f `echo $file | awk -F: '{print $1}'` `echo $file | awk -F: '{print $2}'`
        done
    fi

    if [ "x${PACK_MODE}" = "xdump" ] ; then
        cp -vf sys_partition_nor_dump.fex        sys_partition_nor.fex
        cp -vf sys_partition_card_dump.fex       sys_partition_card.fex
        cp -vf sys_partition_nand_dump.fex       sys_partition_nand.fex
        cp -vf usbtool_test.fex                  usbtool.fex
    fi
    IMG_NAME="${PACK_PLATFORM}_${PACK_BOARD}_${PACK_DEBUG}"

    if [ "x${PACK_SIG}" != "xnone" ]; then
        IMG_NAME="${IMG_NAME}_${PACK_SIG}"
    fi

    if [ "x${PACK_MODE}" = "xdump" ] ; then
        IMG_NAME="${IMG_NAME}_${PACK_MODE}"
    fi

    if [ "x${PACK_SECURE}" = "xsecure" ] ; then
        IMG_NAME="${IMG_NAME}_${PACK_SECURE}"
    fi

	[ -f epos.img ] && gzip -c      epos.img        > epos-gz.img
	[ -f epos.img ] && lz4  -f      epos.img        epos-lz4.img
	[ -f epos.img ] && lzma -zfk    epos.img
}

function do_common()
{
    local img_name="${IMG_NAME}"
    local update_type="SPINOR_FLASH"

    sed -i 's/\\\\/\//g' image_$1.cfg
    sed -i 's/^imagename/;imagename/g' image_$1.cfg

    img_name="${img_name}_$1"

    if [ "x${PACK_SECURE}" != "xnone" -o "x${PACK_SIG}" != "xnone" ]; then
        MAIN_VERION=`awk  '$0~"MAIN_VERSION"{printf"%d", $3}' version_base.mk`
        img_name="${img_name}_v${MAIN_VERION}.img"
    else
        img_name="${img_name}.img"
    fi

    echo "imagename = $img_name" >> image_$1.cfg
    echo "" >> image_$1.cfg;
    pack_info "define image file name is:$img_name"

    [ -f sys_config_$1${SUFFIX}.fex ] || {
        pack_warn "sys_config_$1.fex not exist!"
        continue;
    }

    storage_type=`sed -e '/^$/d' -e '/^;/d' -e '/^\[/d' -n -e '/^storage_type/p' "sys_config_$1${SUFFIX}.fex" | sed -e 's/[a-z,A-Z,_, ,=,\r,\n]//g'`
    echo "storage type value is *${storage_type}*"

    if [ "x${PACK_SECURE}" = "xsecure"  -o "x${PACK_SIG}" = "xsecure" ] ; then
        printf "add burn_secure_mode in target in sys config\n"
        sed -i -e '/^\[target\]/a\burn_secure_mode=1'           sys_config_$1${SUFFIX}.fex
        sed -i -e '/^\[platform\]/a\secure_without_OS=0'        sys_config_$1${SUFFIX}.fex
    elif [ "x${PACK_SIG}" = "xprev_refurbish" ] ; then
        printf "add burn_secure_mode in target in sys config\n"
        sed -i -e '/^\[target\]/a\burn_secure_mode=1'           sys_config_$1${SUFFIX}.fex
        sed -i -e '/^\[platform\]/a\secure_without_OS=1'        sys_config_$1${SUFFIX}.fex
    else
        sed -i '/^burn_secure_mod/d'                            sys_config_$1${SUFFIX}.fex
        sed -i '/^secure_without_OS/d'                          sys_config_$1${SUFFIX}.fex
	fi

    [ -f sys_partition_$1.fex ] || {
        pack_warn "sys_partition_$1.fex not exist!"
        continue
    }

    #Set the compression mode according to the configuration
    if [ ! -z $(grep "^CONFIG_CHANGE_COMPRESS_METHOD=y" "${MELIS_BASE}/.config") ] ;
        then
        update_package_boot_cfg
        change_bootA_partition_size sys_partition_$1.fex
    fi

    busybox unix2dos sys_config_$1${SUFFIX}.fex
    script  sys_config_$1${SUFFIX}.fex      > /dev/null
    cp -f   sys_config_$1${SUFFIX}.bin      config_$1${SUFFIX}.fex

    [ -f boot0_$1.fex ] && {
        if [ "$1"  = "nor" ] ; then        update_boot0 boot0_$1.fex	sys_config_$1${SUFFIX}.bin      "SPINOR_FLASH"  > /dev/null
        elif [ "$1"  = "card" ] ; then     update_boot0 boot0_$1.fex	sys_config_$1${SUFFIX}.bin      "SDMMC_CARD"    > /dev/null
        elif [ "$1"  = "card_product" ] ; then     update_boot0 boot0_$1.fex	sys_config_$1${SUFFIX}.bin      "SDMMC_CARD"    > /dev/null
        elif [ "$1"  = "nand" ] ; then     update_boot0 boot0_$1.fex	sys_config_$1${SUFFIX}.bin      "NAND"          > /dev/null
        else    pack_warn "please check, img_storage_type is \"${img_storage_type[@]}\""
        fi
		update_chip_melis boot0_$1.fex
    }

    busybox unix2dos        sys_partition_$1.fex
    script  sys_partition_$1.fex > /dev/null

    # Those files for SpiNor. We will try to find sys_partition_nor.fex
    if [ -f package_boot0.cfg ] ; then
        echo "pack boot $1 package"
		cp sys_config_$1${SUFFIX}.bin sys_config.bin
        busybox unix2dos        package_boot0.cfg
        dragonsecboot -pack     package_boot0.cfg
        if [ $? -ne 0 ] ; then
            pack_error "dragon pack nor run error" ; exit 1
        fi
        mv boot_package.fex     melis_pkg_nor.fex
        dd if=melis_pkg_nor.fex of=boot_package_nor.fex bs=1k count=32
	else
        update_rtos --image epos-gz.img --output epos-gz-update.img
        if [ $? -ne 0  ]; then
            pack_error "add rtos header error!"
            exit 1
        fi
        cp epos-gz-update.img melis_pkg_nor.fex
	fi

    [ -f fes1.fex ] && update_fes1  fes1.fex        sys_config_$1${SUFFIX}.bin > /dev/null
    [ -f u-boot_$1.fex ] && {
        update_uboot -no_merge          u-boot_${1}.fex         sys_config_$1${SUFFIX}.bin > /dev/null
        #gzip -c u-boot_${1}.fex        > u-boot_${1}-gz.fex
    }
    [ -f package_uboot_$1.cfg ] && {
        busybox unix2dos        package_uboot_${1}.cfg
        dragonsecboot -pack     package_uboot_${1}.cfg
        [ $? -ne 0 ] && {
            pack_error "dragon pack run error" ; exit 1
        }

        update_toc1             boot_package.fex        sys_config_$1${SUFFIX}.bin
        [ $? -ne 0 ] && {
            pack_error "update toc1 run error" ; exit 1
        }
        mv                      boot_package.fex        boot_pkg_uboot_${1}.fex
    }

	rm sys_config.bin
	cp sys_config_$1${SUFFIX}.bin sys_config_bin.fex

	[ -f env.cfg ] && {
		env_size=4096
		mkenvimage -r -p 0x00 -s ${env_size} -o env.fex env.cfg
	}
}

function do_finish()
{
    pack_info "running the function do_finish \"sys_partition_$1.bin\""
    if [ -f sys_partition_$1.bin ] ; then
        if [ $1 = "nor" ] ; then
            update_mbr sys_partition_$1.bin         1       sunxi_mbr_$1.fex
        elif [ $1 = "card" ] ; then
            update_mbr sys_partition_$1.bin         4       sunxi_mbr_$1.fex
		elif [ $1 = "card_product" ] ; then
            update_mbr sys_partition_$1.bin         4       sunxi_mbr_$1.fex
        elif [ $1 = "nand" ] ; then
            update_mbr sys_partition_$1.bin         4       sunxi_mbr_$1.fex
        else
            pack_error "do_finish error, check the storage para is [$1]"
        fi
        if [ $? -ne 0 ]; then
			pack_error "update_mbr failed"
			exit 1
		fi
		if [ "x${PACK_MODE}" = "xdump" ] ; then
			printf "pack dump image, don't need to create full_img\n"
		else
            if [ "x$1" = "xnor" ] ; then
                if [ ${current_rtos_full_img_size} -eq 8 ]; then
                    pack_warn "2021-01-19 do not create full binary because new uboot";create_rtos_full_img ${LOGICAL_UDISK_PARTS_KB_REMAIN_FOR_8M} ${current_rtos_full_img_size}
                elif [ ${current_rtos_full_img_size} -eq 16 ]; then
                    pack_warn "2021-01-19 do not create full binary because new uboot";create_rtos_full_img ${LOGICAL_UDISK_PARTS_KB_REMAIN_FOR_16M} ${current_rtos_full_img_size}
                elif [ ${current_rtos_full_img_size} -eq 64 ]; then
                    pack_warn "2021-01-19 do not create full binary because new uboot";#create_rtos_full_img ${LOGICAL_UDISK_PARTS_KB_REMAIN_FOR_64M} ${current_rtos_full_img_size}
                else
                    pack_error "full img size ${current_rtos_full_img_size} is not 8/16M"
                    exit 1;
                fi
            fi
        fi
        do_dragon image_$1.cfg sys_partition_$1.fex
    fi

	#source ${PACK_TOPDIR}/../../../../aligenie/host/tools/otatool/pack_update_zip_allwinner.sh
	#pack_update_zip
	pack_info "pack finish\n"
}

function pack_update_zip()
{
    local RTOSFS_IMG=rtosfs.img
    local DATA_OUT=${PACK_TOPDIR}/out/${PACK_BOARD}/compile_dir/target/data
    rm -fr update.zip

    ln -fs image/melis_pkg_nor.fex ${RTOSFS_IMG}

#	if [ "x${PACK_SIG}" = "xsecure" ]; then
    	${PACK_TOPDIR}/../../../../aligenie/host/tools/otatool/md5byslice md5.list ${RTOSFS_IMG}  #*.mp3 #toc0.fex toc1.fex
#	else
#		${PACK_TOPDIR}/../../../../aligenie/host/tools/otatool/md5byslice md5.list  ${RTOSFS_IMG} boot_package.fex boot0_nand.fex config.zip config2.zip
#	fi
#	temp disable should revert when key is ready       ${PACK_TOPDIR}/../../../../aligenie/host/tools/otatool/rsa_encrypt_file md5.list md5.encrypt ${PACK_TOPDIR}/tools/target/product/security/alitvsec_priv.pem
#	mv md5.list md5.encrypt
#	if [ "x${PACK_SIG}" = "xsecure" ]; then
    	zip -0  update.zip   ${RTOSFS_IMG} md5.list  #toc0.fex toc1.fex
#	else
#		zip -0  update.zip  ${RTOSFS_IMG} boot_package.fex boot0_nand.fex config.zip config2.zip md5.encrypt
#	fi

#	[ -e ${RECOVERY_IMG} ] &amp;&amp; zip -r update.zip ${RECOVERY_IMG}
    echo '----------update.zip is at----------'
    echo -e '\033[0;31;1m'
    echo ${PACK_TOPDIR}/out/${PACK_BOARD}/update.zip
    echo -e '\033[0m'
}

function do_dragon()
{
    pack_info "running the function do_dragon with file \"$@\""

    local partition_file_name="x$2"
    if [ $partition_file_name != "x" ]; then
        echo ====================================
        echo show \"$2\" message
        show_partition_message $2
    fi
    dragon $@

    if [ $? -eq 0 ]; then
        if [ -e ${IMG_NAME} ]; then
            mv ${IMG_NAME} ../${IMG_NAME}
            echo "----------${image_instruction}----------"
            echo '----------image is at----------'
            echo -e '\033[0;31;1m'
            echo ${PACK_TOPDIR}/out/${PACK_BOARD}/${IMG_NAME}
            echo -e '\033[0m'
        fi
    fi

    [ -e ${PACK_TOPDIR}/tools/scripts/.hooks/post-dragon ] &&
        source ${PACK_TOPDIR}/tools/scripts/.hooks/post-dragon
}

function do_signature()
{
# merge flag: '1' - merge atf/scp/uboot/optee in one package, '0' - do not merge.
    local merge_flag=0

    printf "prepare for signature by openssl\n"
    cp -v ${PACK_TOPDIR}/projects/${PACK_BOARD}/sign_config/dragon_toc.cfg dragon_toc.cfg
    if [ $? -ne 0 ]
    then
        pack_error "dragon toc config file is not exist"
        exit 1
    fi

    if [ x${SUFFIX} == x'' ]; then
        dragonsecboot -toc0 dragon_toc.cfg ${PACK_TOPDIR}/out/${PACK_BOARD}/keys ${PACK_TOPDIR}/out/${PACK_BOARD}/image/version_base.mk
        if [ $? -ne 0 ]
        then
            pack_error "dragon toc0 run error"
            exit 1
        fi
    fi

    update_toc0  toc0.fex           sys_config${SUFFIX}.bin
    if [ $? -ne 0 ]
    then
        pack_error "update toc0 run error"
        exit 1
    fi

    if [ x${SUFFIX} == x'' ]; then
        if [ ${merge_flag} == 1 ]; then
            printf "dragon boot package\n"
            dragonsecboot -pack dragon_toc.cfg
            if [ $? -ne 0 ]
            then
                pack_error "dragon boot_package run error"
                exit 1
            fi
        fi

        dragonsecboot -toc1 dragon_toc.cfg ${PACK_TOPDIR}/out/${PACK_BOARD}/keys \
            ${PACK_TOPDIR}/projects/${PACK_BOARD}/sign_config/cnf_base.cnf \
            ${PACK_TOPDIR}/out/${PACK_BOARD}/image/version_base.mk
        if [ $? -ne 0 ]
        then
            pack_error "dragon toc1 run error"
            exit 1
        fi
    fi

    update_toc1  toc1.fex           sys_config${SUFFIX}.bin
    if [ $? -ne 0 ]
    then
        pack_error "update toc1 run error"
        exit 1
    fi
    echo "secure signature ok!"
}

function do_pack_melis()
{
    pack_info "packing for melis\n"
<<'COMMENT'
COMMENT
}

function do_pack_rtos()
{
    pack_info "packing for rtos\n"

    # boot_package/toc1 limit to 4M，but rtos may large then 4M
    rm -f melis_pkg_nor.fex
    rm -f rtos_pkg.fex
    rm -f freertos-gz-update.fex
    rm -f uboot_toc1.fex

    #init default value
    echo "" > subimg0.fex; subimg0_size=0; subimg0_name="img0"; subimg0_file="";
    echo "" > subimg1.fex; subimg1_size=0; subimg1_name="img1"; subimg1_file="";
    echo "" > subimg2.fex; subimg2_size=0; subimg2_name="img2"; subimg2_file="";
    echo "" > subimg3.fex; subimg3_size=0; subimg3_name="img3"; subimg3_file="";
    echo "" > subimg4.fex; subimg4_size=0; subimg4_name="img4"; subimg4_file="";
    echo "" > subimg5.fex; subimg5_size=0; subimg5_name="img5"; subimg5_file="";
    echo "" > subimg6.fex; subimg6_size=0; subimg6_name="img6"; subimg6_file="";
    echo "" > subimg7.fex; subimg7_size=0; subimg7_name="img7"; subimg7_file="";
    echo "" > subimg8.fex; subimg8_size=0; subimg8_name="img8"; subimg8_file="";
    echo "" > subimg9.fex; subimg9_size=0; subimg9_name="img9"; subimg9_file="";

    #config by user
    subimg0_file_name="rtos_gz"
    subimg0_file="epos-gz.img"

    #compatible with old uboot, it get dst_len from tail
    let dd_size=$align_size-4
    dd if=/dev/zero of=./end.fex bs=1 count=$dd_size
    echo -e -n "\xFF\xFF\xFF\xFF" >> end.fex

<<'COMMENT'
	#merge all subimgs to be new freertos-gz.fex
	cat subimg0.fex subimg1.fex subimg2.fex subimg3.fex subimg4.fex subimg5.fex subimg6.fex subimg7.fex subimg8.fex subimg9.fex end.fex > freertos-gz.fex

	if [ "x${PACK_SIG}" = "xsecure" ] ; then
		echo "secure"
		do_signature

		update_rtos --image freertos-gz.fex \
			--cert toc1/cert/freertos-gz.der \
			--name0 $subimg0_name --size0 $subimg0_size \
			--name1 $subimg1_name --size1 $subimg1_size \
			--name2 $subimg2_name --size2 $subimg2_size \
			--name3 $subimg3_name --size3 $subimg3_size \
			--name4 $subimg4_name --size4 $subimg4_size \
			--name5 $subimg5_name --size5 $subimg5_size \
			--name6 $subimg6_name --size6 $subimg6_size \
			--name7 $subimg7_name --size7 $subimg7_size \
			--name8 $subimg8_name --size8 $subimg8_size \
			--name9 $subimg9_name --size9 $subimg9_size \
			--output freertos-gz-update.fex

		if [ $? -ne 0 ]; then
			pack_error "add rtos header error!"
			exit 1
		fi
		ln -s freertos-gz-update.fex melis_pkg_nor.fex
		ln -s freertos-gz-update.fex rtos_pkg.fex
		cp toc1.fex uboot_toc1.fex #toc1.fex for nand, uboot_toc1.fex for nor

		if [ "x${storage_type}" = "x3" ] ; then
			echo "boot_package_nor.fex" > boot_package_nor.fex
			echo "boot_package.fex" > boot_package.fex
			# for nor, keep small toc1.fex to make uboot happy, won't write to flash
			dd if=uboot_toc1.fex of=toc1.fex bs=1k count=32
		fi
	elif [ "x${PACK_SIG}" = "xprev_refurbish" ] ; then
		echo "prev_refurbish"
		do_signature
		update_rtos --image freertos-gz.fex \
			--cert toc1/cert/freertos-gz.der \
			--name0 $subimg0_name --size0 $subimg0_size \
			--name1 $subimg1_name --size1 $subimg1_size \
			--name2 $subimg2_name --size2 $subimg2_size \
			--name3 $subimg3_name --size3 $subimg3_size \
			--name4 $subimg4_name --size4 $subimg4_size \
			--name5 $subimg5_name --size5 $subimg5_size \
			--name6 $subimg6_name --size6 $subimg6_size \
			--name7 $subimg7_name --size7 $subimg7_size \
			--name8 $subimg8_name --size8 $subimg8_size \
			--name9 $subimg9_name --size9 $subimg9_size \
			--output freertos-gz-update.fex

		if [ $? -ne 0 ]; then
			pack_error "add rtos header error!"
			exit 1
		fi
		ln -s freertos-gz-update.fex melis_pkg_nor.fex
		ln -s freertos-gz-update.fex rtos_pkg.fex
		cp toc1.fex uboot_toc1.fex #toc1.fex for nand, uboot_toc1.fex for nor

		if [ "x${storage_type}" = "x3" ] ; then
			echo "boot_package_nor.fex" > boot_package_nor.fex
			echo "boot_package.fex" > boot_package.fex
			# for nor, keep small toc1.fex to make uboot happy, won't write to flash
			dd if=uboot_toc1.fex of=toc1.fex bs=1k count=32
		fi
	else
		echo "normal"
		update_rtos --image freertos-gz.fex \
			--name0 $subimg0_name --size0 $subimg0_size \
			--name1 $subimg1_name --size1 $subimg1_size \
			--name2 $subimg2_name --size2 $subimg2_size \
			--name3 $subimg3_name --size3 $subimg3_size \
			--name4 $subimg4_name --size4 $subimg4_size \
			--name5 $subimg5_name --size5 $subimg5_size \
			--name6 $subimg6_name --size6 $subimg6_size \
			--name7 $subimg7_name --size7 $subimg7_size \
			--name8 $subimg8_name --size8 $subimg8_size \
			--name9 $subimg9_name --size9 $subimg9_size \
			--output freertos-gz-update.fex
		if [ $? -ne 0 ]; then
			pack_error "add rtos header error!"
			exit 1
		fi
		ln -s freertos-gz-update.fex melis_pkg_nor.fex
		ln -s freertos-gz-update.fex rtos_pkg.fex

		if [ "x${storage_type}" = "x3" ] ; then
			mv boot_package_nor.fex uboot_pkg_nor.fex
			mv boot_package.fex uboot_pkg.fex

			# keep small boot_package.fex/boot_package_nor.fex to make uboot happy, won't write to flash
			dd if=uboot_pkg_nor.fex of=boot_package_nor.fex bs=1k count=32
			dd if=uboot_pkg.fex of=boot_package.fex bs=1k count=32
		fi

		ln -s uboot_pkg_nor.fex uboot_toc1.fex
	fi
COMMENT
}

function create_rtos_full_img()
{
        pack_info "running the function create_rtos_full_img for pure binary imagefile $1 $2"

        set -e

        #rtos use uboot-2018, so not use mbr, but gpt
        gpt_file=sunxi_gpt_nor.fex
        mbr_source_file=sunxi_mbr_nor.fex
        #rtos logic start is 80K
        LOGIC_START=48
        if [ "x${PACK_SIG}" = "xsecure" ] ; then
        	boot0_file_name=toc0.fex
        	full_rtos_img_name=${PACK_PLATFORM}_${2}Mnor_s.fex
        else
	       	boot0_file_name=boot0_nor.fex
        	full_rtos_img_name=${PACK_PLATFORM}_${2}Mnor.fex
        fi
        set +e
        echo ----------------mbr convert to gpt start---------------------
        mbr_convert_to_gpt --source ${mbr_source_file} \
        	               --out ${gpt_file} \
        	               --input_logic_offset $((${LOGIC_START} * 1024 / 512 )) \
        	               --input_flash_size ${2}
        echo ----------------mbr convert to gpt end---------------------
        set -e
        cp ${gpt_file} sunxi_gpt.fex

        cp ${boot0_file_name} boot0.fex
        #cp ${boot0_file_name} boot0_sdcard.fex
        #rtos have not uboot, rtos use gpt
        merge_full_rtos_img --out ${full_rtos_img_name} \
                            --boot0 ${boot0_file_name} \
                            --mbr ${gpt_file} \
                            --logic_start ${LOGIC_START} \
                            --partition sys_partition_nor.bin \
                            --UDISK_partition_size ${1} \
                            --total_image_size ${2}
        if [ $? -ne 0 ]; then
        	pack_error "merge_full_rtos_img failed"
        	exit 1
        else
        	mv ${full_rtos_img_name} ../${full_rtos_img_name}
        	echo ----------rtos full image is at----------
        	echo -e '\033[0;31;1m'
        	echo ${PACK_TOPDIR}/out/${PACK_BOARD}/${full_rtos_img_name}
        	echo -e '\033[0m'
        fi

        set +e
}

function prepare_for_8Mnor()
{
    pack_info "making data image for 8M nor"
    make_data_image sys_partition_nor.fex ${LOGICAL_PARTS_KB_FOR_8M} LOGICAL_UDISK_PARTS_KB_REMAIN_FOR_8M
    sed -i 's/\(imagename = .*\)_[^_]*nor/\1_8Mnor/g'               image_nor.cfg
    IMG_NAME=$(awk '{if($3~/^'${PACK_PLATFORM}'.*img$/)print$3}'    image_nor.cfg)
    current_rtos_full_img_size=8
}

function prepare_for_16Mnor()
{
    pack_info "making data image for 16M nor"
    make_data_image sys_partition_nor.fex ${LOGICAL_PARTS_KB_FOR_16M} LOGICAL_UDISK_PARTS_KB_REMAIN_FOR_16M
    sed -i 's/\(imagename = .*\)_[^_]*nor/\1_16Mnor/g'              image_nor.cfg
    IMG_NAME=$(awk '{if($3~/^'${PACK_PLATFORM}'.*img$/)print$3}'    image_nor.cfg)
    echo $IMG_NAME
    current_rtos_full_img_size=16
}

function prepare_for_64Mnor()
{
    pack_info "making data image for 64M nor"
    make_data_image sys_partition_nor.fex ${LOGICAL_PARTS_KB_FOR_64M} LOGICAL_UDISK_PARTS_KB_REMAIN_FOR_64M "cut_zero"
    sed -i 's/\(imagename = .*\)_[^_]*nor/\1_64Mnor/g'              image_nor.cfg
    IMG_NAME=$(awk '{if($3~/^'${PACK_PLATFORM}'.*img$/)print$3}'    image_nor.cfg)
    current_rtos_full_img_size=64
}

function prepare_for_128Mnand()
{
    pack_info "making data image for 128M nand"
    make_data_image sys_partition_nand.fex ${LOGICAL_PARTS_KB_FOR_128Mnand} LOGICAL_UDISK_PARTS_KB_REMAIN_FOR_128Mnand "cut_zero"
    sed -i 's/\(imagename = .*\)_[^_]*nand/\1_128Mnand/g'           image_nand.cfg
    IMG_NAME=$(awk '{if($3~/^'${PACK_PLATFORM}'.*img$/)print$3}'    image_nand.cfg)
    current_rtos_full_img_size=0 #not support now
}

[ -d "${PACK_TOPDIR}/out/${PACK_BOARD}/image" ] && rm -rf ${PACK_TOPDIR}/out/${PACK_BOARD}/image
mkdir -p ${PACK_TOPDIR}/out/${PACK_BOARD}/image; cd ${PACK_TOPDIR}/out/${PACK_BOARD}/image
pack_info "temporarily Enter pack directory: \"`pwd`\", will be back when terminated"
#read -p "====" -t 3
do_prepare
if [ "x${PACK_STORAGE_TYPE}" = "xnor" ] ; then
	do_common "nor"
elif [ "x${PACK_STORAGE_TYPE}" = "xsdcard" ] ; then
	do_common "card"
elif [ "x${PACK_STORAGE_TYPE}" = "xsdcard_product" ] ; then
	do_common "card_product"
elif [ "x${PACK_STORAGE_TYPE}" = "xnand" ] ; then
	do_common "nand"
fi
do_pack_${PACK_PLATFORM}

if [ "x${PACK_MODE}" = "xdump" ] ; then
    do_finish
elif [ "x${PACK_STORAGE_TYPE}" = "xnor" ] ; then
    [ "x${PACK_NORVOL}" = "x64" ] && {
        prepare_for_64Mnor
        do_finish "nor"
    }
    [ "x${PACK_NORVOL}" = "x16" ] && {
        prepare_for_16Mnor
        do_finish "nor"
    }
    [ "x${PACK_NORVOL}" = "x8" ] && {
        prepare_for_8Mnor
        do_finish "nor"
    }
elif [ "x${PACK_STORAGE_TYPE}" = "xsdcard" ] ; then
	prepare_for_8Mnor
	sed -i 's/\(imagename = .*\)_[^_]*card/\1_8Mcard/g'             image_card.cfg
    IMG_NAME=$(awk '{if($3~/^'${PACK_PLATFORM}'.*img$/)print$3}'    image_card.cfg)
    do_finish "card"
elif [ "x${PACK_STORAGE_TYPE}" = "xsdcard_product" ] ; then
	prepare_for_8Mnor
	sed -i 's/\(imagename = .*\)_[^_]*card/\1_8Mcard/g'             image_card_product.cfg
    IMG_NAME=$(awk '{if($3~/^'${PACK_PLATFORM}'.*img$/)print$3}'    image_card_product.cfg)
    do_finish "card_product"
else
    prepare_for_128Mnand
    do_finish "nand"
fi

exit 0
