.PHONY: all clean

CC:=gcc
LD:=gcc
CFLAGS:=-Wall `pkg-config --cflags libunwind`
LDFLAGS:=`pkg-config --libs libunwind` -rdynamic
LDLIBS:=-ldl

srcs:=un.c
objsdir:=objdir
objs:=$(srcs:%.c=$(objsdir)/%.o)
exe:=un

all: $(exe)

$(exe): $(objs)
#/usr/local/lib/libunwind.so.8.0.1 /usr/lib/i386-linux-gnu/libdl.so
	$(LD) -o $@ $^ $(LDFLAGS) $(LDLIBS)

$(objs): | $(objsdir)
$(objsdir):
	mkdir -p $@

$(objsdir)/%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	-rm -rf $(exe) $(objsdir)
