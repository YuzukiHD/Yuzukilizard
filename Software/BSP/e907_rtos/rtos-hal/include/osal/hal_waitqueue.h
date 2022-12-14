#ifndef SUNXI_HAL_WAITQUEUE_H
#define SUNXI_HAL_WAITQUEUE_H

#ifdef __cplusplus
extern "C"
{
#endif
#include <stddef.h>
#include <stdint.h>

#ifdef CONFIG_KERNEL_FREERTOS
#include <FreeRTOS.h>
#include <task.h>
#include <aw_list.h>

#include <hal_sem.h>

#define HAL_WQ_FLAG_CLEAN    0x00
#define HAL_WQ_FLAG_WAKEUP   0x01

struct hal_waitqueue
{
    uint32_t flag;
    hal_sem_t sem;
    struct list_head waiting_list;
};
typedef struct hal_waitqueue hal_waitqueue_t;

struct hal_waitqueue_node;
typedef int (*hal_waitqueue_func_t)(struct hal_waitqueue_node *wait, void *key);

struct hal_waitqueue_node
{
    void* polling_thread;
    struct list_head list;

    hal_waitqueue_func_t wakeup;
    uint32_t key;
};
typedef struct hal_waitqueue_node hal_waitqueue_node_t;

int __wqueue_default_wake(struct hal_waitqueue_node *wait, void *key);

void hal_waitqueue_add(hal_waitqueue_t *queue, struct hal_waitqueue_node *node);
void hal_waitqueue_remove(struct hal_waitqueue_node *node);
int  hal_waitqueue_wait(hal_waitqueue_t *queue, int condition, int timeout);
void hal_waitqueue_wakeup(hal_waitqueue_t *queue, void *key);
void hal_waitqueue_init(hal_waitqueue_t *queue);
void hal_waitqueue_deinit(hal_waitqueue_t *queue);

#define HAL_DEFINE_WAIT_NODE_FUNC(name, function)       \
    struct hal_waitqueue_node name = {                  \
        xTaskGetCurrentTaskHandle(),                    \
        LIST_HEAD_INIT(((name).list)),                  \
        \
        function,                                       \
        0                                               \
    }

#define HAL_DEFINE_WAIT_NODE(name) HAL_DEFINE_WAIT_NODE_FUNC(name, __wqueue_default_wake)

#elif defined(CONFIG_RTTKERNEL)
#else
#error "can not support the RTOS!!"
#endif

#endif

