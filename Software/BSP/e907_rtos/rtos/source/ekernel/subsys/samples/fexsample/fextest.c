#include <typedef.h>
#include <rtthread.h>
#include <dfs_posix.h>
#include <finsh_api.h>
#include <finsh.h>
#include <debug.h>
#include <pipe.h>
#include <log.h>
#include <rthw.h>
#include <arch.h>
#include <script.h>

static void *phandle;
static int cmd_fexconfig(int argc, const char **argv)
{
    int mainkey_count;
    int voltage;

    phandle = script_get_handle();
    if(phandle == NULL)
    {
        __err("fatal error, fex not initialized.");
        software_break();
        return -1;
    }

    mainkey_count = script_parser_mainkey_count(phandle);

    __log("mainkey %d.", mainkey_count);

    int ret = script_parser_fetch(phandle, "power_sply", "dcdc1_vol", &voltage, 1);

    if(ret == 0)
    {
        __log("volatage %d.", voltage);
    }
    else
    {
       __err("fatal error.");
    }

    return 0;
}

FINSH_FUNCTION_EXPORT_ALIAS(cmd_fexconfig, __cmd_fexconfig, fexconfig test);



