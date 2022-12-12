#!/bin/bash
# This script will transform your global value of h file to Binary data
# make by wuguanling(K.L)

VERSION=1.0
script_name=$0

function creat_struct_data_from_head_file()
{
	head_file=$1
	struct_name=$2
	out_put_dir=./
	[ ! -z $out_put_dir ] && out_put_dir=$3

	#To find arm-gcc tool
	[ -z $TINA_BUILD_TOP ] && echo "please lunch env" && return 1
	CC=$TINA_BUILD_TOP/prebuilt/gcc/linux-x86/arm/toolchain-sunxi-musl/toolchain/bin/arm-openwrt-linux-muslgnueabi-gcc

	#complie head file to object file
	echo -e "#include \"$head_file\"  \nvoid make_by_kl(void) {} " | $CC -x c -c -o tmp.o  -I $PWD -

	echo ---------------------------
	#parser isp struct of head file, To konw offsett and len
	readelf -s --wide tmp.o  | grep "Ndx Name"
	readelf -s --wide tmp.o  | grep $struct_name
	bin_offset=`readelf -s --wide tmp.o  | grep $struct_name | awk '{print $2}'`
	bin_len=`readelf -s --wide tmp.o  | grep $struct_name | awk '{print $3}'`
	echo struct date offset = 0x$bin_offset
	echo struct len = $bin_len

	echo ---------------------------

	#parser .data of object file offset
	readelf -S tmp.o | grep "Name"
	readelf -S tmp.o | grep " .data "
	data=`readelf -S tmp.o | grep " .data " | awk '{print $6}'`
	echo .data offset = 0x$data

	echo ---------------------------

	#cpoy the data form object
	binary_data_offset=`expr $((16#$bin_offset)) + $((16#$data))`
	if [ -z `echo $bin_len | grep 0x` ]; then
		len=$bin_len
	else
		len=$((16#${bin_len#*0x}))
	fi

	[ ! -z $binary_data_offset ] && [ ! -z $len ] && \
	dd if=./tmp.o of=$out_put_dir/$struct_name.bin bs=1 count=$len skip=$binary_data_offset && \
	echo "creat $struct_name.bin success !"

	#rm tmp file
	rm ./tmp.o
}

function parser_head_file_struct()
{
	file=$1
	out_put_path=${file%%/*}

	str0="########## "
	str1=$(ls $file)
	str2=" ##########"
	str3=$(git log $file | awk 'NR==1' | cut -c1-13)
	str4="########## >>>>>>>>>>> "
	echo $str0$str1$str2 > $out_put_path/bin_version
	echo $str4$str3 >> $out_put_path/bin_version

	struct_name=`cat $file | grep ".isp_test_settings"`
	struct_name=${struct_name#*&}
	struct_name=${struct_name%%,*}
	[ ! -z $struct_name ] && \
	creat_struct_data_from_head_file $file $struct_name $out_put_path

	struct_name=`cat $file | grep ".isp_3a_settings"`
	struct_name=${struct_name#*&}
	struct_name=${struct_name%%,*}
	echo $struct_name
	[ ! -z $struct_name ] && \
	creat_struct_data_from_head_file $file $struct_name $out_put_path

	struct_name=`cat $file | grep ".isp_tunning_settings"`
	struct_name=${struct_name#*&}
	struct_name=${struct_name%%,*}
	[ ! -z $struct_name ] && \
	creat_struct_data_from_head_file $file $struct_name $out_put_path

	struct_name=`cat $file | grep ".isp_iso_settings"`
	struct_name=${struct_name#*&}
	struct_name=${struct_name%%,*}
	[ ! -z $struct_name ] && \
	creat_struct_data_from_head_file $file $struct_name $out_put_path

	mv $out_put_path/*3a_settings.bin     $out_put_path/isp_3a_settings.bin
	mv $out_put_path/*iso_settings.bin    $out_put_path/isp_iso_settings.bin
	mv $out_put_path/*test_settings.bin   $out_put_path/isp_test_settings.bin
	mv $out_put_path/*tuning_settings.bin $out_put_path/isp_tuning_settings.bin
}


function parser_dir_and_find_head_file()
{
	dir=$1

	#to file *.h in dir
	for file in $dir/*.h; do
		parser_head_file_struct $file
	done

}

function help_text()
{
	echo "This script will transform your global value of h file to Binary data!
Here, it will transform your ISP file to Binary data."

	echo "You need to make sure that those string in you H file, and the script will parser them:

        .isp_test_settings = &xx_xxx_xxxx,
        .isp_3a_settings = &xx_xxx_xxxx,
        .isp_tunning_settings = &xx_xxx_xxxx,
        .isp_iso_settings = &xx_xxx_xxxx
	"

	echo "[Useage]"
	echo "$script_name -h #it will print help string"
	echo "$script_name -v #it will print version string"
	echo "$script_name -t dir  #it will parser all *.h in dir"
	echo "$script_name -t xxx.h  #it will parser head file"
	echo ""
}

function version_pr()
{
	echo "version=$VERSION"
}

while getopts vht: OPTION
do
        case $OPTION in
        h)
                help_text
        ;;
        v)
                version_pr
        ;;
        t)
		echo $OPTARG
		[ -d $OPTARG ] && parser_dir_and_find_head_file $OPTARG
		[ -f $OPTARG ] && parser_head_file_struct $OPTARG
        ;;
	*)
		echo "input erro!!!"
                help_text
        ;;
    esac
done





