#include <finsh.h>
#include <hal_cmd.h>

typedef int (*cmd_function_t)(int argc, char **argv);
cmd_function_t msh_get_cmd(char *cmd, int size);

struct fork_arg_t
{
    cmd_function_t func;
    int argc;
    char *argv[FINSH_CMD_SIZE];
    char *argv_cmd;
};

static void print_usage(void)
{
    printf("Unage: fork command_name command_args\n");
}

static void fork_thread_entry(void * arg)
{
    if(arg == NULL)
    {
        return;
    }

    struct fork_arg_t * fork_arg = arg;

    fork_arg->func(fork_arg->argc, fork_arg->argv);

    if(fork_arg->argv_cmd != NULL)
    {
        free(fork_arg->argv_cmd);
    }

    free(fork_arg);
}

static int cmd_fork(int argc, const char **argv)
{
    int ret;
    rt_thread_t thread;
    cmd_function_t call;
    int priority = 18;
    uint32_t thread_size = 256 * 1024;
    char *err = NULL;
    char *command_name = NULL;
    struct fork_arg_t * fork_arg;
    char *fork_args_cmd;
    int i;

    if(argc < 2)
    {
        print_usage();
        return -1;
    }

    command_name = (char *)argv[1];

    call = msh_get_cmd(command_name, strlen(command_name));
    if(call == NULL)
    {
        printf("The command no exist!\n");
        return -1;
    }

    fork_arg = malloc(sizeof(struct fork_arg_t));
    if(fork_arg == NULL)
    {
        printf("Alloc memory failed!\n");
        return -1;
    }
    memset(fork_arg, 0, sizeof(struct fork_arg_t));

    fork_args_cmd = malloc(FINSH_CMD_SIZE);
    if(fork_args_cmd == NULL)
    {
        printf("Alloc memory failed!\n");
        free(fork_arg);
        return -1;
    }
    memset(fork_args_cmd, 0, FINSH_CMD_SIZE);

    fork_arg->argv_cmd = fork_args_cmd;
    fork_arg->func = call;
    fork_arg->argc = argc - 1;

    for(i = 1; i < argc; i++)
    {
        fork_arg->argv[i - 1] = fork_args_cmd;
        memcpy(fork_args_cmd, argv[i], strlen(argv[i]));
        fork_args_cmd += (strlen(argv[i]) + 1);
    }

    thread = rt_thread_create(command_name, fork_thread_entry, (void *)fork_arg, thread_size, priority, 10);
    ret = rt_thread_startup(thread);

    return ret;
}
FINSH_FUNCTION_EXPORT_ALIAS(cmd_fork, __cmd_fork, create a thread to run a command);
