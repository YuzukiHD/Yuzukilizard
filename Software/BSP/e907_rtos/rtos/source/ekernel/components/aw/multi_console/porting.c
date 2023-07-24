#include <stdlib.h>
#include <stdio.h>
#include <cli_console.h>
#include <hal_thread.h>
#include <hal_interrupt.h>

#define CONSOLE_CONSOLE_DEVICE_USER_DATA_ID 1

void finsh_thread_entry(void *parameter);

void *cli_current_task(void)
{
    return kthread_self();
}

void *cli_console_thread_entry(void *param)
{
    finsh_thread_entry(param);
    pthread_exit(NULL);
    return NULL;
}

void *cli_task_get_console(void *task_handle)
{
    rt_thread_t task = (rt_thread_t)task_handle;
    void *cli_console = NULL;
    if (task)
    {
        if (task->user_data[CONSOLE_CONSOLE_DEVICE_USER_DATA_ID])
        {
            cli_console = (void *)task->user_data[CONSOLE_CONSOLE_DEVICE_USER_DATA_ID];
            return cli_console;
        }
    }
    return NULL;
}

void *cli_task_set_console(void *task_handle, void *console)
{
    rt_thread_t task = (rt_thread_t)task_handle;
    void *last_console = NULL;
    cpu_cpsr_t flags_cpsr;

    if (task)
    {
        last_console = (void *)task->user_data[CONSOLE_CONSOLE_DEVICE_USER_DATA_ID];

        if (last_console)
        {
            cli_console_remove_task_list_node(last_console, task_handle);
        }

        task->user_data[CONSOLE_CONSOLE_DEVICE_USER_DATA_ID] = console;

        if (task->user_data[CONSOLE_CONSOLE_DEVICE_USER_DATA_ID])
        {
            cli_console_add_task_list_node(console, task_handle);
        }
    }
    return last_console;
}

int cli_console_is_irq_context(void)
{
    return hal_interrupt_get_nest();
}

int cli_task_clear_console(void *task_handle)
{
    rt_thread_t task = (rt_thread_t)task_handle;
    if (task)
    {
        if (task->user_data[CONSOLE_CONSOLE_DEVICE_USER_DATA_ID])
        {
            return 0;
        }
    }
    return 0;
}

cpu_cpsr_t cli_console_lock(cli_console_spinlock_t *lock)
{
    return pthread_spin_lock_irqsave(&lock->lock);
}

void cli_console_unlock(cli_console_spinlock_t *lock, cpu_cpsr_t flags_cpsr)
{
    pthread_spin_unlock_irqrestore(&lock->lock, flags_cpsr);
}

int cli_console_lock_init(cli_console_spinlock_t *lock)
{
    pthread_spin_init(&lock->lock, 0);
    return 0;
}

char *get_task_name(void *tcb)
{
    if (tcb)
    {
        rt_thread_t task = (rt_thread_t)tcb;
        return task->name;
    }
    return "null";
}

#ifdef CONFIG_MULTI_CONSOLE_DEBUG
int cmd_dump_console(int argc, char *argv)
{
    dump_all_cli_console_info();
    return 0;
}
FINSH_FUNCTION_EXPORT_CMD(cmd_dump_console, __cmd_dump_console, Dump Console Debug Information);
#endif

#ifdef CONFIG_MULTI_CONSOLE_REDIRECT_CMD
static void *find_task(char *name)
{
    return rt_thread_find(name);
}

static int cmd_redirect(int argc, char **argv)
{
    cli_console *current_console = NULL;
    void *task = NULL;

    if (argc < 2)
    {
        printf("Usage:\n");
        printf("\tredirect all      : redirect all logs to current console\n");
        printf("\tredirect stop     : stop redirect all logs to current console\n");
        printf("\tredirect taskname : redirect the target task logs to current console\n");
        return 0;
    }

    current_console = get_clitask_console();

    if (!strcmp(argv[1], "all"))
    {
        set_global_console(current_console);
        return 0;
    }

    if (!strcmp(argv[1], "stop"))
    {
        set_global_console(NULL);
        return 0;
    }

    task = find_task(argv[1]);

    if (task != NULL)
    {
        cli_task_set_console(task, current_console);
    }
    else
    {
        printf("can not find the task\n");
    }

    return 0;
}
FINSH_FUNCTION_EXPORT_CMD(cmd_redirect, __cmd_redirect, redirect logs to current console);
#endif
