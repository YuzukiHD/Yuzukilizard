/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __MELIS_PREEMPT_H
#define __MELIS_PREEMPT_H
#include <rtdef.h>
#include <rtthread.h>
#include <arch.h>

/*
 * We put the hardirq and softirq counter into the preemption
 * counter. The bitmask has the following meaning:
 *
 * - bits 0-7 are the preemption count (max preemption depth: 256)
 * - bits 8-15 are the softirq count (max # of softirqs: 256)
 *
 * The hardirq count could in theory be the same as the number of
 * interrupts in the system, but we run all interrupt handlers with
 * interrupts disabled, so we cannot have nesting interrupts. Though
 * there are a few palaeontologic drivers which reenable interrupts in
 * the handler, so we need more than one bit here.
 *
 *         PREEMPT_MASK:    0x000000ff
 *         SOFTIRQ_MASK:    0x0000ff00
 *         HARDIRQ_MASK:    0x000f0000
 *             NMI_MASK:    0x00100000
 * PREEMPT_NEED_RESCHED:    0x80000000
 */
#define PREEMPT_BITS    8
#define SOFTIRQ_BITS    8
#define HARDIRQ_BITS    4
#define NMI_BITS        1

#define PREEMPT_SHIFT   0
#define SOFTIRQ_SHIFT   (PREEMPT_SHIFT + PREEMPT_BITS)
#define HARDIRQ_SHIFT   (SOFTIRQ_SHIFT + SOFTIRQ_BITS)
#define NMI_SHIFT       (HARDIRQ_SHIFT + HARDIRQ_BITS)

#define __IRQ_MASK(x)   ((1UL << (x))-1)

#define PREEMPT_MASK    (__IRQ_MASK(PREEMPT_BITS) << PREEMPT_SHIFT)
#define SOFTIRQ_MASK    (__IRQ_MASK(SOFTIRQ_BITS) << SOFTIRQ_SHIFT)
#define HARDIRQ_MASK    (__IRQ_MASK(HARDIRQ_BITS) << HARDIRQ_SHIFT)
#define NMI_MASK        (__IRQ_MASK(NMI_BITS)     << NMI_SHIFT)
#define PREEMPT_NEED_RESCHED    0x80000000

#define PREEMPT_OFFSET  (1UL << PREEMPT_SHIFT)
#define SOFTIRQ_OFFSET  (1UL << SOFTIRQ_SHIFT)
#define HARDIRQ_OFFSET  (1UL << HARDIRQ_SHIFT)
#define NMI_OFFSET      (1UL << NMI_SHIFT)

#define SOFTIRQ_DISABLE_OFFSET  (2 * SOFTIRQ_OFFSET)

#define PREEMPT_DISABLED    (PREEMPT_DISABLE_OFFSET + PREEMPT_ENABLED)

static inline int preempt_count(void)
{
    if (rt_thread_self() == RT_NULL)
    {
        return 0;
    }
    return rt_thread_self()->preempt_count;
}
static inline void preempt_count_add(int val)
{
    if (rt_thread_self() == RT_NULL)
    {
        return;
    }
    rt_thread_self()->preempt_count += val;
}

static inline void preempt_count_sub(int val)
{
    if (rt_thread_self() == RT_NULL)
    {
        return;
    }
    rt_thread_self()->preempt_count -= val;
}

static inline int thread_need_resched(void)
{
    if (rt_thread_self() == RT_NULL)
    {
        return 0;
    }
    return rt_thread_self()->preempt_count & PREEMPT_NEED_RESCHED;
}

static inline void thread_clear_resched(void)
{
    if (rt_thread_self() == RT_NULL)
    {
        return;
    }
    rt_thread_self()->preempt_count &= ~PREEMPT_NEED_RESCHED;
}

static inline void thread_set_resched(void)
{
    if (rt_thread_self() == RT_NULL)
    {
        return;
    }
    rt_thread_self()->preempt_count |= PREEMPT_NEED_RESCHED;
}

static inline void preempt_disable(void)
{
    preempt_count_add(1);
}

static inline void preempt_enable(void)
{
    preempt_count_sub(1);
}

#define hardirq_count() ((preempt_count() & HARDIRQ_MASK) >> HARDIRQ_SHIFT)
#define preempt_level() ((preempt_count() & PREEMPT_MASK) >> PREEMPT_SHIFT)
#define preemptible()   (preempt_count() == 0 && !irqs_disabled())

#endif /* __MELIS_PREEMPT_H */
