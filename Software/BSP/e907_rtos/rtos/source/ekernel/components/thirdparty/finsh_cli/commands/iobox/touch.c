#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <fcntl.h>

#include <rtthread.h>

static void show_help(void)
{
    printf("Usage: touch <file>...\n");
}

static int touch_do(const char *file)
{
    int fd;

    fd = open(file, O_WRONLY | O_CREAT);
    if (fd < 0)
    {
        printf("open %s failed - %s\n", file, strerror(errno));
        return -1;
    }

    close(fd);
    return 0;
}

static int touch_main(int argc, char **argv)
{
    int index;
    int ret = 0;

    if (argc < 2)
    {
        show_help();
        return -1;
    }

    for (index = 1; index < argc; index++)
    {
        char *file = argv[index];

        if (access(file, F_OK))
        {
            ret |= touch_do(file);
        }
    }
    return ret;
}
FINSH_FUNCTION_EXPORT_CMD(touch_main, __cmd_touch, create empty file);
