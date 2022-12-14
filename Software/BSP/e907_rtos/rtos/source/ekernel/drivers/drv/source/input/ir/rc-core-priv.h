/*
 * Remote Controller core raw events header
 *
 * Copyright (C) 2010 by Mauro Carvalho Chehab
 *
 * This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

#ifndef _RC_CORE_PRIV
#define _RC_CORE_PRIV

#include <list.h>
#include <ringbuffer.h>
#include "bitrev.h"
#include "rc-core.h"

/* Define the max number of pulse/space transitions to buffer */
#define MAX_IR_EVENT_SIZE   64
/*
 * Divide positive or negative dividend by positive divisor and round
 * to closest integer. Result is undefined for negative divisors and
 * for negative dividends if the divisor variable type is unsigned.
 */
#define DIV_ROUND_CLOSEST(x, divisor)(          \
{                           \
    typeof(x) __x = x;              \
    typeof(divisor) __d = divisor;          \
    (((typeof(x))-1) > 0 ||             \
     ((typeof(divisor))-1) > 0 || (__x) > 0) ?  \
    (((__x) + ((__d) / 2)) / (__d)) :   \
    (((__x) - ((__d) / 2)) / (__d));    \
}                           \
                                     )

struct ir_raw_handler
{
    struct list_head list;

    uint64_t protocols; /* which are handled by this handler */
    int (*decode)(struct rc_dev *dev, struct ir_raw_event event);
};

struct ir_raw_event_ctrl
{
    struct list_head list;
    struct rc_dev *dev;
    struct rt_ringbuffer rx_ringbuffer;
    rt_mutex_t rx_ringbuffer_lock;
    rt_uint8_t *ptr;
    struct ir_raw_event prev_ev;
    struct nec_dec
    {
        int state;
        unsigned count;
        uint32_t bits;
        int is_nec_x;
        int necx_repeat;
    } nec;
};

/* macros for IR decoders */
static inline int geq_margin(unsigned d1, unsigned d2, unsigned margin)
{
    return d1 > (d2 - margin);
}

static inline int eq_margin(unsigned d1, unsigned d2, unsigned margin)
{
    return ((d1 > (d2 - margin)) && (d1 < (d2 + margin)));
}

static inline int is_transition(struct ir_raw_event *x, struct ir_raw_event *y)
{
    return x->pulse != y->pulse;
}

static inline void decrease_duration(struct ir_raw_event *ev, unsigned duration)
{
    if (duration > ev->duration)
    {
        ev->duration = 0;
    }
    else
    {
        ev->duration -= duration;
    }
}

/* Returns true if event is normal pulse/space event */
static inline int is_timing_event(struct ir_raw_event ev)
{
    return !ev.carrier_report && !ev.reset;
}

#define TO_US(duration)         DIV_ROUND_CLOSEST((duration), 1000)
#define TO_STR(is_pulse)        ((is_pulse) ? "pulse" : "space")

/*
 * Routines from rc-raw.c to be used internally and by decoders
 */
int ir_raw_event_register(struct rc_dev *dev);
void ir_raw_event_unregister(struct rc_dev *dev);
int ir_raw_handler_register(struct ir_raw_handler *ir_raw_handler);
void ir_raw_handler_unregister(struct ir_raw_handler *ir_raw_handler);
/*
 * Decoder initialization code
 *
 * Those load logic are called during ir-core init, and automatically
 * loads the compiled decoders for their usage with IR raw events
 */

#endif /* _RC_CORE_PRIV */
