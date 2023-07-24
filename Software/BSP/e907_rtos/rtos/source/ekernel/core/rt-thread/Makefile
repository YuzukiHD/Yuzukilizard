subdir-ccflags-y += 	-I$(obj)/include \
                        -I$(obj)/libdl \
            			-I$(srctree)/ekernel/components/aw/multi_console \
            			-I$(srctree)/ekernel/drivers/include/drv \
            			-I$(srctree)/ekernel/drivers

CFLAGS_kservice.o := -Wno-date-time -Wno-implicit-fallthrough

obj-y += clock.o
obj-y += idle.o
obj-y += ipc.o
obj-y += irq.o
obj-y += kservice.o
obj-y += object.o
obj-y += scheduler.o
obj-y += thread.o
obj-y += timer.o
obj-${CONFIG_RT_USING_PIPE} += pipe.o
obj-${CONFIG_RT_USING_RINGBUFFER} += ringbuffer.o
obj-${CONFIG_RT_USING_WAITQUEUE} += waitqueue.o
obj-${CONFIG_RT_USING_WORKQUEUE} += workqueue.o
obj-${CONFIG_RT_USING_COMPLETION} += completion.o
#obj-y += components.o
#obj-y += module.o
obj-${CONFIG_RT_USING_SMALL_MEM} += mem.o
obj-${CONFIG_RT_USING_SLAB} += slab.o
obj-${CONFIG_RT_USING_DEVICE} += device.o
obj-${CONFIG_RT_USING_MEMPOOL} += mempool.o
obj-${CONFIG_RT_USING_MEMHEAP} += memheap.o
obj-${CONFIG_RT_USING_SIGNALS} += signal.o
obj-${CONFIG_RT_JLINK_RTT} += J-RTT/
obj-${CONFIG_CMSIS} += cmsis/
obj-y += wrapper/
obj-${CONFIG_MODULES} += libdl/
obj-${CONFIG_RT_SUPPORT_OPENOCD} += openocd.o
obj-$(CONFIG_RT_USING_TASK_PERF_MONITOR) += perf.o

ifeq ($(CONFIG_KASAN), y)
KASAN_SANITIZE_slab.o := n
endif
