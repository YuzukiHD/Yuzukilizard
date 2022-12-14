subdir-ccflags-y +=	-I./lib_dbg_uart \
			-I$(srctree)/ekernel/drivers/include \
			-I$(srctree)/ekernel/core/rt-thread/include
obj-y += uart_drv.o
obj-${CONFIG_LEGACY_DMS_DEV}   += uart_drv_dms.o
