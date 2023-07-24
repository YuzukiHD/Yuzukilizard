subdir-ccflags-y +=	-I$(srctree)/include/melis/kernel/drivers  \
			-I$(srctree)/include/melis/misc   \
			-I$(srctree)/include/melis/module  \
			-I$(srctree)/include/melis/kernel \
			-I$(srctree)/include/melis/libs/libc  \
			-I$(obj)/ \
			-I$(srctree)/ekernel/core/rt-thread/include  \
			-I$(srctree)/ekernel/drivers/include  \
			-I$(srctree)/ekernel/drivers/include/drv  \
			-I$(srctree)/ekernel/components/aw/multi_console  \
			-DEPDK_DEBUG_LEVEL=EPDK_DEBUG_LEVEL_LOG_ALL -Wall -Wstrict-prototypes

obj-y += console.o
obj-y += porting.o
obj-y += uart_console.o
obj-$(CONFIG_MULTI_CONSOLE_RPMSG) += rpmsg_console/rpmsg_console.o
obj-$(CONFIG_MULTI_CONSOLE_RPMSG) += rpmsg_console/rpmsg_multi_console.o
