#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <elf.h>

static unsigned char tmpbuffer[16 * 1024];

int main(int argc, const char *argv[])
{
	const char *elf_file = NULL;
	char cmd[512];
	Elf32_Ehdr ehdr;
	char class;
	FILE *fd1, *fd2;
	int ret;

	if (argc < 2) {
		printf("Usage: markelf elf_file\n");
		return -EFAULT;
	}

	elf_file = argv[1];

	fd1 = fopen(elf_file, "rb+");
	if (fd1 == NULL) {
		printf("open %s failed, err=%s.\n", elf_file, strerror(errno));
		return errno;
	}

	ret = fseek(fd1, 0, SEEK_SET);
	if (ret < 0) {
		printf("fseek %s failed,err=%s.\n", elf_file, strerror(errno));
		goto close_fd1;
	}

	memset(&ehdr, 0, sizeof(ehdr));

	ret = fread(&ehdr, sizeof(ehdr), 1, fd1);
	if (ret <= 0) {
		printf("fread %s failed,err=%s.\n", elf_file, strerror(errno));
		goto close_fd1;
	}

	class = ehdr.e_ident[EI_CLASS];
	if (class != ELFCLASS32) {
		printf("Unsupported class:%d.\n", class);
		errno = -EINVAL;
		goto close_fd1;
	}
	/* mark this is compress image, rproc need to wait it completion */
	ehdr.e_machine |= 0xff00;

	fd2 = fopen(".tmpdata", "wb");
	if (fd2 == NULL) {
		printf("open %s failed, err=%s.\n", ".tmpdata", strerror(errno));
		return errno;
	}

	ret = fwrite(&ehdr, sizeof(ehdr), 1, fd2);
	if (ret <= 0) {
		printf("fwrite %s failed,err=%s.\n", elf_file, strerror(errno));
		goto close_fd2;
	}

	while (!feof(fd1) > 0) {
		ret = fread(tmpbuffer, 1, sizeof(tmpbuffer), fd1);
		if (ret <= 0)
			break;

		ret = fwrite(tmpbuffer, ret, 1, fd2);
		if (ret <= 0) {
			printf("fwrite %s failed,err=%s.\n", ".tmpdata", strerror(errno));
			goto close_fd2;
		}
	}

	fflush(fd2);
	fclose(fd2);
	fflush(fd1);
	fclose(fd1);

	snprintf(cmd, sizeof(cmd), "mv %s %s;", ".tmpdata", elf_file);
	ret = system(cmd);
	if (ret == -1) {
		printf("rename .tmpdata -> %s failed.\n", elf_file);
		return -EFAULT;
	}

	return 0;

close_fd2:
	fclose(fd2);
close_fd1:
	fclose(fd1);
	return errno;
}
