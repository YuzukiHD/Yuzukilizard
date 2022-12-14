#=====================================================================================
#
#      Filename:  toolchain.mk
#
#   Description:  toolchain extract and configuration.
#
#       Version:  Melis3.0 
#        Create:  2017-11-23 10:09:24
#      Revision:  none
#      Compiler:  gcc version 6.3.0 (crosstool-NG crosstool-ng-1.23.0)
#
#        Author:  caozilong@allwinnertech.com
#  Organization:  BU1-PSW
# Last Modified:  2017-11-23 10:09:24
#
#=====================================================================================

# check gcc tools chain installation.
ifneq ("$(MELIS_BASE)", "")

toolsrc=$(MELIS_BASE)/../tools/xcompiler/on_linux/compiler

ifneq ($(shell grep "^CONFIG_ARM=y" $(MELIS_BASE)/.config),)
    toolchain_archive=$(toolsrc)/gcc-arm-melis-eabi-9-2020-q2-update-x86_64-linux.tar.bz2
    tooldir=$(MELIS_BASE)/../toolchain/gcc-arm-melis-eabi-9-2020-q2-update-x86_64-linux/
    CROSS_COMPILE := $(tooldir)/bin/arm-melis-eabi-
else
ifneq ($(shell grep "^CONFIG_RISCV=y" $(MELIS_BASE)/.config),)
    #toolchain_archive=$(toolsrc)/riscv64-elf-gcc-thead_20200528.tar.gz
    #tooldir=$(MELIS_BASE)/../toolchain/riscv64-elf-gcc-thead_20200528/
    toolchain_archive=$(toolsrc)/riscv64-elf-x86_64-20201104.tar.gz
    tooldir=$(MELIS_BASE)/../toolchain/riscv64-elf-x86_64-20201104/
    CROSS_COMPILE := $(tooldir)/bin/riscv64-unknown-elf-
else
    $(error wrong arch, only support riscv and arm arch present.)
endif
endif

toolchain_check=$(strip $(shell if [ -d $(tooldir) ];  then  echo yes;  fi))
ifneq ("$(toolchain_check)", "yes")
    $(info Found toolchain $(notdir $(toolchain_archive)))
    $(info Extracting GCC......)
    $(shell mkdir -p $(tooldir))
    $(shell tar --strip-components=1 -xf $(toolchain_archive) -C $(tooldir))
    $(info Done)
endif

GCCVERSION = $(shell $(CROSS_COMPILE)gcc --version 2>&1 | grep gcc | sed 's/^.* //g')
ifeq "$(GCCVERSION)" ""
    $(error gcc $(GCCVERSION) crosstoolchain installation failure.)
endif

else
    $(error Please execute 'source melis-env.sh' first.)
endif
