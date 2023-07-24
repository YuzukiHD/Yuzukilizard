#include <stdio.h>
#include <rtthread.h>
#include <dfs_posix.h>

static int cmd_dup_test(int argc, char **argv)
{
    int fd = 0;
    int newfd = 0;
    int length = 0;
    char *end;
    int res = -1;
    char data[16];

    if (argc < 2)
    {
        printf("Usage: dup_test filename\n");
        return -1;
    }

    fd = open(argv[1], O_RDONLY);
    if (fd < 0)
    {
        printf("open file %s failed!\n", argv[1]);
        return -1;
    }

    newfd = dup(fd);
    if (newfd >=0)
    {
        printf("duplicate %d to %d success!\n", fd, newfd);
    }
    else
    {
        printf("duplicate %d to %d failed!\n", fd, newfd);
    }

    memset(data, 0, sizeof(data));
    read(fd, data, sizeof(data) - 1);
    printf("read data from %d : %s\r\n", fd, data);

    res = close(fd);
    if (res != 0)
    {
        printf("close file failed!!!\r\n");
    }

    memset(data, 0, sizeof(data));
    read(newfd, data, sizeof(data) - 1);
    printf("close %d, read data from %d : %s\r\n", fd, newfd, data);
    res = close(newfd);
    if (res != 0)
    {
        printf("close file failed!!!\r\n");
    }

    return 0;
}
FINSH_FUNCTION_EXPORT_ALIAS(cmd_dup_test, __cmd_dup_test, dup test);

static int cmd_dup2_test(int argc, char **argv)
{
    int fd = 0;
    int newfd = 0;
    int length = 0;
    char *end;
    int res = -1;
    char data[16];

    if (argc < 2)
    {
        printf("Usage: dup_test filename\n");
        return -1;
    }

    fd = open(argv[1], O_RDONLY);
    if (fd < 0)
    {
        printf("open file %s failed!\n", argv[1]);
        return -1;
    }

    newfd = dup2(fd, fd + 2);
    if (newfd >=0)
    {
        printf("duplicate %d to %d success!\n", fd, newfd);
    }
    else
    {
        printf("duplicate %d to %d failed!\n", fd, newfd);
        close(fd);
        return -1;
    }

    memset(data, 0, sizeof(data));
    read(fd, data, sizeof(data) - 1);
    printf("read data from %d : %s\r\n", fd, data);

    res = close(fd);
    if (res != 0)
    {
        printf("close file failed!!!\r\n");
    }

    memset(data, 0, sizeof(data));
    read(newfd, data, sizeof(data) - 1);
    printf("close %d, read data from %d : %s\r\n", fd, newfd, data);
    res = close(newfd);
    if (res != 0)
    {
        printf("close file failed!!!\r\n");
    }

    return 0;
}
FINSH_FUNCTION_EXPORT_ALIAS(cmd_dup2_test, __cmd_dup2_test, dup2 test);

