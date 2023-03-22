CC:=arm-openwrt-linux-gcc

INCLUDES += -I$(STAGING_DIR)/usr/include
LIB_PATH := $(STAGING_DIR)/usr/lib
LIBS += -L$(LIB_PATH)

LIBS += -ljpeg

BIN=yolov3-tiny

SRCS=${wildcard *.c}
OBJS=$(addsuffix .o, $(basename $(SRCS)))

.c.o:
	$(CC) $(CFLAGS) -c $<

.c:
	$(CC) $(CFLAGS) $< -o $@ -lm

all: $(BIN) 

$(BIN): $(OBJS)
	$(CC) $(CFLAGS) $(LFLAGS) $(EXTRALFLAGS) $(OBJS) $(LIBS) -g -o $@ -l VIPlite 

clean:
	rm -rf *.o
	rm -rf $(BIN)
	rm -rf *~
