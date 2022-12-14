#include <stdlib.h>
#include <stdio.h>
#include <rtthread.h>

#include <kapi.h>
#include <module/mod_defs.h>

int cmd_insmod(int argc, char **argv)
{
    void *mod_hdle = NULL;
    uint32_t mod_mid = 0;

    if (argc != 2)
    {
        printf("Usage: insmod [mod_path]");
        return -1;
    }

    mod_mid = esMODS_MInstall(argv[1], 0);
    if (!mod_mid)
    {
        printf("install mod(%s) failed!\n", argv[1]);
        return -1;
    }

    mod_hdle = esMODS_MOpen(mod_mid, 0);
#if 0
    if (!mod_hdle)
    {
        printf("open mod(%s) failed!\n", argv[1]);
        return -1;
    }

    if (mod_hdle)
    {
        esMODS_MClose(mod_hdle);
        mod_hdle = NULL;
    }
#endif

    if (mod_mid)
    {
        esMODS_MUninstall(mod_mid);
        mod_mid = 0;
    }
    return 0;
}
FINSH_FUNCTION_EXPORT_ALIAS(cmd_insmod, __cmd_insmod, install mod);
