#=====================================================================================
#
#      Filename:  melis-env.sh
#
#   Description:  setup 'MELIS_BASE' environment variable.
#
#       Version:  Melis3.0
#        Create:  2017-11-09 16:21:57
#      Revision:  none
#      Compiler:  gcc version 6.3.0 (crosstool-NG crosstool-ng-1.23.0)
#
#        Author:  caozilong@allwinnertech.com
#  Organization:  BU1-PSW
# Last Modified:  2020-05-06 12:46:08
#
#=====================================================================================

# In zsh the value of $0 depends on the FUNCTION_ARGZERO option which is
# set by default. FUNCTION_ARGZERO, when it is set, sets $0 temporarily
# to the name of the function/script when executing a shell function or
# sourcing a script. POSIX_ARGZERO option, when it is set, exposes the
# original value of $0 in spite of the current FUNCTION_ARGZERO setting.
#
# Note: The version of zsh need to be 5.0.6 or above. Any versions below
# 5.0.6 maybe encoutner errors when sourcing this script.
if [ -n "$ZSH_VERSION" ]; then
    DIR="${(%):-%N}"
    if [ $options[posixargzero] != "on" ]; then
        setopt posixargzero
        NAME=$(basename -- "$0")
        setopt posixargzero
    else
        NAME=$(basename -- "$0")
    fi
else
    DIR="${BASH_SOURCE[0]}"
    NAME=$(basename -- "$0")
fi

if [ "X$NAME" "==" "Xmelis-env.sh" ]; then
    echo "Source this file (do NOT execute it!) to set the Melis Kernel environment."
    exit
fi

# You can further customize your environment by creating a bash script called
# .melisrc in your home directory. It will be automatically
# run (if it exists) by this script.

if uname | grep -q "MINGW"; then
    win_build=1
    PWD_OPT="-W"
else
    win_build=0
    PWD_OPT=""
fi

# identify OS source tree root directory
export MELIS_BASE=$(builtin cd "$( dirname "$DIR" )" && pwd ${PWD_OPT})
unset PWD_OPT

scripts_path=${MELIS_BASE}/scripts
if [ "$win_build" -eq 1 ]; then
    scripts_path=$(echo "/$scripts_path" | sed 's/\\/\//g' | sed 's/://')
fi
unset win_build
if ! echo "${PATH}" | grep -q "${scripts_path}"; then
    export PATH=${scripts_path}:${PATH}
fi
unset scripts_path

# enable custom environment settings
melis_answer_file=~/melis-env_install.bash
[ -f ${melis_answer_file} ] && {
    echo "Warning: Please rename ~/melis-env_install.bash to ~/.melisrc";
    . ${melis_answer_file};
}
unset melis_answer_file
melis_answer_file=~/.melisrc
[ -f ${melis_answer_file} ] &&  . ${melis_answer_file};
unset melis_answer_file

. ${MELIS_BASE}/envsetup.sh

function ctop()
{
    top_path=${MELIS_BASE}/..
    if [ "$top_path"  ]; then
        \cd $top_path
    else
        echo "Couldn't locate the top dir."
    fi
    unset top_path
}

function cdoc()
{
    doc_path=${MELIS_BASE}/../document
    if [ "$doc_path"  ]; then
        \cd $doc_path
    else
        echo "Couldn't locate the document dir."
    fi
    unset doc_path
}

function cout()
{
    workspace_path=${MELIS_BASE}/out/${TARGET_BOARD}
    if [ "$workspace_path"  ]; then
        \cd $workspace_path
    else
        echo "Couldn't locate the workspace."
    fi
    unset workspace_path
}

function ckernel()
{
    workspace_path=${MELIS_BASE}/ekernel
    if [ "$workspace_path"  ]; then
        \cd $workspace_path
    else
        echo "Couldn't locate the workspace."
    fi
    unset workspace_path
}

function cdriver()
{
    workspace_path=${MELIS_BASE}/ekernel/drivers
    if [ "$workspace_path"  ]; then
        \cd $workspace_path
    else
        echo "Couldn't locate the workspace."
    fi
    unset workspace_path
}

function cnetdrv()
{
    workspace_path=${MELIS_BASE}/ekernel/drivers/drv/source/net
    if [ "$workspace_path"  ]; then
        \cd $workspace_path
    else
        echo "Couldn't locate the workspace."
    fi
    unset workspace_path
}

function csdmmc()
{
    workspace_path=${MELIS_BASE}/ekernel/drivers/hal/source/sdmmc
    if [ "$workspace_path"  ]; then
        \cd $workspace_path
    else
        echo "Couldn't locate the workspace."
    fi
    unset workspace_path
}

function cIsdmmc()
{
    workspace_path=${MELIS_BASE}/ekernel/drivers/include/hal/sdmmc
    if [ "$workspace_path"  ]; then
        \cd $workspace_path
    else
        echo "Couldn't locate the workspace."
    fi
    unset workspace_path
}

function croot()
{
    root_path=${MELIS_BASE}
    if [ "$root_path"  ]; then
        \cd $root_path
    else
        echo "Couldn't locate the source."
    fi
    unset root_path
}

function chal()
{
    hal_path=${MELIS_BASE}/../../rtos-hal
    if [ "$hal_path"  ]; then
        \cd $hal_path
    else
        echo "Couldn't locate the source."
    fi
    unset hal_path
}

function copensbi()
{
    opensbi_path=${MELIS_BASE}/../../brandy-2.0/opensbi
    if [ "$opensbi_path"  ]; then
        \cd $opensbi_path
    else
        echo "Couldn't locate the source."
    fi
    unset opensbi_path
}

function cboot()
{
    uboot_path=${MELIS_BASE}/../../brandy-2.0/u-boot-2018
    if [ "$uboot_path"  ]; then
        \cd $uboot_path
    else
        echo "Couldn't locate the source."
    fi
    unset uboot_path
}

function cboot0()
{
    boot0_path=${MELIS_BASE}/../../brandy-2.0/spl
    if [ "$boot0_path"  ]; then
        \cd $boot0_path
    else
        echo "Couldn't locate the source."
    fi
    unset boot0_path
}

function caudiolib()
{
    audio_lib_path=${MELIS_BASE}/elibrary/bin/3rd/cedar/drivers/audio
    if [ "$audio_lib_path"  ]; then
        \cd $audio_lib_path
    else
        echo "Couldn't locate the audio library."
    fi
    unset audio_lib_path
}

function cvideolib()
{
    video_lib_path=${MELIS_BASE}/elibrary/bin/3rd/cedar/drivers/video
    if [ "$video_lib_path"  ]; then
        \cd $video_lib_path
    else
        echo "Couldn't locate the video library."
    fi
    unset video_lib_path
}

function crtt()
{
    rtt_path=${MELIS_BASE}/ekernel/subsys/net/rt-thread
    if [ "$rtt_path"  ]; then
        \cd $rtt_path
    else
        echo "Couldn't locate the rtt kernel."
fi
    unset rtt_path
}

function czephyr()
{
    zephyr_path=${MELIS_BASE}/ekernel/subsys/net/zephyr
    if [ "$zephyr_path"  ]; then
        \cd $zephyr_path
    else
        echo "Couldn't locate the zephyr kernel."
    fi
    unset zephyr_path
}

function cboot1()
{
    boot1_path=${MELIS_BASE}/eboot/boot1
    if [ "$boot1_path"  ]; then
        \cd $boot1_path
    else
        echo "Couldn't locate the boot1."
    fi
    unset boot1_path
}

function cinclude()
{
    include_path=${MELIS_BASE}/include/melis
    if [ "$include_path"  ]; then
        \cd $include_path
    else
        echo "Couldn't locate the include."
    fi
    unset include_path
}

function cmodule()
{
    module_path=${MELIS_BASE}/emodules
    if [ "$module_path"  ]; then
        \cd $module_path
    else
        echo "Couldn't locate the emodule."
    fi
    unset module_path
}

function crootfs()
{
    rootfs_path=${UDISK_PATH}
    if [ "$rootfs_path"  ]; then
        \cd $rootfs_path
    else
        echo "Couldn't locate the rootfs."
    fi
    unset rootfs_path
}

function cramfs()
{
    ramfs_path=${MELIS_BASE}/projects/${TARGET_BOARD}/data/ramfs
    if [ "$ramfs_path"  ]; then
        \cd $ramfs_path
    else
        echo "Couldn't locate the ramfs."
    fi
    unset ramfs_path
}

function cefex()
{
    efex_path=${MELIS_BASE}/workspace/suniv/eFex
    if [ "$efex_path"  ]; then
        \cd $efex_path
    else
        echo "Couldn't locate the eFex."
    fi
    unset efex_path
}

function cconfigs()
{
    select_config_path=${MELIS_BASE}/projects/${TARGET_BOARD}/configs
    if [ "$select_config_path"  ]; then
        \cd $select_config_path
    else
        echo "Couldn't locate the ${select_config_path}."
    fi
    unset select_config_path
}

function cbin()
{
    select_config_path=${MELIS_BASE}/projects/${TARGET_BOARD}/bin
    if [ "$select_config_path"  ]; then
        \cd $select_config_path
    else
        echo "Couldn't locate the ${select_config_path}."
    fi
    unset select_config_path
}

function codeline()
{
    find ${MELIS_BASE}/ -type f -name "*.[sSch]" | xargs cat | wc -l
    cloc ${MELIS_BASE}/
}

function formatall()
{
    pushd ${MELIS_BASE}
    ASTYLE_TOOL_PATH=${MELIS_BASE}/tools/packtool
    ${ASTYLE_TOOL_PATH}/astyle --options=${MELIS_BASE}/.astylerc \
	"*.cpp" "*.c" "*.inc" "*.h" \
	--exclude=workspace \
	--exclude=utility \
	--exclude=scripts \
	--exclude=eboot \
	--exclude=emodules \
	--exclude=ekernel/subsys \
    unset workpath
    popd
}

function format()
{
    ASTYLE_TOOL_PATH=${MELIS_BASE}/tools/packtool
    ${ASTYLE_TOOL_PATH}/astyle --options=${MELIS_BASE}/.astylerc "*.cpp"
    ${ASTYLE_TOOL_PATH}/astyle --options=${MELIS_BASE}/.astylerc "*.cc"
    ${ASTYLE_TOOL_PATH}/astyle --options=${MELIS_BASE}/.astylerc "*.c"
    ${ASTYLE_TOOL_PATH}/astyle --options=${MELIS_BASE}/.astylerc "*.inc"
    ${ASTYLE_TOOL_PATH}/astyle --options=${MELIS_BASE}/.astylerc "*.h"
    unset workpath
}

function cgraph()
{
    git log --graph --decorate --oneline --simplify-by-decoration --all
}

function flash()
{
    SUNXI_FEL_PATH=${MELIS_BASE}/tools/packtool
    image_file="${workpath}/melis100.fex"
    if [ -f "$image_file" ]; then
        echo "start buring........."
    else
        echo "image file not exist!"
    fi

    ${SUNXI_FEL_PATH}/packtool/sunxi-fel spiflash-info
    ${SUNXI_FEL_PATH}/packtool/sunxi-fel -p spiflash-write 0 $1
    unset workpath
    unset image_file
}

function rdrom()
{
    workpath=${MELIS_BASE}/workspace/suniv/beetles
    image_file="${workpath}/flashrom.bin"
    if [ -f "$image_file" ]; then
        rm -fr image_file
    fi

    echo "start reading rom........."

    ${workpath}/packtool/sunxi-fel spiflash-info
    ${workpath}/packtool/sunxi-fel -p spiflash-read 0 0xa00000 $1
    unset workpath
    unset image_file
}

function comopen()
{
    ports_USB=$(ls /dev/ttyUSB*)
    ports_ACM=$(ls /dev/ttyACM*)
    ports="$ports_USB $ports_ACM"
    datename=$(date +%Y%m%d-%H%M%S)
    select port in $ports;do
        if [ "$port" ]; then
            echo "You select the choice '$port'"
            echo "love" | sudo -S minicom -c on -D "$port" -C /tmp/"$datename".log "$@"
            break
        else
            echo "Invaild selection"
        fi
    done
}

function openocd_connect()
{
    sudo JLinkGDBServer -device arm9
    #sudo JLinkGDBServer -device cortex-a7
}

function gdb_connect()
{
    arm-melis-eabi-gdb -se  ./ekernel/melis.elf
    #then execute following instructions in gdb environment.
    #tar remote localhost:2331 
}

function gerit_push()
{
    git push origin melis-kbuild:refs/for/melis-kbuild
}

function scan_path () 
{
    IFS=":"
    for path in $PATH; do
        find "$path" -maxdepth 1 -executable -name 'arm*-gcc' -printf '%f\t\n' 2>/dev/null
    done
}

function find_gcc()
{
    # Use only the first field from result, and convert it to a toolchain prefix
    scan_path | cut -f 1 | sed -e 's/-gcc/-/'
} 

function find_address()
{
    arm-melis-eabi-addr2line -e ekernel/melis30.elf -a -f -p "$@"
}

function find_branch_base()
{
    git merge-base $1 $2
}

function callstack()
{
    python ${MELIS_BASE}/tools/python/backtrace_parser.py $1 ${MELIS_BASE}/ekernel/melis30.elf
}

function delog()
{
    git log --diff-filter=D --summary
}

function cmpp()
{
    local mpp_middleware_path=${MELIS_BASE}/ekernel/subsys/avframework/eyesee-mpp/middleware/sun8iw19p1
    if [ -d "${mpp_middleware_path}" ]; then
        cd ${mpp_middleware_path}
    else
        echo "Couldn't locate the mpp middleware path:[${mpp_middleware_path}]."
    fi
}

function cv4l2()
{
    local v4l2_path=${MELIS_BASE}/ekernel/subsys/avframework/v4l2
    if [ -d "${v4l2_path}" ]; then
        cd ${v4l2_path}
    else
        echo "Couldn't locate the mpp middleware path:[${v4l2_path}]."
    fi
}

if ! echo "${PATH}" | grep -q "${MELIS_BASE}/../toolchain/bin"; then
    export PATH=$PATH:${MELIS_BASE}/../toolchain/bin
fi

