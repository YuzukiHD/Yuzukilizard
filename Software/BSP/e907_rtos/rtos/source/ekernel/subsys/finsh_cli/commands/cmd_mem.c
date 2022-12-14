
#include <hal_cmd.h>

void dump_page_table(void);

static int cmd_memory_layout(int argc, const char **argv)
{
    dump_page_table();
    return 0;
}
FINSH_FUNCTION_EXPORT_ALIAS(cmd_memory_layout, __cmd_memory_layout, show memory layout);
