
#notic: change CC by guixing
#CC = $(CROSS_COMPILE)gcc
CC = arm-linux-gnueabi-gcc

IFLAGS := -g -Wall

DFLAGS += -DLIB

#DFLAGS += -DIL_BIGENDIAN

obj-dhd_priv = dhd_priv.o

DHD_PRIV := dhd_priv

CFLAGS += $(IFLAGS) $(DFLAGS)

all:  $(DHD_PRIV)

$(DHD_PRIV): $(obj-dhd_priv)
	$(CC) -o $@ $(obj-dhd_priv)

clean:
	-rm -f $(DHD_HPRIV) *.o
