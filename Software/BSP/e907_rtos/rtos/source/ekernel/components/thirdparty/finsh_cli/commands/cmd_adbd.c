#include <hal_cmd.h>

int cmd_adbd(int argc, char *argv[])
{
    int adbd_init(int argc, char **argv);
    adbd_init(argc, argv);
    return 0;
}

FINSH_FUNCTION_EXPORT_CMD(cmd_adbd, adbd, adbd service);
