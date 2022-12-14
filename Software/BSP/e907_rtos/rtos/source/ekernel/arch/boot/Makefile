boot := ekernel/arch/boot

%.dtb: | scripts
	$(Q)$(MAKE) $(build)=$(boot)/dts MACHINE=arm $(boot)/dts/$@

dtbs: prepare scripts
	$(Q)$(MAKE) $(build)=$(boot)/dts

%.bin: | scripts
	$(Q)$(MAKE) $(build)=$(boot)/fex MACHINE=arm $(boot)/fex/$@

fexs: prepare scripts
	$(Q)$(MAKE) $(build)=$(boot)/fex
