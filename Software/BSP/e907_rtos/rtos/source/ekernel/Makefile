include $(MELIS_BASE)/scripts/Makefile.common

OBJCOPYFLAGS := -S -O binary \
                -R .note \
                -R .comment \
                -R COMMON \
                -R .ARM.attributes \
                -R .en_frame \
                -R .init -R .fini \
                --gap-fill=0xff

subdir-ccflags-y +=	-I$(srctree)/ekernel/arch  \
			-I$(srctree)/ekernel/core/rt-thread/include \
			-I$(srctree)/ekernel/core/rt-thread/include/libc \
			-I$(srctree)/ekernel/drivers/include/drv \
			-I$(srctree)/ekernel/drivers/include/hal \
			-I$(srctree)/ekernel/drivers/include/osal \
			-I$(srctree)/ekernel/drivers/rtos-hal/hal/source/ccmu/sunxi \
			-I$(srctree)/ekernel/components/finsh_cli \
			-I$(srctree)/ekernel/components/thirdparty/dfs/include \
			-I$(srctree)/ekernel/drivers/drv/source/sound \
			-D__LINUX_ERRNO_EXTENSIONS__

subdir-ccflags-y += -DFD_SETSIZE=1024

MOD_NAME := melis30
$(MOD_NAME)-objs += built-in.o
obj-m += $(MOD_NAME).o

obj-y := arch/ core/ drivers/ components/
obj-y += ../projects/$(TARGET_BOARD)/src/

usrlibs-y += -L$(srctree)/${elibrary-libs}

usrlibs-$(CONFIG_DRIVERS_VIN) += -L$(srctree)/ekernel/drivers/rtos-hal/hal/source/vin/vin_isp/isp_server/out \
	--start-group \
	-lisp_algo \
	--end-group

usrlibs-${CONFIG_SDK_RELEASE}	+= 	-L$(srctree)/${elibrary-libs}/3rd/v4l2/ \
					-L$(srctree)/${elibrary-libs}/3rd/adbd/ \
					--start-group \
					-lv4l2 -ladbd \
					--end-group

usrlibs-${VIDEO_SUNXI_VIN} 	+= --require-defined=vin_init
usrlibs-${CONFIG_COMPONENT_ADBD}+= --require-defined=adbd_init

ifneq ($(filter y, $(CONFIG_mpp_venc) $(CONFIG_mpp_vdec)),)
usrlibs-y += -L$(srctree)/ekernel/components/avframework/eyesee-mpp/middleware/sun8iw19p1/media/LIBRARY/libcedarc/base \
	-L$(srctree)/ekernel/components/avframework/eyesee-mpp/middleware/sun8iw19p1/media/LIBRARY/libcedarc/library/${LIB_DIR} \
	-L$(srctree)/ekernel/components/avframework/eyesee-mpp/middleware/sun8iw19p1/media/LIBRARY/libcedarc/memory \
	-L$(srctree)/ekernel/components/avframework/eyesee-mpp/middleware/sun8iw19p1/media/LIBRARY/libcedarc/vdecoder \
	--start-group \
	-lcdc_base \
	-lVE \
	-lMemAdapter \
	--end-group
endif

usrlibs-$(CONFIG_mpp_venc) += -L$(srctree)/ekernel/components/avframework/eyesee-mpp/middleware/sun8iw19p1/media/LIBRARY/libcedarc/base \
	-L$(srctree)/ekernel/components/avframework/eyesee-mpp/middleware/sun8iw19p1/media/LIBRARY/libcedarc/library/${LIB_DIR} \
	-L$(srctree)/ekernel/components/avframework/eyesee-mpp/middleware/sun8iw19p1/media/LIBRARY/libcedarc/memory \
	-L$(srctree)/ekernel/components/avframework/eyesee-mpp/middleware/sun8iw19p1/media/LIBRARY/libcedarc/vdecoder \
	-L$(srctree)/ekernel/components/avframework/eyesee-mpp/middleware/sun8iw19p1/media/LIBRARY/libcedarc/vencoder \
	-L$(srctree)/ekernel/components/avframework/eyesee-mpp/middleware/sun8iw19p1/media/LIBRARY/libcedarc/vencoder/base \
	--start-group \
	-lvencoder \
	-lvenc_base \
	-lvenc_codec \
	--end-group

usrlibs-$(CONFIG_mpp_vdec) += -L$(srctree)/ekernel/components/avframework/eyesee-mpp/middleware/sun8iw19p1/media/LIBRARY/libcedarc/base \
	-L$(srctree)/ekernel/components/avframework/eyesee-mpp/middleware/sun8iw19p1/media/LIBRARY/libcedarc/library/${LIB_DIR} \
	-L$(srctree)/ekernel/components/avframework/eyesee-mpp/middleware/sun8iw19p1/media/LIBRARY/libcedarc/memory \
	-L$(srctree)/ekernel/components/avframework/eyesee-mpp/middleware/sun8iw19p1/media/LIBRARY/libcedarc/vdecoder \
	--start-group \
	-lawh264 \
	-lawh265 \
	-lawmjpegplus \
	-lvideoengine \
	-lvdecoder \
	--end-group

usrlibs-$(CONFIG_mpp_vi) += -L$(srctree)/ekernel/components/avframework/eyesee-mpp/middleware/sun8iw19p1/media/LIBRARY/libisp/out \
	--start-group \
	-liniparser \
	-lisp_algo \
	-lisp_dev \
	-lisp_ini \
	-lisp_math \
	--end-group

usrlibs-$(CONFIG_mpp_aenc) += -L$(srctree)/ekernel/components/avframework/eyesee-mpp/middleware/sun8iw19p1/media/LIBRARY/AudioLib/lib \
	--start-group \
	-laacenc \
	-lmp3enc \
	--end-group

usrlibs-$(CONFIG_mpp_adec) += -L$(srctree)/ekernel/components/avframework/eyesee-mpp/middleware/sun8iw19p1/media/LIBRARY/AudioLib/lib \
	--start-group \
	-laac \
	-lmp3 \
	-lwav \
	--end-group

usrlibs-$(CONFIG_mpp_aec) += -L$(srctree)/ekernel/components/avframework/eyesee-mpp/middleware/sun8iw19p1/media/LIBRARY/aec_lib \
	--start-group \
	-lAec \
	--end-group

usrlibs-$(CONFIG_SUNXI_AEC_TEST) += -L$(srctree)/elibrary/media/libaec \
	--start-group \
	-laec \
	--end-group

usrlibs-$(CONFIG_mpp_softdrc) += -L$(srctree)/ekernel/components/avframework/eyesee-mpp/middleware/sun8iw19p1/media/LIBRARY/drc_lib \
	--start-group \
	-lDrc \
	--end-group

usrlibs-$(CONFIG_XR829) += -L$(srctree)/ekernel/drivers/drv/source/net/xradio \
	--start-group \
	-lxr829_40M \
	--end-group

usrlibs-$(CONFIG_DRIVERS_NAND_FLASH) += -L$(srctree)/ekernel/drivers/rtos-hal/hal/source/nand_flash/ \
	--start-group \
	-lmelis-nand-sun20iw1 \
	--end-group

linkscript = -T$(prjtree)/$(TARGET_BOARD)/kernel.lds

lnkflags-y += --omagic --gc-sections -cref --entry=_start \
			-X ${linkscript} \
			-Map=$(obj)/linker.map --build-id=sha1

ifeq ($(CONFIG_KALLSYMS), y)
# Usage:
#   $(call link_module, input1.o, input2.o, output.o)
define link_module
$(Q)$(LD) $(ld_flags) -o $3 $1 \
			$(lnkflags-y) \
			--start-group \
			$(usrlibs-y)  \
			$(deplibs-y)  \
			--end-group   \
			$2
endef
# Usage:
#   $(call gen_kall_o, input.o, output.o)
define gen_kall_o
nm $1 | scripts/kallsyms $(if $(CONFIG_KALLSYMS_ALL), --all-symbols,) > sym.S; \
$(CC) -c -o $2  sym.S \
		${KBUILD_AFLAGS} ${KBUILD_AFLAGS_KERNEL} \
		${NOSTDINC_FLAGS} ${LINUXINCLUDE} \
		${KBUILD_CPPFLAGS}; \
rm sym.S;
endef
endif

$(obj)/$(MOD_NAME).bin :%.bin :%.o FORCE
ifeq ($(CONFIG_KALLSYMS), y)
	@# step 1 Link mm1.o it has all symbols buf kallsyms is empty
	$(Q)$(call link_module, ekernel/$($(MOD_NAME)-objs), , .tmp_melis1.o)
	$(Q)$(call gen_kall_o, .tmp_melis1.o, .tmp_kallsyms1.o)
	@# step 2 Libk mm2 to geve kallsyms with right size
	$(Q)$(call link_module, ekernel/$($(MOD_NAME)-objs), .tmp_kallsyms1.o, .tmp_melis2.o)
	$(Q)$(call gen_kall_o, .tmp_melis2.o, .tmp_kallsyms2.o)
	@# step 3 The correct kallsyms is linked to finale obj file
	$(Q)$(call link_module, ekernel/$($(MOD_NAME)-objs), .tmp_kallsyms2.o, ekernel/$(MOD_NAME).o)
	$(Q)rm .tmp_melis1.o
	$(Q)rm .tmp_melis2.o
	$(Q)rm .tmp_kallsyms1.o
	$(Q)rm .tmp_kallsyms2.o
endif
	$(call if_changed,objcopy)
	$(call if_changed,renameelf)
	$(Q)cp -f $@  $(prjtree)/$(TARGET_BOARD)/epos.img
	$(Q)$(OBJDUMP) -d $(<:.o=.elf) > $(obj)/$(MOD_NAME).dis
	$(Q)mksysmap $(<:.o=.elf) $(obj)/System.map
	$(Q)$(STRIP) $(<:.o=.elf)
	$(Q)echo -e '\033[0;31;1m'
	$(Q)$(SIZE)  $(<:.o=.elf)
	$(Q)echo -e '\033[0m'

always     	:= $(MOD_NAME).bin

clean:mod_clean
