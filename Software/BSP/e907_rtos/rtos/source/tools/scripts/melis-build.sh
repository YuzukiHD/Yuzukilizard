cd source
source melis-env.sh

# Useage:
#	build melis:   ./melis-build.sh v853-e907-ver1-board -j16
#	menuconfig :   ./melis-build.sh v853-e907-ver1-board menuconfig
#	clean      :   ./melis-build.sh v853-e907-ver1-board clean
#   strip elf  :   ./melis-build.sh v853-e907-ver1-board strip

function read_board_info() {
	local board_path=${MELIS_BASE}/projects
	local j=0
	for i in `ls -1 $board_path`
	do
		if [ -d $board_path/$i ]; then
			local board_name=${i#*config_}
			boards_list[j]=$board_name
			j=`expr $j + 1`
		fi
	done
}

function show_board_info() {
	local board_path=${MELIS_BASE}/projects
	local j=0
	for i in  ${boards_list[@]}
	do
			echo "    "$i
	done
}

function match_board() {
	local board_path=${MELIS_BASE}/projects
	local j=0
	for i in  ${boards_list[@]}
	do
		if [ "$1" == "$i" ]; then
			return 1
		fi
	done

	return 0
}

function get_strip() {
	local arch=`sed -n '/CONFIG_RISCV=/p' .config`

	if [ -z $arch ]; then
		echo "arm-melis-eabi-strip"
	else
		echo "riscv64-unknown-elf-strip"
	fi
}

read_board_info

if [ $# -eq 0 ]; then
	echo "You should enter following parameters."
	show_board_info
	exit
fi

$(match_board $1)

if [ $? -ne 1 ]; then
	echo "Invalid parameters:"$1
	echo "You should enter following parameters."
	show_board_info
	exit
fi

lunch $1

if [ "$2" == "strip" ]; then
	strip=$(get_strip)
	if [ "$3" ]; then
		if [ -e $3 ]; then
			$strip $3
		else
			echo "target file not exist."
		fi
	else
		if [ -e ./ekernel/melis30.elf ]; then
			$strip ./ekernel/melis30.elf
		else
			echo "Please compile melis."
		fi
	fi
else
	make $2
fi

