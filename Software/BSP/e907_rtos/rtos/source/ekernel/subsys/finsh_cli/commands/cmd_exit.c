#include <cli_console.h>
#include <hal_cmd.h>

static int cmd_exit(int argc, char *argv[])
{
#ifdef CONFIG_SUBSYS_MULTI_CONSOLE
    cli_console_current_task_destory();
#endif
    return 0;
}
FINSH_FUNCTION_EXPORT_ALIAS(cmd_exit, __cmd_exit, Exit current console);
