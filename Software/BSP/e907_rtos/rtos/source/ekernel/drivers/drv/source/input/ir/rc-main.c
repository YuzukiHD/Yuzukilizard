/* rc-main.c - Remote Controller core module
 *
 * Copyright (C) 2009-2010 by Mauro Carvalho Chehab
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

#include "rc-core.h"
#include "rc-core-priv.h"
#include <aw_list.h>
#include <stdlib.h>
#include <hal_atomic.h>
#include <sunxi_input.h>
#include <init.h>
#include <input_event_codes.h>

/* Sizes are in bytes, 256 bytes allows for 32 entries on x64 */
#define IR_TAB_MIN_SIZE 256
#define IR_TAB_MAX_SIZE 8192
#define RC_DEV_MAX  256

/* FIXME: IR_KEYPRESS_TIMEOUT should be protocol specific */
#define IR_KEYPRESS_TIMEOUT 160
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))
hal_spinlock_t rc_lock;

static LIST_HEAD(rc_map_list);
static int key_repeat;

static struct rc_map_table empty[] =
{
    { 0x2a, KEY_COFFEE },
};

static struct rc_map_list empty_map =
{
    .map = {
        .scan    = empty,
        .size    = ARRAY_SIZE(empty),
        .rc_type = RC_TYPE_UNKNOWN, /* Legacy IR type */
        .name    = RC_MAP_EMPTY,
    }
};

int rc_map_register(struct rc_map_list *map)
{
    uint32_t __cspr;
    __cspr = hal_spin_lock_irqsave(&rc_lock);
    list_add_tail(&map->list, &rc_map_list);
    hal_spin_unlock_irqrestore(&rc_lock, __cspr);
    return 0;
}

/**
 * ir_lookup_by_scancode() - locate mapping by scancode
 * @rc_map: the struct rc_map to search
 * @scancode:   scancode to look for in the table
 * @return: index in the table, -1U if not found
 *
 * This routine performs binary search in RC keykeymap table for
 * given scancode.
 */
static unsigned int ir_lookup_by_scancode(const struct rc_map *rc_map,
        unsigned int scancode)
{
    int start = 0;
    int end = rc_map->len - 1;
    int mid;

    while (start <= end)
    {
        mid = (start + end) / 2;
        if (rc_map->scan[mid].scancode < scancode)
        {
            start = mid + 1;
        }
        else if (rc_map->scan[mid].scancode > scancode)
        {
            end = mid - 1;
        }
        else
        {
            return mid;
        }
    }

    return -1U;
}

uint32_t rc_g_keycode_from_table(struct rc_dev *dev, uint32_t scancode)
{
    struct rc_map *rc_map = &dev->rc_map;
    unsigned int keycode;
    unsigned int index;
    uint32_t __cspr;
    __cspr = hal_spin_lock_irqsave(&rc_lock);

    index = ir_lookup_by_scancode(rc_map, scancode);
    keycode = index < rc_map->len ?
              rc_map->scan[index].keycode : KEY_RESERVED;

    hal_spin_unlock_irqrestore(&rc_lock, __cspr);

    if (keycode != KEY_RESERVED)
        IR_dprintk(1, "%s: scancode 0x%04x keycode 0x%02x\n",
                   dev->driver_name, scancode, keycode);

    return keycode;
}
/**
 * ir_do_keyup() - internal function to signal the release of a keypress
 * @dev:    the struct rc_dev descriptor of the device
 * @sync:   whether or not to call input_sync
 *
 * This function is used internally to release a keypress, it must be
 * called with keylock held.
 */
static void ir_do_keyup(struct rc_dev *dev, int sync)
{
    if (!dev->keypressed)
    {
        return;
    }

    IR_dprintk(2, "keyup key 0x%04x, dev->last_scancode is 0x%04x\n", \
               dev->last_keycode, dev->last_scancode);
    input_report_key(dev->input_dev, dev->last_keycode, 0);
    if (sync)
    {
        input_sync(dev->input_dev);
    }

    dev->keypressed = 0;
    key_repeat = 0;
}

/**
 * rc_keyup() - signals the release of a keypress
 * @dev:    the struct rc_dev descriptor of the device
 *
 * This routine is used to signal that a key has been released on the
 * remote control.
 */
void rc_keyup(struct rc_dev *dev)
{
    uint32_t __cspr;
    __cspr = hal_spin_lock_irqsave(&rc_lock);
    ir_do_keyup(dev, 1);
    hal_spin_unlock_irqrestore(&rc_lock, __cspr);
}

/**
 * ir_timer_keyup() - generates a keyup event after a timeout
 * @cookie: a pointer to the struct rc_dev for the device
 *
 * This routine will generate a keyup event some time after a keydown event
 * is generated when no further activity has been detected.
 */
static void ir_timer_keyup(unsigned long cookie)
{
    struct rc_dev *dev = (struct rc_dev *)cookie;
    uint32_t __cspr;
    __cspr = hal_spin_lock_irqsave(&rc_lock);
    ir_do_keyup(dev, 1);
    hal_spin_unlock_irqrestore(&rc_lock, __cspr);
}

/**
 * rc_repeat() - signals that a key is still pressed
 * @dev:    the struct rc_dev descriptor of the device
 *
 * This routine is used by IR decoders when a repeat message which does
 * not include the necessary bits to reproduce the scancode has been
 * received.
 */
void rc_repeat(struct rc_dev *dev)
{
    uint32_t __cspr;
    __cspr = hal_spin_lock_irqsave(&rc_lock);

    sunxi_input_event(dev->input_dev, EV_MSC, MSC_SCAN, dev->last_scancode);
    input_sync(dev->input_dev);

    hal_spin_unlock_irqrestore(&rc_lock,__cspr);
}

/**
 * ir_do_keydown() - internal function to process a keypress
 * @dev:    the struct rc_dev descriptor of the device
 * @protocol:   the protocol of the keypress
 * @scancode:   the scancode of the keypress
 * @keycode:    the keycode of the keypress
 * @toggle:     the toggle value of the keypress
 *
 * This function is used internally to register a keypress, it must be
 * called with keylock held.
 */
static void ir_do_keydown(struct rc_dev *dev, enum rc_type protocol,
                          uint32_t scancode, uint32_t keycode, uint8_t toggle)
{
    int new_event = (!dev->keypressed        ||
                     ((dev->last_scancode & 0xffffffUL) != (scancode & 0xffffffUL)) ||
                     !key_repeat);

    if (new_event && dev->keypressed)
    {
        ir_do_keyup(dev, 0);
    }

    sunxi_input_event(dev->input_dev, EV_MSC, MSC_SCAN, scancode);

    if (new_event && keycode != KEY_RESERVED)
    {
        /* Register a keypress */
        dev->keypressed = 1;
        dev->last_scancode = scancode;
        dev->last_keycode = keycode;

        input_report_key(dev->input_dev, keycode, 1);
    }
    input_sync(dev->input_dev);
}

/**
 * rc_keydown() - generates input event for a key press
 * @dev:    the struct rc_dev descriptor of the device
 * @protocol:   the protocol for the keypress
 * @scancode:   the scancode for the keypress
 * @toggle:     the toggle value (protocol dependent, if the protocol doesn't
 *              support toggle values, this should be set to zero)
 *
 * This routine is used to signal that a key has been pressed on the
 * remote control.
 */
void rc_keydown(struct rc_dev *dev, enum rc_type protocol, uint32_t scancode, uint8_t toggle)
{
    uint32_t __cspr;

    uint32_t keycode = rc_g_keycode_from_table(dev, scancode);
    __cspr = hal_spin_lock_irqsave(&rc_lock);

    ir_do_keydown(dev, protocol, scancode, keycode, toggle);

    hal_spin_unlock_irqrestore(&rc_lock, __cspr);
}


static void rc_dev_release(struct rc_dev *dev)
{
    if (!dev)
    {
        return;
    }
    free(dev);
}

struct rc_dev *rc_allocate_device(void)
{
    struct rc_dev *dev;

    dev = malloc(sizeof(*dev));
    if (!dev)
    {
        return NULL;
    }

    dev->input_dev = sunxi_input_allocate_device();
    if (!dev->input_dev)
    {
        free(dev);
        return NULL;
    }

    return dev;
}

int rc_register_device(struct rc_dev *dev)
{
    int rc;

    if (!dev)
    {
        return -1;
    }

    input_set_bit(EV_KEY, dev->input_dev->evbit);
    input_set_bit(EV_REP, dev->input_dev->evbit);
    input_set_bit(EV_MSC, dev->input_dev->evbit);
    input_set_bit(MSC_SCAN, dev->input_dev->mscbit);

    dev->input_dev->name = dev->driver_name;

    rc = sunxi_input_register_device(dev->input_dev);
    goto out_input;

    if (dev->driver_type == RC_DRIVER_IR_RAW)
    {
        rc = ir_raw_event_register(dev);
        if (rc < 0)
        {
            goto out_input;
        }
    }

    return 0;

out_input:

    dev->input_dev = NULL;
    return rc;
}

void rc_unregister_device(struct rc_dev *dev)
{
    if (!dev)
    {
        return;
    }

    if (dev->driver_type == RC_DRIVER_IR_RAW)
    {
        ir_raw_event_unregister(dev);
    }

    dev->input_dev = NULL;

    rc_free_device(dev);
    rc_dev_release(dev);
}

static int rc_core_init(void)
{
    rc_map_register(&empty_map);

    return 0;
}

subsys_initcall(rc_core_init);
