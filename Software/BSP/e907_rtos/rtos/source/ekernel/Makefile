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
			-I$(srctree)/ekernel/drivers/hal/source/ccmu/sunxi \
			-I$(srctree)/ekernel/subsys/finsh_cli \
			-I$(srctree)/ekernel/subsys/thirdparty/dfs/include \
			-I$(srctree)/ekernel/drivers/drv/source/sound \
			-D__LINUX_ERRNO_EXTENSIONS__

subdir-ccflags-y += -DFD_SETSIZE=1024

MOD_NAME := melis30
$(MOD_NAME)-objs += built-in.o
obj-m += $(MOD_NAME).o

obj-y := arch/ core/ drivers/ subsys/
obj-y += ../projects/$(TARGET_BOARD)/src/

usrlibs-y += -L$(srctree)/${elibrary-libs}

usrlibs-$(CONFIG_DRIVERS_VIN) += -L$(srctree)/ekernel/drivers/hal/source/vin/vin_isp/isp_server/out \
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
usrlibs-y += -L$(srctree)/ekernel/subsys/avframework/eyesee-mpp/middleware/sun8iw19p1/media/LIBRARY/libcedarc/base \
	-L$(srctree)/ekernel/subsys/avframework/eyesee-mpp/middleware/sun8iw19p1/media/LIBRARY/libcedarc/library/${LIB_DIR} \
	-L$(srctree)/ekernel/subsys/avframework/eyesee-mpp/middleware/sun8iw19p1/media/LIBRARY/libcedarc/memory \
	-L$(srctree)/ekernel/subsys/avframework/eyesee-mpp/middleware/sun8iw19p1/media/LIBRARY/libcedarc/vdecoder \
	--start-group \
	-lcdc_base \
	-lVE \
	-lMemAdapter \
	--end-group
endif

usrlibs-$(CONFIG_mpp_venc) += -L$(srctree)/ekernel/subsys/avframework/eyesee-mpp/middleware/sun8iw19p1/media/LIBRARY/libcedarc/base \
	-L$(srctree)/ekernel/subsys/avframework/eyesee-mpp/middleware/sun8iw19p1/media/LIBRARY/libcedarc/library/${LIB_DIR} \
	-L$(srctree)/ekernel/subsys/avframework/eyesee-mpp/middleware/sun8iw19p1/media/LIBRARY/libcedarc/memory \
	-L$(srctree)/ekernel/subsys/avframework/eyesee-mpp/middleware/sun8iw19p1/media/LIBRARY/libcedarc/vdecoder \
	-L$(srctree)/ekernel/subsys/avframework/eyesee-mpp/middleware/sun8iw19p1/media/LIBRARY/libcedarc/vencoder \
	-L$(srctree)/ekernel/subsys/avframework/eyesee-mpp/middleware/sun8iw19p1/media/LIBRARY/libcedarc/vencoder/base \
	--start-group \
	-lvencoder \
	-lvenc_base \
	-lvenc_codec \
	--end-group

usrlibs-$(CONFIG_mpp_vdec) += -L$(srctree)/ekernel/subsys/avframework/eyesee-mpp/middleware/sun8iw19p1/media/LIBRARY/libcedarc/base \
	-L$(srctree)/ekernel/subsys/avframework/eyesee-mpp/middleware/sun8iw19p1/media/LIBRARY/libcedarc/library/${LIB_DIR} \
	-L$(srctree)/ekernel/subsys/avframework/eyesee-mpp/middleware/sun8iw19p1/media/LIBRARY/libcedarc/memory \
	-L$(srctree)/ekernel/subsys/avframework/eyesee-mpp/middleware/sun8iw19p1/media/LIBRARY/libcedarc/vdecoder \
	--start-group \
	-lawh264 \
	-lawh265 \
	-lawmjpegplus \
	-lvideoengine \
	-lvdecoder \
	--end-group

usrlibs-$(CONFIG_mpp_vi) += -L$(srctree)/ekernel/subsys/avframework/eyesee-mpp/middleware/sun8iw19p1/media/LIBRARY/libisp/out \
	--start-group \
	-liniparser \
	-lisp_algo \
	-lisp_dev \
	-lisp_ini \
	-lisp_math \
	--end-group

usrlibs-$(CONFIG_mpp_aenc) += -L$(srctree)/ekernel/subsys/avframework/eyesee-mpp/middleware/sun8iw19p1/media/LIBRARY/AudioLib/lib \
	--start-group \
	-laacenc \
	-lmp3enc \
	--end-group

usrlibs-$(CONFIG_mpp_adec) += -L$(srctree)/ekernel/subsys/avframework/eyesee-mpp/middleware/sun8iw19p1/media/LIBRARY/AudioLib/lib \
	--start-group \
	-laac \
	-lmp3 \
	-lwav \
	--end-group

usrlibs-$(CONFIG_mpp_aec) += -L$(srctree)/ekernel/subsys/avframework/eyesee-mpp/middleware/sun8iw19p1/media/LIBRARY/aec_lib \
	--start-group \
	-lAec \
	--end-group

usrlibs-$(CONFIG_SUNXI_AEC_TEST) += -L$(srctree)/elibrary/media/libaec \
	--start-group \
	-laec \
	--end-group

usrlibs-$(CONFIG_mpp_softdrc) += -L$(srctree)/ekernel/subsys/avframework/eyesee-mpp/middleware/sun8iw19p1/media/LIBRARY/drc_lib \
	--start-group \
	-lDrc \
	--end-group

usrlibs-$(CONFIG_XR829) += -L$(srctree)/ekernel/drivers/drv/source/net/xradio \
	--start-group \
	-lxr829_40M \
	--end-group

usrlibs-$(CONFIG_DRIVERS_NAND_FLASH) += -L$(srctree)/ekernel/drivers/hal/source/nand_flash/ \
	--start-group \
	-lmelis-nand-sun20iw1 \
	--end-group

linkscript = -T$(prjtree)/$(TARGET_BOARD)/kernel.lds

lnkflags-y += --omagic --gc-sections -cref --entry=_start \
			-X ${linkscript} \
			-Map=$(obj)/linker.map --build-id=sha1

$(obj)/$(MOD_NAME).bin :%.bin :%.o FORCE
	$(call if_changed,objcopy)
	$(call if_changed,renameelf)
	$(Q)cp -f $@  $(prjtree)/$(TARGET_BOARD)/epos.img
	$(Q)$(OBJDUMP) -d $(<:.o=.elf) > $(obj)/$(MOD_NAME).dis
	$(Q)mksysmap $(<:.o=.elf) $(obj)/System.map
	$(Q)echo -e '\033[0;31;1m'
	$(Q)$(SIZE)  $(<:.o=.elf)
	$(Q)echo -e '\033[0m'

always     	:= $(MOD_NAME).bin

clean:mod_clean
