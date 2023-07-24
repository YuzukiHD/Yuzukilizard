#
# linux/arch/arm/boot/compressed/Makefile
#
# create a compressed zMelis image from the original Melis
#

OBJS		=

AFLAGS_head.o += -DTEXT_OFFSET=$(TEXT_OFFSET)
HEAD	= head.o
OBJS	+= misc.o decompress.o
OBJS	+= _ashldi3.o
ifeq ($(CONFIG_KERNEL_COMPRESS_DEBUG),y)
OBJS	+= backtrace.o
endif

# string library code (-Os is enforced to keep it much smaller)
OBJS		+= string.o
CFLAGS_string.o	:= -Os

ifeq ($(CONFIG_KERNEL_COMPRESS_ELF),y)
OBJS		+= elf_loader.o
endif

GCOV_PROFILE		:= n

ifeq ($(CONFIG_KERNEL_COMPRESS_ELF),y)
DATA := $(obj)/../../../melis30.elf
else
DATA := $(obj)/../../../melis30.bin
endif
#
# We now have a PIC decompressor implementation.  Decompressors running
# from RAM should not define ZTEXTADDR.  Decompressors running directly
# from ROM or Flash must define ZTEXTADDR (preferably via the config)
# FIXME: Previous assignment to ztextaddr-y is lost here. See SHARK
ZTEXTADDR	:= $(CONFIG_DRAM_PHYBASE) + 0x200000
ZBSSADDR	:= ALIGN(8)

CPPFLAGS_link.lds := -DTEXT_START="$(ZTEXTADDR)" -DBSS_START="$(ZBSSADDR)"

compress-$(CONFIG_KERNEL_COMPRESS_GZIP) = gzip
compress-$(CONFIG_KERNEL_COMPRESS_LZO)  = lzo
compress-$(CONFIG_KERNEL_COMPRESS_LZMA) = lzma
compress-$(CONFIG_KERNEL_COMPRESS_XZ)   = xzkern
compress-$(CONFIG_KERNEL_COMPRESS_LZ4)  = lz4

ifeq ($(CONFIG_KERNEL_COMPRESS_GZIP),y)
CONFIG_KERNEL_COMPRESS_METHON := 0x1
endif
ifeq ($(CONFIG_KERNEL_COMPRESS_LZ0),y)
CONFIG_KERNEL_COMPRESS_METHON := 0x2
endif
ifeq ($(CONFIG_KERNEL_COMPRESS_LZMA),y)
CONFIG_KERNEL_COMPRESS_METHON := 0x3
endif
ifeq ($(CONFIG_KERNEL_COMPRESS_XZ),y)
CONFIG_KERNEL_COMPRESS_METHON := 0x4
endif
ifeq ($(CONFIG_KERNEL_COMPRESS_LZ4),y)
CONFIG_KERNEL_COMPRESS_METHON := 0x5
endif

CPPFLAGS_link.lds += -DCONFIG_KERNEL_COMPRESS_METHON="$(CONFIG_KERNEL_COMPRESS_METHON)"

targets       := link.lds piggy_data piggy.o orig_data head.o $(OBJS)

clean-files += piggy_data zImage

KBUILD_CFLAGS += -DDISABLE_BRANCH_PROFILING

# -fstack-protector-strong triggers protection checks in this code,
# but it is being used too early to link to meaningful stack_chk logic.
nossp_flags := $(call cc-option, -fno-stack-protector)

ccflags-y := -fpic $(call cc-option,-mno-single-pic-base,) -fno-builtin -I$(obj)
asflags-y := -DZIMAGE

# Supply kernel BSS size to the decompressor via a linker symbol.
KBSS_SZ = $(shell $(CROSS_COMPILE)size $(obj)/../../../melis30.elf | \
		awk 'END{print $$3}')
LDFLAGS_zImage = --defsym _kernel_bss_size=$(KBSS_SZ)

LDFLAGS_zImage += --defsym zelfaddr=$(CONFIG_KERNEL_COMPRESS_LOADADDR)

# Supply ZRELADDR to the decompressor via a linker symbol.
LDFLAGS_zImage += --defsym zreladdr=$(CONFIG_DRAM_PHYBASE)
# Report unresolved symbol references
LDFLAGS_zImage += --no-undefined
# Delete all temporary local symbols
LDFLAGS_zImage += -X
# Next argument is a linker script
LDFLAGS_zImage += -T

KBUILD_CFLAGS += -O0

Image: $(obj)/zImage FORCE
ifeq ($(CONFIG_SUPPORT_AMP), y)
	@$(Q)$(OBJCOPY) -O binary -j .resource_table $(DATA) rsc.txt
	@$(Q)$(OBJCOPY) --update-section .resource_table=rsc.txt $(obj)/zImage
	@rm rsc.txt
endif
	@cp $(obj)/zImage $(MELIS_BASE)/ekernel/arch/boot/Image
	@cp $(obj)/zImage $(MELIS_BASE)/ekernel/arch/boot/zImage
	@$(Q)$(STRIP) $(MELIS_BASE)/ekernel/arch/boot/zImage
	@$(Q)$(OBJCOPY) -O binary ekernel/arch/boot/zImage ekernel/arch/boot/zImage.bin
	$(Q)$(SIZE) ekernel/arch/boot/zImage

$(obj)/zImage: $(obj)/link.lds $(obj)/$(HEAD) $(obj)/piggy.o \
		$(addprefix $(obj)/, $(OBJS)) FORCE
	$(call if_changed,ld)

# if defined(CONFIG_SUPPORT_AMP) && defined(CONFIG_KERNEL_COMPRESS_ELF)
# we need special handling for elf headers
$(obj)/orig_data: $(DATA) FORCE
	@cp $< $@
	$(Q)$(STRIP) $@
ifeq ($(CONFIG_SUPPORT_AMP), y)
ifeq ($(CONFIG_KERNEL_COMPRESS_ELF),y)
	$(MELIS_BASE)/scripts/markelf $@
endif
endif

$(obj)/piggy_data: $(obj)/orig_data FORCE
	$(call if_changed,$(compress-y))

$(obj)/piggy.o: $(obj)/piggy_data

#$(obj)/link.lds: $(obj)/link.lds.S FORCE
#	@$(CC) -E -P -U$(ARCH) $(NOSTDINC_FLAGS) $(CPPFLAGS_link.lds) \
#			$(obj)/link.lds.S > $(obj)/link.lds
