#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <rtthread.h>
#include <hal_mutex.h>

hal_mutex_t mutex = NULL;
void sleep(int seconds);

static int cmd_osal_mutex_test(int argc, char **argv)
{
    int res = -1;

    if (mutex == NULL)
    {
        res = hal_mutex_create(&mutex);
        if (res == 0)
        {
            printf("create mutex success!\n");
        }
        else
        {
            return -1;
        }
    }
    printf("begin lock\n");
    hal_mutex_lock(&mutex);
    printf("lock, then sleep\n");
    sleep(100);
    hal_mutex_unlock(&mutex);
    printf("finsh lock\n");
    return 0;
}
FINSH_FUNCTION_EXPORT_ALIAS(cmd_osal_mutex_test, __cmd_osal_mutex_test, osal mutex test);

