#!/bin/sh
#
# link melis
#
# melis is linked from the objects selected by $(KBUILD_VMMELIS_INIT) and
# $(KBUILD_VMMELIS_MAIN). Most are built-in.o files from top-level directories
# in the kernel tree, others are specified in arch/$(ARCH)/Makefile.
# Ordering when linking is important, and $(KBUILD_VMMELIS_INIT) must be first.
#
# melis
#   ^
#   |
#   +-< $(KBUILD_VMMELIS_INIT)
#   |   +--< init/version.o + more
#   |
#   +--< $(KBUILD_VMMELIS_MAIN)
#   |    +--< drivers/built-in.o mm/built-in.o + more
#   |
#   +-< ${kallsymso} (see description in KALLSYMS section)
#
# melis version (uname -v) cannot be updated during normal
# descending-into-subdirs phase since we do not yet know if we need to
# update melis.
# Therefore this step is delayed until just before final link of melis.
#
# System.map is generated to document addresses of all kernel symbols

# Error out on error
set -e

# Nice output in kbuild format
# Will be supressed by "make -s"
info()
{
	if [ "${quiet}" != "silent_" ]; then
		printf "  %-7s %s\n" ${1} ${2}
	fi
}

# Link of melis.o used for section mismatch analysis
# ${1} output file
modpost_link()
{
	${LD} ${LDFLAGS} -r -o ${1} ${KBUILD_VMMELIS_INIT}                   \
		--start-group ${KBUILD_VMMELIS_MAIN} --end-group
}

# Link of melis
# ${1} - optional extra .o files
# ${2} - output file
melis_link()
{
	local lds="${objtree}/${KBUILD_LDS}"

	if [ "${SRCARCH}" != "um" ]; then
		${LD} ${LDFLAGS} ${LDFLAGS_melis} -o ${2}                  \
			-T ${lds} ${KBUILD_VMMELIS_INIT}                     \
			--start-group ${KBUILD_VMMELIS_MAIN} --end-group ${1}
	else
		${CC} ${CFLAGS_melis} -o ${2}                              \
			-Wl,-T,${lds} ${KBUILD_VMMELIS_INIT}                 \
			-Wl,--start-group                                    \
				 ${KBUILD_VMMELIS_MAIN}                      \
			-Wl,--end-group                                      \
			-lutil -lrt -lpthread ${1}
		rm -f linux
	fi
}


# Create ${2} .o file with all symbols from the ${1} object file
kallsyms()
{
	info KSYM ${2}
	local kallsymopt;

	if [ -n "${CONFIG_HAVE_UNDERSCORE_SYMBOL_PREFIX}" ]; then
		kallsymopt="${kallsymopt} --symbol-prefix=_"
	fi

	if [ -n "${CONFIG_KALLSYMS_ALL}" ]; then
		kallsymopt="${kallsymopt} --all-symbols"
	fi

	if [ -n "${CONFIG_ARM}" ] && [ -z "${CONFIG_XIP_KERNEL}" ] && [ -n "${CONFIG_PAGE_OFFSET}" ]; then
		kallsymopt="${kallsymopt} --page-offset=$CONFIG_PAGE_OFFSET"
	fi

	if [ -n "${CONFIG_X86_64}" ]; then
		kallsymopt="${kallsymopt} --absolute-percpu"
	fi

	local aflags="${KBUILD_AFLAGS} ${KBUILD_AFLAGS_KERNEL}               \
		      ${NOSTDINC_FLAGS} ${MELISINCLUDE} ${KBUILD_CPPFLAGS}"

	${NM} -n ${1} | \
		scripts/kallsyms ${kallsymopt} | \
		${CC} ${aflags} -c -o ${2} -x assembler-with-cpp -
}

# Create map file with all symbols from ${1}
# See mksymap for additional details
mksysmap()
{
	${CONFIG_SHELL} "${srctree}/scripts/mksysmap" ${1} ${2}
}

sortextable()
{
	${objtree}/scripts/sortextable ${1}
}

# Delete output files in case of error
cleanup()
{
	rm -f .old_version
	rm -f .tmp_System.map
	rm -f .tmp_kallsyms*
	rm -f .tmp_version
	rm -f .tmp_melis*
	rm -f System.map
	rm -f melis
	rm -f melis.o
}

on_exit()
{
	if [ $? -ne 0 ]; then
		cleanup
	fi
}
trap on_exit EXIT

on_signals()
{
	exit 1
}
trap on_signals HUP INT QUIT TERM

#
#
# Use "make V=1" to debug this script
case "${KBUILD_VERBOSE}" in
*1*)
	set -x
	;;
esac

if [ "$1" = "clean" ]; then
	cleanup
	exit 0
fi

# We need access to CONFIG_ symbols
case "${KCONFIG_CONFIG}" in
*/*)
	. "${KCONFIG_CONFIG}"
	;;
*)
	# Force using a file from the current directory
	. "./${KCONFIG_CONFIG}"
esac

#${srctree}/utility/host-elf/signboot/checksum  0 ${prjtree}/eGon/storage_media/spinor/booter0.bin ${prjtree}/eGon/storage_media/spinor/boot0.bin
#${srctree}/utility/host-elf/signboot/checksum  1 ${prjtree}/eGon/storage_media/spinor/booter1.bin ${prjtree}/eGon/storage_media/spinor/boot1.bin
#${srctree}/utility/host-elf/script/script        ${prjtree}/eFex/sys_config.fex
#${srctree}/utility/host-elf/script/script        ${prjtree}/eFex/sys_partition.fex
#${srctree}/utility/host-elf/script/script        ${prjtree}/rootfs/app_config.fex


rm -f .old_version
