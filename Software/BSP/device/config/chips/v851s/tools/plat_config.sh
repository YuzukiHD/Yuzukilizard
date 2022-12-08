#!/bin/bash

#set -e

#
# 3:ddr3 4:ddr4 7:lpddr3 8:lpddr4
#
DRAM_TYPE=0
DRAM_NAME="null"
PACK_CHIP="sun8iw21p1"


copy_boot_file()
{
	DRAM_TYPE=`awk  '$0~"dram_type"{printf"%d", $3}' out/sys_config.fex`

	case $DRAM_TYPE in
		3) DRAM_NAME="ddr3"
		;;
		4) DRAM_NAME="ddr4"
		;;
		7) DRAM_NAME="lpddr3"
		;;
		8) DRAM_NAME="lpddr4"
		;;
		*) DRAM_NAME="unknow"
		exit 0
		;;
	esac

	plat_boot_file_list=(
		chips/${PACK_CHIP}/bin/boot0_nand_${PACK_CHIP}_${DRAM_NAME}.bin:out/boot0_nand.fex
		chips/${PACK_CHIP}/bin/boot0_sdcard_${PACK_CHIP}_${DRAM_NAME}.bin:out/boot0_sdcard.fex
		chips/${PACK_CHIP}/bin/boot0_spinor_${PACK_CHIP}_${DRAM_NAME}.bin:out/boot0_spinor.fex
		chips/${PACK_CHIP}/bin/fes1_${PACK_CHIP}_${DRAM_NAME}.bin:out/fes1.fex
		chips/${PACK_CHIP}/bin/sboot_${PACK_CHIP}_${DRAM_NAME}.bin:out/sboot.bin
		chips/${PACK_CHIP}/bin/scp_${DRAM_NAME}.bin:out/scp.fex
	)


	printf "copying boot file for  ${DRAM_NAME}\n"
	for file in ${plat_boot_file_list[@]} ; do
		cp -f $(echo $file | sed -e 's/:/ /g') 2>/dev/null
	done
}

#copy_boot_file
