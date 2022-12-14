ccflags-y := -I$(srctree)/ekernel/core/zephyr/kernel/include

asflags-y := ${ccflags-y}
CFLAGS_init.o := -Wno-main

obj-y =
obj-y += $(strip \
	version.o \
	prep_c.o  \
	irq_offload.o \
)

obj-y += $(strip \
	sys_clock.o \
	thread.o \
	init.o \
	sem.o \
	device.o \
	thread_abort.o \
	idle.o \
	sched.o \
	mutex.o \
	queue.o \
	stack.o \
	mem_slab.o \
	mempool.o \
	msg_q.o \
	mailbox.o \
	alert.o \
	pipes.o \
	errno.o \
	work_q.o \
	system_work_q.o \
	atomic_c.o \
)

obj-$(CONFIG_INT_LATENCY_BENCHMARK) += int_latency_bench.o
obj-$(CONFIG_STACK_CANARIES) += compiler_stack_protect.o
obj-$(CONFIG_SYS_CLOCK_EXISTS) += timer.o
obj-$(CONFIG_ATOMIC_OPERATIONS_C) += atomic_c.o
obj-$(CONFIG_POLL) += poll.o
obj-$(CONFIG_PTHREAD_IPC) += pthread.o
