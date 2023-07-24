#include <stdlib.h>
#include <rtthread.h>
#include <stdio.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
/*#include <unistd.h>*/
#include <getopt.h>

#include <dfs_posix.h>

static void show_help(void)
{
    printf("Usage: mount -t type -d device -p path -f flag\n");
}

static int cmd_mount(int argc, const char **argv)
{
    int ch = 0;
    char *type = NULL;
    char *device = NULL;
    char *path = NULL;
    int ret = 0;
    char *end = NULL;
    unsigned long rwflag = 0;

    while ((ch = getopt(argc, (char **)argv, "t:d:p:f:h")) != -1)
    {
        switch (ch)
        {
            case 't':
                type = optarg;
                break;
            case 'd':
                device = optarg;
                break;
            case 'p':
                path = optarg;
                break;
            case 'f':
                rwflag = strtoul(optarg, &end, 0);
                break;
            case 'h':
                show_help();
                return -1;
            default:
                break;
        }
    }

    if (type == NULL || device == NULL || path == NULL)
    {
        show_help();
        return -1;
    }

    ret = mount(device, path, type, rwflag, 0);
    if (ret)
    {
        printf("%s -t %s -d %s -p %s failed\n", argv[0], type, device, path);
        return -1;
    }
    return 0;
}
FINSH_FUNCTION_EXPORT_ALIAS(cmd_mount, __cmd_mount, mount filesystem);

static int cmd_umount(int argc, const char **argv)
{
    int ret = 0;

    if (argc < 2)
    {
        printf("Usage: umount path\n");
        return -1;
    }

    ret = umount(argv[1]);
    if (ret)
    {
        printf("umount %s failed\n", argv[1]);
        return -1;
    }
    return 0;
}
FINSH_FUNCTION_EXPORT_ALIAS(cmd_umount, __cmd_umount, umount filesystem);
