#CC ?= gcc
CC = arm-linux-gnueabi-gcc

src_list := main_linux.c easy_setup_linux.c easy_setup/easy_setup.c easy_setup/scan.c proto/bcast.c proto/neeze.c proto/akiss.c proto/changhong.c
obj_list := $(src_list:%.c=%.o)

CFLAGS := -Ieasy_setup -Iproto

.PHONY: setup

setup:$(obj_list)
	$(CC) -o $@ $^
	
%.o:%.c
	$(CC) -c $(CFLAGS) -o $@ $^

clean:
	rm $(obj_list) setup


