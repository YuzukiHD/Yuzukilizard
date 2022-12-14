subdir-ccflags-y +=     -I$(srctree)/ekernel/arch/riscv/atomic/include \
                        -I$(srctree)/ekernel/arch/riscv/common \
                        -I$(srctree)/ekernel/arch/riscv/include

subdir-asflags-y +=     -I$(srctree)/ekernel/arch/riscv/include
subdir-asflags-y +=     -I$(srctree)/ekernel/arch/riscv/common

subdir-asflags-y +=     -I$(srctree)/ekernel/arch/riscv/rv32/include
subdir-ccflags-y +=     -I$(srctree)/ekernel/arch/riscv/rv32/include

obj-y += common/
obj-y += sunxi/

# chip supporting package driver.
obj-y += csp/

obj-${CONFIG_RV32} += rv32/
ifneq ($(CONFIG_STANDBY_MSGBOX), y)
obj-y += standby/
endif
