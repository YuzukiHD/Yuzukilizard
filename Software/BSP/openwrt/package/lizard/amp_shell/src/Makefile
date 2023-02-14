TARGET = amp_shell
FLAGS = -DCONFIG_PRINT_CLI_PROMPT

SOURCE = main.c shell.c command.c amp_device.c rawdev/rpmsg.c

all: $(TARGET) install

$(TARGET):
	$(CC) -g -O $(SOURCE) $(FLAGS) $(CFLAGS) $(LDFLAGS) -o $@

install:
	mkdir -p $(INSTALL_PREFIX)/usr/bin
	cp $(TARGET) $(INSTALL_PREFIX)/usr/bin

clean:
	rm -rf $(OBJS) $(TARGET)

.PHONY: all clean install
