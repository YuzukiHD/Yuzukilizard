
MODULE_TOP = $(TARGET_TOP)/frameworks/av/media/libcedarc
ifdef LICHEE_PLATFORM_OS
ifeq ($(LICHEE_PLATFORM_OS),$(filter $(LICHEE_PLATFORM_OS),eyeseelinux linux))
MODULE_TOP = $(TARGET_TOP)/middleware/media/LIBRARY/libcedarc
else ifeq ($(LICHEE_PLATFORM_OS),tina)
MODULE_TOP = $(PACKAGE_TOP)/media/LIBRARY/libcedarc
endif
endif

product = v853
#product = $(shell echo $(MPP_PRODUCT) | cut -d '_' -f 1)

########## configure CONF_ANDROID_VERSION ##########
android_version = $(shell echo $(PLATFORM_VERSION) | cut -c 1)

config_file = null

ifeq ($(product), octopus)
    ifeq ($(android_version), 5)
        config_file = config_pad_A83_lollipop.mk
    else ifeq ($(android_version), 7)
	config_file = config_pad_A83_Nougat.mk
    endif
else ifeq ($(product), astar)
    ifeq ($(android_version), 7)
        config_file = config_pad_A33_Nougat.mk
    endif
else ifeq ($(product), tulip)
    ifeq ($(android_version), 5)
        config_file = config_pad_A64_lollipop.mk
    else ifeq ($(android_version), 6)
        config_file = config_pad_A64_Marshmallow.mk
    else ifeq ($(android_version), 7)
        config_file = config_pad_A64_Nougat.mk
    else ifeq ($(android_version), 8)
        config_file = config_pad_A64_Oreo.mk
    endif
else ifeq ($(product), dolphin)
    ifeq ($(android_version), 4)
        config_file = config_box_H3_kitkat.mk
    else ifeq ($(android_version), 7)
    	config_file = config_box_H3_Nougat.mk
    endif
else ifeq ($(product), rabbit)
    ifeq ($(android_version), 5)
        config_file = config_box_H64_lollipop.mk
    endif
else ifeq ($(product), cheetah)
    ifeq ($(android_version), 4)
        config_file = config_box_H5_kitkat.mk
    else ifeq ($(android_version), 5)
        config_file = config_box_H5_lollipop.mk
    else ifeq ($(android_version), 7)
        config_file = config_box_H5_Nougat.mk
    endif
else ifeq ($(product), eagle)
    ifeq ($(android_version), 5)
        config_file = config_box_H8_kitkat.mk
    else ifeq ($(android_version), 7)
        config_file = config_box_H8_Nougat.mk
    endif
else ifeq ($(product), t3)
    ifeq ($(android_version), 6)
        config_file = config_cdr_T3_Marshmallow.mk
	else ifeq ($(android_version), 7)
        config_file = config_cdr_T3_Nougat.mk
    else ifeq ($(android_version), 8)
        config_file = config_cdr_T3_Oreo.mk
    endif
else ifeq ($(product), t7)
	ifeq ($(android_version), 7)
        config_file = config_cdr_T7_Nougat.mk
	else ifeq ($(android_version), 8)
		config_file = config_cdr_T7_Oreo.mk
    endif
else ifeq ($(product), kylin)
    ifeq ($(android_version), 7)
        config_file = config_vr_A80_Nougat.mk
    endif
else ifeq ($(product), petrel)
    ifeq ($(android_version), 4)
        config_file = config_box_H6_kitkat.mk
    else ifeq ($(android_version), 7)
        config_file = config_box_H6_Nougat.mk    
    else ifeq ($(android_version), 8)
        config_file = config_box_H6_Oreo.mk
    endif
else ifeq ($(product), neptune)
    ifeq ($(android_version), 7)
        config_file = config_vr_A63_Nougat.mk
    endif
else ifeq ($(product), uranus)
    ifeq ($(android_version), 7)
        config_file = config_pad_A63_Nougat.mk
    else ifeq ($(android_version), 8)
        config_file = config_pad_A63_Oreo.mk
    endif
else ifeq ($(product), t8)
    ifeq ($(android_version), 7)
        config_file = config_pad_A83_Nougat.mk
     else ifeq ($(android_version), 8)
         config_file = config_pad_T8_Oreo.mk
     endif
else ifeq ($(product), venus)
    ifeq ($(android_version), 8)
        config_file = config_pad_A50_Oreo.mk
    endif
    ifeq ($(android_version), 9)
        config_file = config_pad_A50_Pie.mk
    endif
else ifneq ($(filter $(product), v5 v316 v536 v313 v163 v833 v831 v533 v853 v853s r853s v851s v851se qg3101),)
    config_file = config_sdv_v316_tina.mk
endif

ifeq ($(config_file), null)
    $(error "can not find config_file: product=$(product), androd_version=$(android_version)")
else
    $(warning "config file: $(config_file)")
    include $(MODULE_TOP)/config/$(config_file)
endif
