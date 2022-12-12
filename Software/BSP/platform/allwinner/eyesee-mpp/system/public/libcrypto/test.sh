#!/bin/sh
mk -B;
mk build_test;
pack_img
cp -f /home1/qinjian/workspace/V5/lichee/tools/pack/sun8iw12p1_linux_dvb_uart0.img ~/fw/
