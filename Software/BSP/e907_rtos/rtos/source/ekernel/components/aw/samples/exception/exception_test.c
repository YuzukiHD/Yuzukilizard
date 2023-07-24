#include <typedef.h>
#include <rtthread.h>
#include <rthw.h>
#include <arch.h>
#include <log.h>
#include <dfs_posix.h>
#include <finsh_api.h>
#include <finsh.h>
#include <debug.h>

int exception_sample(int argc, char **argv)
{
    asm volatile(".word 0xffffffff");
    return 0;
}

FINSH_FUNCTION_EXPORT_ALIAS(exception_sample, __cmd_exception_sample, timeslice_sample test);


