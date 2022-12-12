#############################################################################
############################ T7 configuration. #############################
#############################################################################

##---: new bdmv stream
LOCAL_CFLAGS += -DCONF_NEW_BDMV_STREAM
##---------------------------------------------------------------------------##

##---: Deinterlace V2X Support // new just support in H6/T7
LOCAL_CFLAGS += -DCONF_DI_V2X_SUPPORT
##---------------------------------------------------------------------------##

##---: Deinterlace V2X MULPLEX Support //just support in T7
LOCAL_CFLAGS += -DCONF_DI_MULPLEX
##---------------------------------------------------------------------------##

##---: new display
CONF_NEW_DISPLAY = yes
LOCAL_CFLAGS += -DCONF_NEW_DISPLAY
##---------------------------------------------------------------------------##

##---: STB // Set Top Box
CONF_PRODUCT_STB = no
#LOCAL_CFLAGS += -DCONF_PRODUCT_STB
##---------------------------------------------------------------------------##

##---: CMCC-OTT // must be STB
CONF_CMCC = no
ifeq ($(CONF_PRODUCT_STB), yes)
    ifeq (cmccwasu, $(TARGET_BUSINESS_PLATFORM))
        CONF_CMCC = yes
    endif

    ifeq ($(CONF_CMCC), yes)
         LOCAL_CFLAGS += -DCONF_CMCC
    endif
endif
##---------------------------------------------------------------------------##

##---: YUNOS-OTT // must be STB
CONF_YUNOS = no
ifeq ($(CONF_PRODUCT_STB), yes)
    ifeq (aliyunos , $(TARGET_BUSINESS_PLATFORM))
       CONF_YUNOS = yes
       endif

    ifeq ($(CONF_YUNOS), yes)
       LOCAL_CFLAGS += -DCONF_YUNOS
    endif
endif
##---------------------------------------------------------------------------##

##---: dtv
CONF_DTV = no
#LOCAL_CFLAGS += -DCONF_DTV
##---------------------------------------------------------------------------##

##---: fec-rtp
CONF_RTP = no
#LOCAL_CFLAGS += -DCONF_RTP
##---------------------------------------------------------------------------##

##---: detail track info
#LOCAL_CFLAGS += -DCONF_DETAIL_TRACK_INFO
##---------------------------------------------------------------------------##

##---: H.264@4K point to point
LOCAL_CFLAGS += -DCONF_H264_4K_P2P
##---------------------------------------------------------------------------##

##---: H.265@4K
LOCAL_CFLAGS += -DCONF_H265_4K
##---------------------------------------------------------------------------##

##---: H.265@4K point to point
LOCAL_CFLAGS += -DCONF_H265_4K_P2P
##---------------------------------------------------------------------------##

##---: kernel version
#LOCAL_CFLAGS += -DCONF_KERNEL_VER=304 # v3.04
LOCAL_CFLAGS += -DCONF_KERNEL_VER=310 # v3.10
##---------------------------------------------------------------------------##

##---: enable media boost memory
#LOCAL_CFLAGS += -DCONF_MEDIA_BOOST_MEM
##---------------------------------------------------------------------------##

##---: enable lock cpu num/freq, h5 only
#LOCAL_CFLAGS += -DCONF_MEDIA_BOOST_CPU
##---------------------------------------------------------------------------##

##---: GPU type IMG/MALI
LOCAL_CFLAGS += -DCONF_GPU_MALI
#LOCAL_CFLAGS += -DCONF_GPU_IMG
##---------------------------------------------------------------------------##

##---: define kernel bitwide, values 32/64
LOCAL_CFLAGS += -DCONF_KERN_BITWIDE=64
##---------------------------------------------------------------------------##

##---: ve phy offset
LOCAL_CFLAGS += -DCONF_VE_PHY_OFFSET=0x40000000
##---------------------------------------------------------------------------##

##---: if support 3D
#LOCAL_CFLAGS += -DCONF_3D_ENABLE
##---------------------------------------------------------------------------##

##---: if need send black frame to gpu for avoid GPU bug
#LOCAL_CFLAGS += -DCONF_SEND_BLACK_FRAME_TO_GPU
##---------------------------------------------------------------------------##

##---: sound control write data with async mode
LOCAL_CFLAGS += -DCONF_SC_WRITE_ASYNC_MODE
##---------------------------------------------------------------------------##


###################################end define####################################
