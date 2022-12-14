#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <rtthread.h>
#include <hal_timer.h>

static void timer_func(void *param)
{
    printf("data: %s\n", (char *)param);
}

static int cmd_osal_timer_test(int argc, char **argv)
{
    osal_timer_t timer;

    timer = osal_timer_create("test", timer_func, "test", 100, OSAL_TIMER_FLAG_PERIODIC);
    osal_timer_start(timer);
    sleep(100);
    return 0;
}

FINSH_FUNCTION_EXPORT_ALIAS(cmd_osal_timer_test, __cmd_osal_timer_test, osal timer test);
