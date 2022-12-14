#ifndef _INTERRUPT_H
#define _INTERRUPT_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdbool.h>

#include <hal_interrupt.h>

#define IRQF_SHARED     0x00000080
#define IRQF_PROBE_SHARED   0x00000100
#define __IRQF_TIMER        0x00000200
#define IRQF_PERCPU     0x00000400
#define IRQF_NOBALANCING    0x00000800
#define IRQF_IRQPOLL        0x00001000
#define IRQF_ONESHOT        0x00002000
#define IRQF_NO_SUSPEND     0x00004000
#define IRQF_FORCE_RESUME   0x00008000
#define IRQF_NO_THREAD      0x00010000
#define IRQF_EARLY_RESUME   0x00020000
#define IRQF_COND_SUSPEND   0x00040000

enum irqreturn
{
    IRQ_NONE                = (0 << 0),
    IRQ_HANDLED             = (1 << 0),
    IRQ_WAKE_THREAD         = (1 << 1),
};

#define IRQACTION_NAME_MAX 16


struct irqaction
{
    hal_irq_handler_t       handler;
    void                *dev_id;
    struct irqaction    *next;
    struct irqaction    *secondary;
    unsigned int        irq;
    unsigned int        flags;
    unsigned long       thread_flags;
    unsigned long       thread_mask;
    unsigned long       irq_nums;
    const char          name[IRQACTION_NAME_MAX];
};

extern hal_irqreturn_t no_action(void *dev_id);
#define IRQ_NOTCONNECTED    (1U << 31)

extern int request_threaded_irq(unsigned int irq, hal_irq_handler_t handler,
                     hal_irq_handler_t thread_fn,
                     unsigned long flags, const char *name, void *dev);

static inline int request_irq(unsigned int irq, hal_irq_handler_t handler, unsigned long flags,
        const char *name, void *dev)
{
    return request_threaded_irq(irq, handler, NULL, flags, name, dev);
}

#endif
