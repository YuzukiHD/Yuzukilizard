#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#define     MISK_PARTITON_NAND	"/dev/nandf"
#define     MISK_PARTITON_NOR	"/dev/mtdblock6"

int reboot_efex(void)
{
	int flash_handle = 0;
	int len = 0;
	char command[32] = "efex";

	memset(command, 0x00, sizeof(command));
	strcpy(command, "efex");

	if (access(MISK_PARTITON_NOR, F_OK) == 0) {
		printf("[nor] reboot efex\n");
		flash_handle = open(MISK_PARTITON_NOR, O_RDWR);
	} else if (access(MISK_PARTITON_NAND, F_OK) == 0) {
		printf("[nand] reboot efex\n");
		flash_handle = open(MISK_PARTITON_NAND, O_RDWR);
	} else {
		printf("[nor] try nor reboot efex\n");
		flash_handle = open(MISK_PARTITON_NOR, O_RDWR);
	}
	if (flash_handle < 0) {
		fprintf(stderr,"%s:open fail,line=%d\n",__func__,__LINE__);
		return -1;
	}

	len = write(flash_handle, &command[0], sizeof(command));
	if (len < 0) {
		fprintf(stderr,"%s:write fail,line=%d\n",__func__,__LINE__);
		return -1;
	}
	sync();
	close(flash_handle);
	system("reboot");

    return 0;
}

int main(int argc, char *argv[])
{
	reboot_efex();

	return 0;
}
