
#include <stdio.h>
#include <rtthread.h>
#include <dfs_posix.h>

static int cmd_ftruncate_test(int argc, char **argv)
{
    int fd = 0;
    int length = 0;
    char *end;
    int res = -1;

    if (argc < 3)
    {
        printf("Usage: ftruncate_test filename length\n");
        return -1;
    }

    length = strtoul(argv[2], &end, 0);

    fd = open(argv[1], O_RDWR);
    if (fd < 0)
    {
        printf("open file %s failed!\n", argv[1]);
        return -1;
    }

    res = ftruncate(fd, length);
    if (res == 0)
    {
        printf("ftruncate %s to %d success!\n", argv[1], length);
    }
    else
    {
        printf("ftruncate %s to %d failed!\n", argv[1], length);
    }
    close(fd);
    return 0;
}
FINSH_FUNCTION_EXPORT_ALIAS(cmd_ftruncate_test, __cmd_ftruncate_test, ftruncate test);
