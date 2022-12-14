-include include/config/auto.conf

DB_QUOTES := "

ifdef CONFIG_ARM

TARGET_BINARY_FORMAT := elf32-littlearm
ARCH_VFPUSED  := -mfpu=neon-vfpv4 -mtune=cortex-a7 -march=armv7ve \
		-mcpu=cortex-a7 -mfloat-abi=hard

ifndef CONFIG_SDK_THUMB_MODE
# aapcs is default eabi on baremetal, difference with aapcs-linux eabi is the enums type size.
# aapcs:dynamic according the acutual type range.
# aapcs-linux: fix int type, means 4 bytes.
ARCH_VFPUSED += -mabi=aapcs
# ARCH_VFPUSED += -mabi=aapcs-linux
endif

ifdef CONFIG_SDK_THUMB_MODE
ARCH_VFPUSED += -mthumb
endif
else
ifdef CONFIG_RISCV
TARGET_BINARY_FORMAT := elf32-littleriscv
endif
endif

ifdef CONFIG_ARM
MELISINCLUDE += -Iinclude/melis/arch/cortex-v7a
else
ifdef CONFIG_RISCV
MELISINCLUDE += -Iinclude/melis/arch/riscv -DARCH_CPU_64BIT
else
    #$(error no arch select.)
endif
endif

ifdef CONFIG_RISCV
# t-head compile flag.
RISCVFLAGS     := $(subst $(DB_QUOTES),,$(CONFIG_TOOLCHAIN_MACH_FLAGS))
RISCVFLAGS     += -falign-functions=4
ARCH_VFPUSED   += $(subst $(DB_QUOTES),,$(CONFIG_TOOLCHAIN_MACH_FLAGS))
endif

MELISINCLUDE   += $(RISCVFLAGS)

export MELISINCLUDE TARGET_BINARY_FORMAT
