
#ifndef CLI_CONSOLE_H
#define CLI_CONSOLE_H
#include <aw_list.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>

#define CLI_CONSOLE_MAX_NAME_LEN 32

typedef unsigned long cpu_cpsr_t;

typedef void (* command_callback)(void *private_data);

typedef struct _cli_console_spinlock_t
{
    pthread_spinlock_t lock;
} cli_console_spinlock_t;

typedef struct _wraper_task
{
    struct list_head task_list_node;
    void *task;
} wraper_task;

typedef struct _device_console
{
    char *name;
    int (*read)(void *buf, size_t len, void *private_data);
    int (*write)(const void *buf, size_t len, void *private_data);
    int (*init)(void *private_data);
    int (*deinit)(void *private_data);
} device_console;

typedef struct _command_line_console
{
    struct list_head i_list;
    device_console *dev_console;
    char name[CLI_CONSOLE_MAX_NAME_LEN];
    int init_flag;
    int exit_flag;
    int alive;
    void *private_data;
    void *task;
    cli_console_spinlock_t lock;
    command_callback finsh_callback;
    command_callback start_callback;
    struct list_head task_list;
} cli_console;

static int remove_cli_console_from_list(cli_console *console);

static int register_cli_console_to_list(cli_console *console);

static cli_console *lookup_cli_console(const char *name);

int get_alive_console_num(void);

int32_t create_default_console_task(uint32_t stack_size, uint32_t priority);

int remove_console(cli_console *console);
int register_console(cli_console *console);
cli_console *get_current_console(void);

cli_console *get_default_console(void);

cli_console *set_default_console(void *console);

cli_console *get_global_console(void);

cli_console *set_global_console(void *console);



//int cli_console_read(void * buf, size_t nbytes);
int cli_console_read(cli_console *console, void *buf, size_t nbytes);

//int cli_console_write(void * buf, size_t nbytes);
int cli_console_write(cli_console *console, const void *buf, size_t nbytes);

int cli_console_deinit(cli_console *console);

int cli_console_init(cli_console *console);

cli_console *cli_console_create(device_console *dev_console, const char *name, void *private_data);

int cli_console_destory(cli_console *console);

int cli_console_task_create(cli_console *console, pthread_t *tid, uint32_t stack_size, uint32_t priority);

int cli_console_task_destory(cli_console *console);

void cli_console_current_task_destory(void);

char *cli_task_get_console_name(void);

char *cli_console_get_name(cli_console *console);

int cli_console_check_invalid(cli_console *console);

int cli_console_task_check_exit_flag(void);
void cli_console_set_exit_flag(cli_console *console);

void check_console_task_exit(void);

void cli_console_remove_task_list_node(cli_console *console, void *task);

void cli_console_add_task_list_node(void *current_console, void *task);


void *cli_task_get_console(void *task_handle);
void *cli_task_set_console(void *task_handle, void *console);
int cli_task_clear_console(void *task_handle);

cli_console *cli_device_get_console(void *device_handle);

int uart_console_write(const void *buf, size_t len, void *privata_data);
int uart_console_read(void *buf, size_t len, void *privata_data);

void *cli_current_task(void);
int cli_console_is_irq_context(void);

void *cli_console_thread_entry(void *param);

cpu_cpsr_t cli_console_lock(cli_console_spinlock_t *lock);
void cli_console_unlock(cli_console_spinlock_t *lock, cpu_cpsr_t flags_cpsr);
int cli_console_lock_init(cli_console_spinlock_t *lock);

int multiple_console_init(void);

char *get_task_name(void *tcb);
cli_console *get_clitask_console(void);

int dump_all_cli_console_info(void);

int cli_console_printf(cli_console *console, const char *fmt, ...);
int cli_console_debug_printf(const char *fmt, ...);

static void *find_task(char *name);

#endif  /*CLI_CONSOLE_H*/
