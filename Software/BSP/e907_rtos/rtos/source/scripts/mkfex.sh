#!/bin/bash

srctree=$1
if [ ! -e $srctree/platform.txt ]; then
	echo "platform info file not exist."
	exit
fi

board=`cat $srctree/platform.txt | sed -n 's/board=\(.*\)/\1/p'`;
src=${srctree}/projects/$board/configs/sys_config.fex
dst=${srctree}/ekernel/arch/boot/fex/sys_config.fex
out=${srctree}/ekernel/arch/boot/fex/sys_config.bin
script=${srctree}/tools/packtool/script

if [ -e "$src" ]; then
	echo "Compiling sys_config.fex"
	cp $src $dst
	busybox unix2dos $dst
	$script $dst > .sys_config.fex.tmp
	rm -fr .sys_config.fex.tmp
else
	echo "$src not exist."
fi
