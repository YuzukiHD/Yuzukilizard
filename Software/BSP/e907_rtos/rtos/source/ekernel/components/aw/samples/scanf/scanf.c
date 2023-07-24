#include <stdio.h>
#include <rtthread.h>
#include <finsh.h>
#include <finsh_api.h>


static int cmd_scanf(int argc, const char **argv)
{
    int a, b, c;
    char str[16] = {0};

    printf("Give me the value of 'int' type a,b,c and 'char' type str seperated with whitespaces:\n");
    scanf("%d%d%d%s", &a, &b, &c, str);

    printf("a=%d,b=%d,c=%d,%s\n", a, b, c, str);

    return 0;
}

FINSH_FUNCTION_EXPORT_ALIAS(cmd_scanf, __cmd_scanf, scanf test);
