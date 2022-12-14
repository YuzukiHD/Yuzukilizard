function hmm() {
cat <<EOF
Invoke ". source/melis-env.sh" from your shell to add the following functions to your environment:

== before all ==
- lunch:        lunch <product_name>-<build_variant>

== build project ==
- Cleaning targets:
-   clean	  - Remove most generated files but keep the config and
-                     enough build support to build external modules
-   mrproper	  - Remove all generated files + config + various backup files
-   distclean	  - mrproper + remove editor backup and patch files
-
- Configuration targets:
-   make menuconfig to to do the customilize configuration.
-
- Other generic targets:
-   all	  - Build all targets marked with [*]
- * melis	  - Build the bare kernel
- * modules	  - Build all modules
-   gtags        - Generate GNU GLOBAL index
-
- Static analysers:
-   checkstack      - Generate a list of stack hogs
-   namespacecheck  - Name space analysis on compiled kernel
-   versioncheck    - Sanity check on version.h usage
-   includecheck    - Check for duplicate included header files
-   export_report   - List the usages of all exported symbols
-   headers_check   - Sanity check on exported headers
-   headerdep       - Detect inclusion cycles in headers
-   coccicheck      - Check with Coccinelle
-
- Kernel selftest:
-                     running kselftest on it
-   kselftest-clean - Remove all generated kselftest files
-   kselftest-merge - Merge all the config dependencies of kselftest to existing

- jump directory:
- croot:    Jump to the top of the tree.
- cboot:    Jump to uboot.
- cboot0:   Jump to boot0.
- cdts:     Jump to device tree.
- cbin:     Jump to uboot/boot0 bin directory.
- ckernel:  Jump to kernel.
- cdevice:  Jump to target.
- ccommon:  Jump to platform common.
- cconfigs: Jump to configs of target.
- cout:     Jump to out directory of target.
- ctarget:  Jump to target of compile directory.
- crootfs:  Jump to rootfs of compile directory.
- ctoolchain: Jump to toolchain directory.
- callwinnerpk: Jump to package allwinner directory.
- ctinatest:  Jump to tinateset directory.
- godir:    Go to the directory containing a file.

== grep file ==
- cgrep:    Greps on all local C/C++ files.

Look at the source to view more functions. The complete list is:
EOF
    T=$(gettop)
    local A
    A=""
    for i in `cat $T/envsetup.sh | sed -n "/^[ \t]*function /s/function \([a-z_]*\).*/\1/p" | sort | uniq`; do
      A="$A $i"
    done
    echo $A
}

function gettop()
{
    local source_path=${MELIS_BASE}
    if [ "$source_path"  ]; then
	(\cd $source_path; PWD= /bin/pwd)
    else
        echo "Couldn't locate the melis-sdk top dir."
    fi
    unset source_path
}

function ctoolchain()
{
    local T=$(gettop)
    [ -z "$T" ] \
        && echo "Couldn't locate the top of the tree.  Try setting TOP." \
        && return

    \cd $T/../toolchain/bin
}

function make()
{
	local start_time=$(date +"%s")
    command make V=s "$@"
    local ret=$?
    local end_time=$(date +"%s")
    local tdiff=$(($end_time-$start_time))
    local hours=$(($tdiff / 3600 ))
    local mins=$((($tdiff % 3600) / 60))
    local secs=$(($tdiff % 60))
	local board=${TARGET_BOARD}
    echo
    if [ $ret -eq 0 ] ; then
        echo -n -e "#### make completed successfully "
    else
        echo -n -e "#### make failed to build some targets "
    fi
    if [ $hours -gt 0 ] ; then
        printf "(%02g:%02g:%02g (hh:mm:ss))" $hours $mins $secs
    elif [ $mins -gt 0 ] ; then
        printf "(%02g:%02g (mm:ss))" $mins $secs
    elif [ $secs -gt 0 ] ; then
        printf "(%s seconds)" $secs
    fi
    echo -e " ####"
    echo
    return $ret
}

function mopensbi()
{
    local platformid=$(get_platform)
    opensbi_path=${MELIS_BASE}/../../brandy-2.0/opensbi
    if [ "$opensbi_path"  ]; then
        pushd $opensbi_path
    else
        echo "Couldn't locate the opensbi."
        return -1
    fi
    local start_time=$(date +"%s")
    ./build.sh -p $platformid
    local ret=$?
    local end_time=$(date +"%s")
    local tdiff=$(($end_time-$start_time))
    local hours=$(($tdiff / 3600 ))
    local mins=$((($tdiff % 3600) / 60))
    local secs=$(($tdiff % 60))
    echo
    if [ $ret -eq 0 ] ; then
        echo -n -e "#### make opensbi completed successfully "
        cp build/platform/thead/c910/firmware/fw_jump.bin ${MELIS_BASE}/projects/$TARGET_BOARD/sbi-bin/sbi.bin
    else
        echo -n -e "#### make opensbi failed to build some targets "
    fi
    if [ $hours -gt 0 ] ; then
        printf "(%02g:%02g:%02g (hh:mm:ss))" $hours $mins $secs
    elif [ $mins -gt 0 ] ; then
        printf "(%02g:%02g (mm:ss))" $mins $secs
    elif [ $secs -gt 0 ] ; then
        printf "(%s seconds)" $secs
    fi
    echo -e " ####"
    echo
    unset opensbi_path
    popd
    return $ret
}

function muboot()
{
    local platformid=$(get_platform)
    uboot_path=${MELIS_BASE}/../../brandy-2.0/u-boot-2018
    if [ "$uboot_path"  ]; then
        pushd $uboot_path
    else
        echo "Couldn't locate the uboot."
        return -1
    fi
    local start_time=$(date +"%s")
    make distclean
    make ${platformid}_rtos_nor_defconfig
    make -j16
    local ret=$?
    local end_time=$(date +"%s")
    local tdiff=$(($end_time-$start_time))
    local hours=$(($tdiff / 3600 ))
    local mins=$((($tdiff % 3600) / 60))
    local secs=$(($tdiff % 60))
    echo
    if [ $ret -eq 0 ] ; then
        echo -n -e "#### make u-boot completed successfully "
        cp u-boot-${platformid}.bin ${MELIS_BASE}/projects/$TARGET_BOARD/bin/u-boot_${platformid}_nor.bin
    else
        echo -n -e "#### make u-boot failed to build some targets "
    fi
    if [ $hours -gt 0 ] ; then
        printf "(%02g:%02g:%02g (hh:mm:ss))" $hours $mins $secs
    elif [ $mins -gt 0 ] ; then
        printf "(%02g:%02g (mm:ss))" $mins $secs
    elif [ $secs -gt 0 ] ; then
        printf "(%s seconds)" $secs
    fi
    echo -e " ####"
    echo
    unset uboot_path
    popd
    return $ret
}

function mboot0()
{
    local platformid=$(get_platform)
    boot0_path=${MELIS_BASE}/../../brandy-2.0/spl
    if [ "$boot0_path"  ]; then
        pushd $boot0_path
    else
        echo "Couldn't locate the boot0."
        return -1
    fi
    local start_time=$(date +"%s")
    if [ -d board/${platformid}_backup ]; then
		rm board/${platformid}_backup
    fi
    mv board/${platformid} board/${platformid}_backup
    cp board/${platformid}_rtos board/${platformid} -rf
    make distclean
    make p=${platformid}
    make
    local ret=$?
    rm board/${platformid} -rf
    mv board/${platformid}_backup board/${platformid}
    local end_time=$(date +"%s")
    local tdiff=$(($end_time-$start_time))
    local hours=$(($tdiff / 3600 ))
    local mins=$((($tdiff % 3600) / 60))
    local secs=$(($tdiff % 60))
    echo
    if [ $ret -eq 0 ] ; then
        echo -n -e "#### make boot0 completed successfully "
        cp nboot/boot0_spinor_${platformid}.bin ${MELIS_BASE}/projects/$TARGET_BOARD/bin/boot0_${platformid}_nor.bin
        cp nboot/boot0_sdcard_${platformid}.bin ${MELIS_BASE}/projects/$TARGET_BOARD/bin/boot0_${platformid}_card.bin
        cp nboot/boot0_nand_${platformid}.bin ${MELIS_BASE}/projects/$TARGET_BOARD/bin/boot0_${platformid}_nand.bin
        cp  fes/fes1_sun20iw1p1.bin ${MELIS_BASE}/projects/$TARGET_BOARD/bin/fes1_${platformid}.bin
    else
        echo -n -e "#### make boot0 failed to build some targets "
    fi
    if [ $hours -gt 0 ] ; then
        printf "(%02g:%02g:%02g (hh:mm:ss))" $hours $mins $secs
    elif [ $mins -gt 0 ] ; then
        printf "(%02g:%02g (mm:ss))" $mins $secs
    elif [ $secs -gt 0 ] ; then
        printf "(%s seconds)" $secs
    fi
    echo -e " ####"
    echo
    unset boot0_path
    popd
    return $ret
}

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
	echo "Press any key to continue!"
	char=`get_char`
}


function godir ()
{
    if [[ -z "$1" ]]; then
        echo "Usage: godir <regex>"
        return
    fi
    local T=$(gettop)
    if [[ ! -f $T/filelist ]]; then
        echo -n "Creating index..."
        (\cd $T; find . -type f > filelist)
        echo " Done"
        echo ""
    fi
    local lines
    lines=($(\grep "$1" $T/filelist | sed -e 's/\/[^/]*$//' | sort | uniq))
    if [[ ${#lines[@]} = 0 ]]; then
        echo "Not found"
        return
    fi
    local pathname
    local choice
    if [[ ${#lines[@]} > 1 ]]; then
        while [[ -z "$pathname" ]]; do
            local index=1
            local line
            for line in ${lines[@]}; do
                printf "%6s %s\n" "[$index]" $line
                index=$(($index + 1))
            done
            echo
            echo -n "Select one: "
            unset choice
            read choice
            if [[ $choice -gt ${#lines[@]} || $choice -lt 1 ]]; then
                echo "Invalid choice"
                continue
        fi
            pathname=${lines[$(($choice-1))]}
        done
    else
        pathname=${lines[0]}
    fi
    \cd $T/$pathname
}

pack_usage()
{
	printf "Usage: pack [-cCHIP] [-pPLATFORM] [-bBOARD] [-d] [-s] [-m] [-w] [-i] [-h]
	-c CHIP (default: $chip)
	-p PLATFORM (default: $platform)
	-b BOARD (default: $board)
	-d pack firmware with debug info output to card0
	-s pack firmware with signature
	-m pack dump firmware
	-w pack programmer firmware
	-i pack sys_partition.fex downloadfile img.tar.gz
	-n pack norflash volume, 8: 8Mbytes, 16: 16Mbytes, default parameter is 8
	-a pack storage type , eg nor sdcard nand,default parameter is nor
	-h print this help message
"
}

function pack() {
	local T=$(gettop)
	local chip=$(get_platform)
	local platform=melis
	local board_platform=none
	local board=${TARGET_BOARD}
	local debug=uart0
	local sigmode=none
	local securemode=none
	local mode=normal
	local programmer=none
	local tar_image=none
	local nor_volume=8
	local storage_type=nor

	if [ "v459-perf1" == "$TARGET_BOARD" ] ; then
	    nor_volume=16
	fi

	unset OPTIND
	while getopts "dsvmwihn:a:" arg
	do
		case $arg in
			d)
				debug=card0
				;;
			s)
				sigmode=secure
				;;
			v)
				securemode=secure
				;;
			m)
				mode=dump
				;;
			w)
				programmer=programmer
				;;
			i)
				tar_image=tar_image
				;;
			n)
			    nor_volume=$OPTARG;
			    ;;
			a)
				storage_type=$OPTARG;
			    ;;
			h)
				pack_usage
				return 0
				;;
			?)
			return 1
			;;
		esac
	done

	if [ "x$chip" = "x" ]; then
		echo "platform($TARGET_PLATFORM) not support"
		#return
	fi

	if [ -f $T/tools/scripts/pack_img.sh ]; then
		$T/tools/scripts/pack_img.sh -c $chip -p $platform -b $board -d $debug -s $sigmode -m $mode -w $programmer -v $securemode -i $tar_image -t $T -n $nor_volume -a $storage_type
		#pause
	else
		echo "pack_img script not exist!"
	fi
}

function cconfigs()
{
    select_config_path=${MELIS_BASE}/projects/${board_name}/configs
    if [ "$select_config_path"  ]; then
        \cd $select_config_path
    else
        echo "Couldn't locate the ${select_config_path}."
    fi
    unset select_config_path
}

function add_lunch_menu(){
    board_config_path=${MELIS_BASE}/projects
    IFS=$(echo -en "\n\b")
    local j=0
    for i in `ls -1 $board_config_path`
    do
        #if [[ "$i" =~ ^config_.*  ]]; then
        if [[ -d ${MELIS_BASE}/projects/$i ]]; then
            board_name=${i#*config_}
            config_list[j]=$board_name
            j=`expr $j + 1`
        fi
    done
}

function print_lunch_menu(){
    local i=1
    for config_list_id in ${config_list[@]}
    do
        echo "    $i. $config_list_id"
        i=$(($i+1))
    done
}

function copy_config(){
    board_name=$1
    echo "You have select $board_name "
    export TARGET_BOARD=$board_name
    export UDISK_PATH=${MELIS_BASE}/projects/${TARGET_BOARD}/data/UDISK

    dotconfig=${MELIS_BASE}/projects/${board_name}/configs/defconfig
    parttable=${MELIS_BASE}/projects/${board_name}/configs/sys_partition.fex
    sysconfig=${MELIS_BASE}/projects/${board_name}/configs/sys_config.fex

    if [ -f ${MELIS_BASE}/projects/${board_name}/configs/chip.mk ];then
        source ${MELIS_BASE}/projects/${board_name}/configs/chip.mk
    fi

    if [ -f ${dotconfig} ]; then
        cp -rf ${dotconfig} ${MELIS_BASE}/.config
    else
        echo "fatal error, no default .config file."
        return -1
    fi
    CONFIG_TARGET_PLATFORM=$(get_platform)
    if [ "${CONFIG_TARGET_PLATFORM}" = "sun8iw19p1" ]; then
        cp -fr ${sysconfig} ${MELIS_BASE}/ekernel/arch/boot/fex/
    fi
}

function get_platform() {
    unset CONFIG_SOC_SUN3IW2P2
    unset CONFIG_SOC_SUN3IW2P1
    unset CONFIG_SOC_SUN3IW1P1
    unset CONFIG_SOC_SUN8IW18P1
    unset CONFIG_SOC_SUN8IW19P1
    unset CONFIG_SOC_SUN20IW1P1
    unset CONFIG_SOC_SUN20IW3P1

    . ${MELIS_BASE}/.config

    YES="y"

    if [ "$CONFIG_SOC_SUN3IW2P2"    = "$YES" ]; then
	echo "sun3iw2p2"
    elif [ "$CONFIG_SOC_SUN3IW2P1"  = "$YES" ]; then
	echo "sun3iw2p1"
    elif [ "$CONFIG_SOC_SUN3IW1P1"  = "$YES" ]; then
	echo "sun3iw1p1"
    elif [ "$CONFIG_SOC_SUN8IW18P1" = "$YES" ]; then
	echo "sun8iw18p1"
    elif [ "$CONFIG_SOC_SUN8IW19P1" = "$YES" ]; then
	echo "sun8iw19p1"
    elif [ "$CONFIG_SOC_SUN20IW1P1" = "$YES" ]; then
	echo "sun20iw1p1"
    elif [ "$CONFIG_SOC_SUN20IW3P1" = "$YES" ]; then
	echo "sun20iw3p1"
    fi
}

function get_targetboard(){
    echo "$TARGET_BOARD"
}

function platform_verbose(){
    export TARGET_PLATFORM=$(get_platform)
    echo "============================================"
    echo "Project Based On Platform" $TARGET_PLATFORM $TARGET_BOARD
    echo "============================================"
}

function lunch(){
    local uname=$(uname -a)
    local board_choice
    echo
    echo "You're building on" $uname
    echo
    echo "Lunch menu... pick a combo:"
    echo "The supported board:"

    if [ "$1"  ] ; then
        board_choice=$1
    else
        print_lunch_menu
        echo -n "What is your choice? "
        read board_choice
    fi

    if (echo -n $board_choice | grep -q -e "^[0-9][0-9]*$"); then
        if  [ "$board_choice" -gt ${#config_list[@]} ]; then
	        echo "Too much number, exceed the maxium support board!"
	        return -1
        fi

        if [ "$board_choice" -gt 0  ] ;then
            echo "The $board_choice is number." > /dev/null
        else
            echo "The soc family [$board_choice] not supported!"
            return -1
        fi

        [ $board_choice -le ${#config_list[@]} ] \
            && board_choice=${config_list[$(($board_choice-1))]}
    else
        board_choice="$board_choice"
    fi

    for config_list_id in ${config_list[@]}
    do
        if [ "$config_list_id" == "$board_choice" ] ; then
            copy_config $config_list_id
        fi
    done

    platform_verbose

    check_elf_store_directory

    if [ -n $(grep "^CONFIG_ARM=y" "${MELIS_BASE}/.config") ] ; then
        export PATH=${PATH}:${MELIS_BASE}/../toolchain/gcc-arm-melis-eabi-9-2020-q2-update-x86_64-linux/bin
    fi
    if [ -n `grep "^CONFIG_RISCV=y" "${MELIS_BASE}/.config"` ] ; then
	    export PATH=${PATH}:${MELIS_BASE}/../toolchain/riscv64-elf-x86_64-20201104/bin
    fi
	
	#create platform info file
	echo "chip=$(get_platform)" > platform.txt
	echo "platform=melis" >> platform.txt
	echo "board_platform=none" >> platform.txt
	echo "board=${TARGET_BOARD}" >> platform.txt
	echo "debug=uart0" >> platform.txt
	echo "sigmode=none" >> platform.txt
	echo "securemode=none" >> platform.txt
	echo "mode=normal" >> platform.txt
	echo "programmer=none" >> platform.txt
	echo "tar_image=none" >> platform.txt
	echo "nor_volume=8" >> platform.txt
	echo "torage_type=nor" >> platform.txt
}

function _lunch() {
    local cur prev

    COMPREPLY=()
    cur="${COMP_WORDS[COMP_CWORD]}"
    prev="${COMP_WORDS[COMP_CWORD-1]}"

    if [[ ${cur} == *   ]] ; then
        COMPREPLY=( $(compgen -W "${config_list[*]}" -- ${cur})   )
        return 0
    fi
}

function check_elf_store_directory(){
    if [ ! -d "${MELIS_BASE}/emodules/bin/" ] ; then
        mkdir -pv "${MELIS_BASE}/emodules/bin/"
    fi
    if [ ! -d "${MELIS_BASE}/emodules/bin/cedar" ] ; then
        mkdir -pv "${MELIS_BASE}/emodules/bin/cedar"
    fi
    if [ ! -d "${MELIS_BASE}/emodules/bin/willow" ] ; then
        mkdir -pv "${MELIS_BASE}/emodules/bin/willow"
    fi
    if [ ! -d ${MELIS_BASE}/projects/${TARGET_BOARD}/data/UDISK/mod/ ] ; then
        mkdir -pv ${MELIS_BASE}/projects/${TARGET_BOARD}/data/UDISK/mod/
    fi
    if [ ! -d ${MELIS_BASE}/projects/${TARGET_BOARD}/data/UDISK/mod/cedar/ ] ; then
        mkdir -pv ${MELIS_BASE}/projects/${TARGET_BOARD}/data/UDISK/mod/cedar/
    fi
    if [ ! -d ${MELIS_BASE}/projects/${TARGET_BOARD}/data/UDISK/mod/willow/ ] ; then
        mkdir -pv ${MELIS_BASE}/projects/${TARGET_BOARD}/data/UDISK/mod/willow/
    fi
}

function envsetup() {
    unset config_list
    add_lunch_menu
    complete -F _lunch lunch
}

envsetup
