支持的命令词： { "小志开始录像", "小志停止录像", "小志拍照", "小志连拍", "小志关机" }

每次送给音频320个字节音频数据

编译链接
/home/txz/clare/V536/v536_toolchain/toolchain-sunxi-musl/toolchain/bin/arm-openwrt-linux-gccl main.c -o demo -I . -L . -luvdz -ltxzEngine

音频要求：
pcm_sample_rate = 16000

pcm_bit_width = 16

example:
	提供了已经录制好的pcm数据
