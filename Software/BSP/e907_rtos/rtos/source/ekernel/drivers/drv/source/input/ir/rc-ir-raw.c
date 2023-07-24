/* rc-ir-raw.c - handle IR pulse/space events
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
#include <stdlib.h>
#include <rtthread.h>
#include <ringbuffer.h>
#include "rc-core-priv.h"
#include <aw_list.h>

static LIST_HEAD(ir_raw_handler_list);

static int ir_raw_event_thread(void *data)
{
    struct ir_raw_event ev;
    struct ir_raw_handler *handler;
    struct ir_raw_event_ctrl *raw = (struct ir_raw_event_ctrl *)data;
    while (!rt_ringbuffer_data_len(&raw->rx_ringbuffer))
    {
        rt_mutex_take(raw->rx_ringbuffer_lock, RT_WAITING_FOREVER);
        rt_ringbuffer_get(&raw->rx_ringbuffer, (rt_uint8_t *)&ev, sizeof(struct ir_raw_event));
        rt_mutex_release(raw->rx_ringbuffer_lock);
        list_for_each_entry(handler, &ir_raw_handler_list, list)
        if (raw->dev->enabled_protocols & handler->protocols ||
            !handler->protocols)
        {
            handler->decode(raw->dev, ev);
        }
    }
    return 0;
}

/**
 * ir_raw_event_handle() - schedules the decoding of stored ir data
 * @dev:    the struct rc_dev device descriptor
 *
 * This routine will tell rc-core to start decoding stored ir data.
 */
void ir_raw_event_handle(struct rc_dev *dev)
{
    unsigned long flags;

    if (!dev->raw)
    {
        return;
    }

    ir_raw_event_thread(dev->raw);
}

/**
 * ir_raw_event_store() - pass a pulse/space duration to the raw ir decoders
 * @dev:    the struct rc_dev device descriptor
 * @ev:     the struct ir_raw_event descriptor of the pulse/space
 *
 * This routine (which may be called from an interrupt context) stores a
 * pulse/space duration for the raw ir decoding state machines. Pulses are
 * signalled as positive values and spaces as negative values. A zero value
 * will reset the decoding state machines.
 */
int ir_raw_event_store(struct rc_dev *dev, struct ir_raw_event *ev)
{
    if (!dev->raw)
    {
        return -1;
    }

    IR_dprintk(2, "sample: (%05ldus %s)\n",
               TO_US(ev->duration), TO_STR(ev->pulse));
    rt_mutex_take(dev->raw->rx_ringbuffer_lock, RT_WAITING_FOREVER);
    rt_ringbuffer_put(&dev->raw->rx_ringbuffer, (rt_uint8_t *)ev, sizeof(struct ir_raw_event));
    rt_mutex_release(dev->raw->rx_ringbuffer_lock);

    return 0;
}

/*
 * Used to (un)register raw event clients
 */
int ir_raw_event_register(struct rc_dev *dev)
{
    if (!dev)
    {
        return -1;
    }

    dev->raw = malloc(sizeof(*dev->raw));
    if (!dev->raw)
    {
        return -1;
    }

    dev->raw->dev = dev;
    dev->raw->ptr = malloc(sizeof(struct ir_raw_event) * MAX_IR_EVENT_SIZE);
    if (dev->raw->ptr)
    {
        rt_ringbuffer_init(&dev->raw->rx_ringbuffer, dev->raw->ptr, sizeof(struct ir_raw_event) * MAX_IR_EVENT_SIZE);
    }
    else
    {
        rt_kprintf("ir_raw_event: no memory\n");
        goto out;
    }
    dev->raw->rx_ringbuffer_lock = rt_mutex_create("ir_rx", RT_IPC_FLAG_FIFO);
    return 0;

out:
    free(dev->raw);
    return -1;
}

void ir_raw_event_unregister(struct rc_dev *dev)
{
    rt_mutex_delete(dev->raw->rx_ringbuffer_lock);
    free(dev->raw->ptr);
    free(dev->raw);
}


/*
 * Extension interface - used to register the IR decoders
 */

int ir_raw_handler_register(struct ir_raw_handler *ir_raw_handler)
{
    struct ir_raw_event_ctrl *raw;

    list_add_tail(&ir_raw_handler->list, &ir_raw_handler_list);

    return 0;
}

void ir_raw_handler_unregister(struct ir_raw_handler *ir_raw_handler)
{
    struct ir_raw_event_ctrl *raw;
    uint64_t protocols = ir_raw_handler->protocols;

    list_del(&ir_raw_handler->list);
}
