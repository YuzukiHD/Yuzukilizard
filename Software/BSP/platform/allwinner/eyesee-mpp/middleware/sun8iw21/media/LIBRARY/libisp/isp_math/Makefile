
CFLAGS	:= -O2 -g -fPIC -W -Wall
LDFLAGS	:=

ifneq ($(findstring toolchain-sunxi-glibc,$(CC)),)
ifeq ($(V),32)
	CFLAGS	+= -mfpu=neon -mfloat-abi=hard -fdiagnostics-color=auto
endif
endif

ifneq ($(findstring toolchain-sunxi-musl,$(CC)),)
ifeq ($(V),32)
	CFLAGS	+= -mfpu=neon -mfloat-abi=hard -fdiagnostics-color=auto
endif
endif

OBJECTS := matrix.o isp_math_util.o

install: libisp_math.a libisp_math.so
	rm -rf  $(OBJECTS)
	$(STP) --strip-debug --strip-unneeded *.a
	$(STP) --strip-debug --strip-unneeded *.so
	mv *.a ../out/$(TLDIR)
	mv *.so ../out/$(TLDIR)

%.o : %.c
	$(CC) $(CFLAGS) -c -o $@ $<

all: libisp_math.a libisp_math.so

libisp_math.a: $(OBJECTS)
	$(AR) r $@ $^

libisp_math.so: $(OBJECTS)
	$(CC) -o $@ -shared $^

clean:
	-$(RM) *.o
	-$(RM) libisp_math.so
	-$(RM) libisp_math.a
