#!/bin/sh

if [ $# -ne 1 ]; then
	echo "Please Input function name."
	exit
fi

arch=`sed -n '/CONFIG_ARM=y/p' .config`

if [ -z $arch ]; then
	echo "arch is riscv"
	toolchain=riscv64-unknown-elf-
else
	echo "arch is arm"
	toolchain=arm-melis-eabi-
fi

func=$1

start=`nm -n ekernel/melis30.elf |grep "^[a-fA-F0-9]* \S $func$" |awk '{ print $1 }'`
end=`nm -n ekernel/melis30.elf |grep -A1 "^[a-fA-F0-9]* \S $func$" |awk '{ getline;print $1 }'`

if [ -z $start ]; then
	echo "$func not exist."
	exit
fi

echo "$func: 0x$start - 0x$end"

${toolchain}objdump -d ./ekernel/melis30.elf --start-address=0x$start --stop-address=0x$end
