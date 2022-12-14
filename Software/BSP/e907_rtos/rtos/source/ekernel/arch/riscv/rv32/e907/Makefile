subdir-ccflags-y +=     -I$(srctree)/ekernel/arch/riscv/rv32/e907 \
			-I$(srctree)/ekernel/arch/riscv/rv32/e907/clic \
			-I$(srctree)/include/melis/ekernel/arch/riscv

subdir-asflags-y +=     -I$(srctree)/ekernel/arch/riscv/rv32/e907

obj-y += vectors.o
obj-y += trap_c.o
obj-y += startup_gcc.o
obj-y += system.o
obj-y += clic/
obj-y += cache.o
obj-$(CONFIG_STANDBY_MSGBOX) += sleep.o
