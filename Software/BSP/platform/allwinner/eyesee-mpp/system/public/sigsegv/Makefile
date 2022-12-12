.PHONY: all clean

CC:=arm-linux-gnueabi-gcc
# CC:=g++
CFLAGS:=-Wall -g -fPIC
LDFLAGS:=-rdynamic -shared
LDLIBS:=-ldl

srcs:=sigsegv.c
objsdir:=objdir
objs:=$(srcs:%.c=$(objsdir)/%.o)

exe:=libsigsegv.so

all: $(exe)

$(exe): $(objs)
	$(CC) -o $@ $^ $(LDFLAGS) $(LDLIBS)

$(objs): | $(objsdir)
$(objsdir):
	@mkdir -p $@

$(objsdir)/%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	-rm -rf $(exe) $(objsdir)
