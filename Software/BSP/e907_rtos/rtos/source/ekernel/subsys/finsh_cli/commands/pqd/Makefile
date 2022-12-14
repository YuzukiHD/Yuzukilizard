subdir-ccflags-y +=	-I$(srctree)/include/melis/kernel/drivers \
			-I$(srctree)/include/melis/kernel \
			-I$(srctree)/include/melis/kernel/pthread \
			-I$(srctree)/include/melis/libs/libc \
			-I$(srctree)/ekernel/subsys \
			-I$(srctree)/ekernel/subsys/libc \
			-I$(srctree)/ekernel/subsys/adbd \
			-I$(srctree)/ekernel/subsys/adbd/include \
			-I$(srctree)/ekernel/subsys/finsh_cli \
			-I$(srctree)/ekernel/subsys/aw/multi_console \
			-I$(srctree)/ekernel/subsys/net/rt-thread/lwip/src/ \
			-I$(srctree)/ekernel/subsys/net/rt-thread/lwip/src/include \
			-I$(srctree)/ekernel/subsys/net/rt-thread/lwip/src/arch/include \
			-I$(srctree)/ekernel/filesystem/fs \
			-I$(srctree)/emodules/mod_shell/Esh_builtins/commands/doscript \
			-DEPDK_DEBUG_LEVEL=EPDK_DEBUG_LEVEL_LOG_ALL -Wall -Wstrict-prototypes

EXTRA_CFLAGS+=-Wno-date-time

obj-y += pq_main.o
obj-y += trans_info.o
obj-y += dexxx/de_generic.o
obj-y += dexxx/de201_trans.o

#$(obj)/copyfiles:
#		@(cp $(obj)/disp_cfg/sunxi_pq.cfg $(MELIS_BASE)/workspace/suniv/rootfs/etc/sunxi_pq.cfg; \
#            cp $(obj)/disp_cfg/disp_firmware $(MELIS_BASE)/workspace/suniv/rootfs/etc/disp_firmware)




#always += copyfiles

