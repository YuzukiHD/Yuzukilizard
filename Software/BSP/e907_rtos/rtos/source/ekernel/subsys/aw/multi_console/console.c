#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <cli_console.h>

int32_t hal_uart_send(int32_t dev, const char *data, uint32_t num);

extern cli_console cli_uart_console;

static LIST_HEAD(gCliConsolelist);

static cli_console *default_console = &cli_uart_console;
static cli_console *global_console = NULL;

static int mul_console_init_flag = 0;
static cli_console_spinlock_t console_lock;

#ifdef CONFIG_MULTI_CONSOLE_DEBUG
/*
 * dump target console information
 * @console: target console
 * */
static void dump_cli_console_info(cli_console *console)
{
    struct list_head *pos;
    struct list_head *q;
    wraper_task *tmp;
    int i = 0;

    if (console == NULL)
    {
        return;
    }

    printf("name : [%s]\n", console->name);
    list_for_each_safe(pos, q, &console->task_list)
    {
        tmp = list_entry(pos, wraper_task, task_list_node);
        if (tmp)
        {
            printf("inode[%d] : %s\n", i, get_task_name(tmp->task));
            i++;
        }
    }
}

/*
 * dump all cli console information
 */
int dump_all_cli_console_info(void)
{
    struct list_head *pos;
    struct list_head *q;
    cli_console *tmp = NULL;

    list_for_each_safe(pos, q, &gCliConsolelist)
    {
        tmp = list_entry(pos, cli_console, i_list);
        dump_cli_console_info(tmp);
    }
    return 0;
}

/**
 * cli_console_debug_printf, to debug multi-console itself
 */
int cli_console_debug_printf(const char *fmt, ...)
{
    int ret;
    va_list ap;
    cpu_cpsr_t flags_cpsr;
    char *b;
    int i;

#define CLI_CONSOLE_DEBUG_BUF_SIZE 512
    static char cli_console_debug_buf[CLI_CONSOLE_DEBUG_BUF_SIZE];

    va_start(ap, fmt);
    flags_cpsr = cli_console_lock(&console_lock);
    ret = vsnprintf(cli_console_debug_buf, sizeof(cli_console_debug_buf) - 1, fmt, ap);

    b = (char *)&cli_console_debug_buf;

    if (ret > CLI_CONSOLE_DEBUG_BUF_SIZE)
    {
        ret = CLI_CONSOLE_DEBUG_BUF_SIZE;
    }

    for (i = 0; i < ret; i ++)
    {
        if (*(b + i) == '\n')
        {
            hal_uart_send(CONFIG_CLI_UART_PORT, "\r", 1);
        }
        hal_uart_send(CONFIG_CLI_UART_PORT, b + i, 1);
    }

    cli_console_unlock(&console_lock, flags_cpsr);
    va_end(ap);
    return ret;
}
#endif

int cli_console_printf(cli_console *console, const char *fmt, ...)
{
    int ret;
    va_list ap;
    cpu_cpsr_t flags_cpsr;

#define CLI_CONSOLE_DEBUG_BUF_SIZE 512
    static char cli_console_debug_buf[CLI_CONSOLE_DEBUG_BUF_SIZE];

    va_start(ap, fmt);
    flags_cpsr = cli_console_lock(&console_lock);
    ret = vsnprintf(cli_console_debug_buf, sizeof(cli_console_debug_buf) - 1, fmt, ap);

    if (ret > CLI_CONSOLE_DEBUG_BUF_SIZE)
    {
        ret = CLI_CONSOLE_DEBUG_BUF_SIZE;
    }
    cli_console_unlock(&console_lock, flags_cpsr);

    cli_console_write(console, &cli_console_debug_buf, ret);

    va_end(ap);
    return ret;
}

int register_cli_console_to_list(cli_console *console)
{
    cpu_cpsr_t flags_cpsr;

    if (console == NULL)
    {
        return -1;
    }

    INIT_LIST_HEAD(&console->i_list);

    flags_cpsr = cli_console_lock(&console_lock);
    list_add(&console->i_list, &gCliConsolelist);
    cli_console_unlock(&console_lock, flags_cpsr);

    return 0;
}

int remove_cli_console_from_list(cli_console *console)
{
    struct list_head *pos;
    struct list_head *q;
    cli_console *tmp;
    cpu_cpsr_t flags_cpsr;

    if (console == NULL)
    {
        return -1;
    }

    flags_cpsr = cli_console_lock(&console_lock);
    list_for_each_safe(pos, q, &gCliConsolelist)
    {
        tmp = list_entry(pos, cli_console, i_list);
        if (tmp == console)
        {
            list_del(pos);
            break;
        }
    }
    cli_console_unlock(&console_lock, flags_cpsr);

    return 0;
}

static cli_console *lookup_cli_console(const char *name)
{
    struct list_head *pos;
    struct list_head *q;
    cli_console *tmp;
    cpu_cpsr_t flags_cpsr;

    if (name == NULL)
    {
        return NULL;
    }

    flags_cpsr = cli_console_lock(&console_lock);
    list_for_each_safe(pos, q, &gCliConsolelist)
    {
        tmp = list_entry(pos, cli_console, i_list);
        if (!strcmp(name, tmp->name))
        {
            cli_console_unlock(&console_lock, flags_cpsr);
            return tmp;
        }
    }
    cli_console_unlock(&console_lock, flags_cpsr);

    return NULL;
}

static int cli_console_check_is_in_list(cli_console *console)
{
    struct list_head *pos;
    struct list_head *q;
    cli_console *tmp;
    int ret = 0;
    cpu_cpsr_t flags_cpsr;

    if (console == NULL)
    {
        return ret;
    }

    flags_cpsr = cli_console_lock(&console_lock);
    list_for_each_safe(pos, q, &gCliConsolelist)
    {
        tmp = list_entry(pos, cli_console, i_list);
        if (console == tmp)
        {
            ret = 1;
        }
    }
    cli_console_unlock(&console_lock, flags_cpsr);

    return ret;
}

int get_alive_console_num(void)
{
    struct list_head *pos;
    struct list_head *q;
    cli_console *tmp;
    int alive_console_n = 0;
    cpu_cpsr_t flags_cpsr;

    flags_cpsr = cli_console_lock(&console_lock);
    list_for_each_safe(pos, q, &gCliConsolelist)
    {
        tmp = list_entry(pos, cli_console, i_list);
        if (tmp->alive == 1)
        {
            alive_console_n++;
        }
    }
    cli_console_unlock(&console_lock, flags_cpsr);

    return alive_console_n;
}

cli_console *get_default_console(void)
{
    cpu_cpsr_t flags_cpsr;
    cli_console *tmp;

    flags_cpsr = cli_console_lock(&console_lock);
    if (default_console->exit_flag == 1 || default_console->init_flag == 0)
    {
        tmp = NULL;
    }
    else
    {
        tmp = default_console;
    }
    cli_console_unlock(&console_lock, flags_cpsr);

    return tmp;
}

cli_console *set_default_console(void *console)
{
    cpu_cpsr_t flags_cpsr;
    cli_console *last_console;

    flags_cpsr = cli_console_lock(&console_lock);
    last_console = default_console;
    default_console = console;
    cli_console_unlock(&console_lock, flags_cpsr);

    return last_console;
}

cli_console *get_global_console(void)
{
    cpu_cpsr_t flags_cpsr;
    cli_console *tmp;

    flags_cpsr = cli_console_lock(&console_lock);
    tmp = global_console;
    cli_console_unlock(&console_lock, flags_cpsr);

    return tmp;
}

cli_console *set_global_console(void *console)
{
    cpu_cpsr_t flags_cpsr;
    cli_console *last_console;

    flags_cpsr = cli_console_lock(&console_lock);
    last_console = global_console;
    global_console = console;
    cli_console_unlock(&console_lock, flags_cpsr);

    return last_console;
}

cli_console *get_current_console(void)
{
    cli_console *console = NULL;

    console = get_global_console();
    if (console)
    {
        return console;
    }

    console = cli_task_get_console(cli_current_task());
    return (console != NULL && console->exit_flag != 1 && console->init_flag == 1) ? console : get_default_console();
}

/*
 * get real console from cli task
 * @return cli task console
 * */
cli_console *get_clitask_console(void)
{
    return cli_task_get_console(cli_current_task());
}

void cli_console_add_task_list_node(void *current_console, void *task)
{
    cli_console *console = (cli_console *)(current_console);
    cpu_cpsr_t flags_cpsr;

    if (console == NULL || task == NULL)
    {
        return;
    }

    wraper_task *wraper_task_node = malloc(sizeof(wraper_task));
    if (wraper_task_node == NULL)
    {
        return;
    }
    memset(wraper_task_node, 0, sizeof(wraper_task));

    flags_cpsr = cli_console_lock(&console->lock);

    wraper_task_node->task = task;
    INIT_LIST_HEAD(&wraper_task_node->task_list_node);
    list_add(&wraper_task_node->task_list_node, &console->task_list);

    cli_console_unlock(&console->lock, flags_cpsr);
}

void cli_console_remove_task_list_node(cli_console *console, void *task)
{
    struct list_head *pos;
    struct list_head *q;
    wraper_task *tmp;
    cpu_cpsr_t flags_cpsr;

    if (console == NULL)
    {
        return;
    }

    flags_cpsr = cli_console_lock(&console->lock);
    list_for_each_safe(pos, q, &console->task_list)
    {
        tmp = list_entry(pos, wraper_task, task_list_node);
        if (tmp && tmp->task == task)
        {
            list_del(pos);
            free(tmp);
            break;
        }
    }
    cli_console_unlock(&console->lock, flags_cpsr);
}

static void cli_console_clear_task_list(cli_console *console)
{
    struct list_head *pos;
    struct list_head *q;
    wraper_task *tmp;
    cpu_cpsr_t flags_cpsr;

    if (console == NULL)
    {
        return;
    }

    flags_cpsr = cli_console_lock(&console->lock);
    list_for_each_safe(pos, q, &console->task_list)
    {
        tmp = list_entry(pos, wraper_task, task_list_node);
        if (tmp && tmp->task)
        {
            cli_task_clear_console(tmp->task);
            list_del(pos);
            free(tmp);
        }
    }
    cli_console_unlock(&console->lock, flags_cpsr);
}

static int new_line_format_write(
    cli_console *console,
    device_console *dev_console,
    const char *inbuf,
    char *outbuf,
    size_t input_nbytes,
    size_t output_nbytes)
{
    int size = input_nbytes;
    int wbytes = 0;

    int i = 0;
    int j = 0;

    while (size--)
    {
        if (*(inbuf + j) == '\n')
        {
            outbuf[i++] = '\r';
        }
        outbuf[i++] = *(inbuf + j);
        j++;
    }
    wbytes = dev_console->write(outbuf, output_nbytes, console->private_data);

    if (wbytes >= input_nbytes)
    {
        wbytes = input_nbytes;
    }
    return wbytes;
}

int cli_console_read(cli_console *console, void *buf, size_t nbytes)
{
    device_console *dev_console;
    int rbytes = 0;

    if (!cli_console_check_is_in_list(console))
    {
        console = get_current_console();
    }

    if (console == NULL)
    {
        console = get_current_console();
    }

    if (console && console != &cli_uart_console && !console->exit_flag)
    {
        dev_console = console->dev_console;
        if (dev_console && dev_console->read)
        {
            rbytes = dev_console->read(buf, nbytes, console->private_data);
        }
    }
    else
    {
        rbytes = uart_console_read(buf, nbytes, NULL);
    }

    return rbytes;
}

int cli_console_write(cli_console *console, const void *buf, size_t nbytes)
{
    int wbytes = -1;
    cpu_cpsr_t flags_cpsr;
    device_console *dev_console = NULL;

    if (cli_console_is_irq_context() != 0)
    {
        wbytes = uart_console_write(buf, nbytes, NULL);
        return wbytes;
    }

    if (!cli_console_check_is_in_list(console))
    {
        console = get_current_console();
    }
    if (console == NULL)
    {
        console = get_current_console();
    }

    if (console && console != &cli_uart_console && !console->exit_flag)
    {
        dev_console = console->dev_console;
        if (dev_console && dev_console->write)
        {
#if 1
            int re_num = 0, size = nbytes, re_it = 0;
            char *inbuf = (char *)buf;

            while (size--)
            {
                if (*(inbuf + re_it) == '\n')
                {
                    re_num++;
                }
                re_it++;
            }

            if (re_num + nbytes > 64)
            {
                char *outbuf = malloc(re_num + nbytes);
                if (outbuf)
                {
                    memset(outbuf, 0, re_num + nbytes);
                    wbytes = new_line_format_write(console, dev_console, inbuf, outbuf, nbytes, re_num + nbytes);
                    free(outbuf);
                }
                else
                {
                    wbytes = dev_console->write(inbuf, nbytes, console->private_data);
                }
            }
            else
            {
                char outbuf[64];
                memset(outbuf, 0, 64);
                wbytes = new_line_format_write(console, dev_console, inbuf, outbuf, nbytes, re_num + nbytes);
            }
#else
            wbytes = dev_console->write(buf, nbytes, console->private_data);
#endif
        }
    }
    else
    {
        wbytes = uart_console_write(buf, nbytes, NULL);
    }
    return wbytes;
}

int cli_console_init(cli_console *console)
{
    int ret = -1;
    device_console *dev_console = NULL;

    if (console && console->init_flag == 0)
    {
        INIT_LIST_HEAD(&console->task_list);

        register_cli_console_to_list(console);
        cli_console_lock_init(&console->lock);

        dev_console = console->dev_console;
        if (dev_console && dev_console->init)
        {
            ret = dev_console->init(console->private_data);
            if (ret == 1)
            {
                console->exit_flag = 0;
                console->init_flag = 1;
            }
        }
    }
    return ret;
}

int cli_console_deinit(cli_console *console)
{
    int ret = -1;
    device_console *dev_console = NULL;

    if (global_console != NULL && global_console == console)
    {
        set_global_console(NULL);
    }

    if (console && console->init_flag == 1)
    {
        cpu_cpsr_t flags_cpsr;

        flags_cpsr = cli_console_lock(&console->lock);
        console->exit_flag = 1;
        cli_console_unlock(&console->lock, flags_cpsr);

        cli_console_clear_task_list(console);
        remove_cli_console_from_list(console);

        dev_console = console->dev_console;
        if (dev_console && dev_console->deinit)
        {
            ret = dev_console->deinit(console->private_data);
            if (ret == 1)
            {
                console->exit_flag = 1;
                console->init_flag = 0;
            }
        }
    }
    return ret;
}

/*
 * create new cli_console
 * @param dev_console: the device console is attached to the new cli console
 * @param name: the name of new cli console
 * @param private_data: the private data of the new cli console
 */
cli_console *cli_console_create(
    device_console *dev_console,
    const char *name,
    void *private_data)
{
    cli_console *console;

    if (dev_console == NULL)
    {
        return NULL;
    }

    console = malloc(sizeof(cli_console));
    if (console == NULL)
    {
        return NULL;
    }
    memset(console, 0, sizeof(cli_console));

    console->dev_console = dev_console;
    memcpy(console->name, name, CLI_CONSOLE_MAX_NAME_LEN - 1 > strlen(name) ? strlen(name) : CLI_CONSOLE_MAX_NAME_LEN - 1);
    console->private_data = private_data;

    return console;
}

int cli_console_destory(cli_console *console)
{
    if (console)
    {
        free(console);
        return 0;
    }
    return -1;
}

/* get current console name
 * @return the name of current console name
 */
char *cli_task_get_console_name(void)
{
    cli_console *console;
    char *name = NULL;

    console = get_clitask_console();
    if (console)
    {
        name = console->name;
    }

    if (name == NULL)
    {
        name = default_console->name;
    }

    return name;
}

/* get target console name
 * @param console: the target console
 * @return the name of target console
 * */
char *cli_console_get_name(cli_console *console)
{
    char *name = NULL;
    if (console)
    {
        name = console->name;
    }
    if (name == NULL)
    {
        name = default_console->name;
    }

    return name;
}


int cli_console_check_invalid(cli_console *console)
{
    if (console == NULL)
    {
        return 0;
    }
    return 1;

}

/* create a cli console task
 * @param console: the console which is attached t the new task
 * @param stack_size: the stack size of the new cli task
 * @param priority: the priority of the new cli task
 * */
int cli_console_task_create(cli_console *console, pthread_t *tid, uint32_t stack_size, uint32_t priority)
{
    int32_t ret;

    if (!cli_console_check_invalid(console))
    {
        return -1;
    }
    cli_console_init(console);
    console->alive = 1;

    pthread_attr_t attr;
    struct sched_param sched;

    sched.sched_priority = priority;
    pthread_attr_init(&attr);
    pthread_attr_setschedparam(&attr, &sched);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    pthread_attr_setstacksize(&attr, stack_size);
    ret = pthread_create(tid, &attr, cli_console_thread_entry, console);
    if (ret != 0)
    {
        return -1;
    }
    console->task = (void *)tid;
    pthread_setname_np(*tid, cli_console_get_name(console));
    return 0;
}

/*
 * destory cli console task
 * @param console: cli_console need to be destoryed
 * @return 0
 * */
int cli_console_task_destory(cli_console *console)
{
    if (console == NULL)
    {
        return -1;
    }
    console->exit_flag = 1;
    cli_console_deinit(console);
    console->alive = 0;
    return 0;
}

/*
 * create a task for run cli command
 * @param console: cli console
 * @param stack_size: the stack size of the new task
 * @param priority: the priority of the new task
 * @param task_entry: the entry of the new task
 * @param task_args: the arguments of the new task entry
 * @param task_name: the name of the new task
 * @return the new task handle
 * */
void *cli_command_task_create(
    cli_console *console,
    uint32_t stack_size,
    uint32_t priority,
    void *task_entry,
    void *task_args,
    const char *task_name)
{
    int32_t ret;

    if (!cli_console_check_invalid(console))
    {
        return NULL;
    }
    cli_console_init(console);

    pthread_attr_t attr;
    struct sched_param sched;
    pthread_t tid;

    sched.sched_priority = priority;
    pthread_attr_init(&attr);
    pthread_attr_setschedparam(&attr, &sched);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    pthread_attr_setstacksize(&attr, stack_size);
    ret = pthread_create(&tid, &attr, task_entry, task_args);

    if (ret != 0)
    {
        return NULL;
    }

    console->alive = 1;
    console->task = (void *)tid;
    pthread_setname_np(tid, task_name);

    return (void *)tid;
}

void cli_console_current_task_destory(void)
{
    cli_console_task_destory(get_clitask_console());
}


/*
 * check cli_console exit_flag
 * @return 0/1, 0:the task should be alive; 1:the task should be deleted
 * */
int cli_console_task_check_exit_flag(void)
{
    int exit_flag = 0;
    cli_console *console;

    console = cli_task_get_console(cli_current_task());

    if (console)
    {
        exit_flag = console->exit_flag;
    }
    else
    {
        exit_flag = 0;
    }

    return exit_flag;
}

void cli_console_set_exit_flag(cli_console *console)
{
    if (console)
    {
        console->exit_flag = 1;
    }
}

void check_console_task_exit(void)
{
    cli_console *console = get_clitask_console();
    if (console && console->exit_flag == 1 && console != get_default_console())
    {
        pthread_exit(NULL);
    }
}

void task_inited_hook(void *thread)
{
    cli_task_set_console(thread, cli_task_get_console(cli_current_task()));
}

void task_delete_hook(void *thread)
{
    cli_task_set_console(thread, NULL);
}

int multiple_console_init(void)
{
    cli_console_lock_init(&console_lock);
    cli_console_init(default_console);
    return 0;
}
