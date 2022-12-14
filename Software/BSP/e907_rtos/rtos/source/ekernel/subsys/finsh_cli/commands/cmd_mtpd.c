
#include <hal_cmd.h>

int cmd_mtpd(int argc, char *argv[])
{
    int mtpd_main(int argc, char **argv);
    mtpd_main(argc, argv);
    return 0;
}

FINSH_FUNCTION_EXPORT_CMD(cmd_mtpd, mtpd, mtpd service);

