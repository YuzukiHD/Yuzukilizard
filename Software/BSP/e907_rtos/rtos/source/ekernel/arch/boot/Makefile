boot := ekernel/arch/boot

%.dtb: | scripts
	$(Q)$(MAKE) $(build)=$(boot)/dts MACHINE=arm $(boot)/dts/$@

dtbs: prepare scripts
	$(Q)$(MAKE) $(build)=$(boot)/dts

%.bin: | scripts
	$(Q)$(MAKE) $(build)=$(boot)/fex MACHINE=arm $(boot)/fex/$@

fexs: prepare scripts
	$(Q)$(MAKE) $(build)=$(boot)/fex

ifeq ($(CONFIG_KERNEL_COMPRESS), y)
Image: prepare scripts
ifeq ($(CONFIG_RV32), y)
	$(Q)$(MAKE) $(build)=$(boot)/../riscv/compressed $@
endif
else
Image: prepare scripts
	@cp $(boot)/../../melis30.elf $(boot)/Image
	@cp $(boot)/Image $(boot)/zImage
	$(Q)$(STRIP) $(boot)/zImage
	$(Q)$(OBJCOPY) -O binary $(boot)/zImage $(boot)/zImage.bin
endif
