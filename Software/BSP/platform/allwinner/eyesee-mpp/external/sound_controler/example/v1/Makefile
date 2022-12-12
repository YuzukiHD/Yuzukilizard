CC = /home/sunrongmeng/workspace/perf-tina-v459/prebuilt/gcc/linux-x86/arm/toolchain-sunxi-musl/toolchain/bin/arm-openwrt-linux-muslgnueabi-gcc
.PHONY: all
all:
	$(CC) main.c -o demo -I. -L. -luvdz -ltxzEngine
.PHONY: clean
clean:
	rm -rf demo
