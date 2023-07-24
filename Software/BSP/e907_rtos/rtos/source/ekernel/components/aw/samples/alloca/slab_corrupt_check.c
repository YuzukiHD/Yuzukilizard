#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <rtthread.h>

static char *buf1;
static char *buf2;
static char *pagebuf1;
static char *pagebuf2;

static int cmd_zone_malloc_check_test(int argc, const char **argv)
{
    buf1 = malloc(40);
    if (!buf1)
    {
        return -1;
    }

    buf2 = malloc(40);
    if (!buf2)
    {
        free(buf1);
        buf1 = NULL;
        return -1;
    }
    memset(buf1, 0x55, 60);
    return 0;
}
FINSH_FUNCTION_EXPORT_ALIAS(cmd_zone_malloc_check_test, __cmd_zone_malloc_check_test, enter panic mode);

static int cmd_zone_free_check_test(int argc, const char **argv)
{
    if (buf1)
    {
        free(buf1);
        buf1 = NULL;
    }
    if (buf2)
    {
        free(buf2);
        buf2 = NULL;
    }
    return 0;
}
FINSH_FUNCTION_EXPORT_ALIAS(cmd_zone_free_check_test, __cmd_zone_free_check_test, enter panic mode);

static int cmd_page_malloc_test(int argc, const char **argv)
{
    pagebuf1 = rt_page_alloc(2);
    if (!pagebuf1)
    {
        return -1;
    }

    pagebuf2 = rt_page_alloc(2);
    if (!pagebuf2)
    {
        return -1;
    }

    memset(pagebuf1, 0x55, 2 * 4096 + 20);
    return 0;
}
FINSH_FUNCTION_EXPORT_ALIAS(cmd_page_malloc_test, __cmd_page_malloc_test, enter panic mode);

static int cmd_page_free_check_test(int argc, const char **argv)
{
    if (pagebuf1)
    {
        rt_page_free(pagebuf1, 2);
        pagebuf1 = NULL;
    }
    if (pagebuf2)
    {
        rt_page_free(pagebuf2, 2);
        pagebuf2 = NULL;
    }
    return 0;
}
FINSH_FUNCTION_EXPORT_ALIAS(cmd_page_free_check_test, __cmd_page_free_check_test, enter panic mode);

