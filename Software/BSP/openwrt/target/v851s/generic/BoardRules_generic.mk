#
# Copyright (C) 2015-2016 Allwinner
#
# This is free software, licensed under the GNU General Public License v2.
# See /build/LICENSE for more information.

TARGET_CPU_VARIANT:=$(subst ",,$(word 1,$(subst +," ,$(CONFIG_CPU_TYPE))))

