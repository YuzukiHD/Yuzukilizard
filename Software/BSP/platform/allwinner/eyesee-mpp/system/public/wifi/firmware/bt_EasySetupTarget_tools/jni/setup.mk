#
# Copyright 2014, Broadcom Corporation
# All Rights Reserved.
#
# This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
# the contents of this file may not be disclosed to third parties, copied
# or duplicated in any form, in whole or in part, without the prior
# written permission of Broadcom Corporation.
#

NAME := App_easy_setup

$(NAME)_SOURCES := main_rtos.c easy_setup_rtos.c easy_setup/easy_setup.c proto/bcast.c proto/neeze.c proto/akiss.c proto/changhong.c proto/jingdong.c proto/jd.c
$(NAME)_INCLUDES := ./proto ./easy_setup



WIFI_CONFIG_DCT_H := wifi_config_dct.h
