/*
 * Remote Controller core header
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

#ifndef _RC_CORE
#define _RC_CORE

#include <ringbuffer.h>
#include <sunxi_input.h>
#include "rc-map.h"
#include <stdio.h>
#include <string.h>

#define IR_dprintk(level, fmt, ...)             \
    do {                                \
        printf("%s()%d"fmt, __func__, __LINE__, ##__VA_ARGS__); \
    } while (0)

/**
 * enum rc_driver_type - type of the RC output
 *
 * @RC_DRIVER_SCANCODE: Driver or hardware generates a scancode
 * @RC_DRIVER_IR_RAW:   Driver or hardware generates pulse/space sequences.
 *          It needs a Infra-Red pulse/space decoder
 */
enum rc_driver_type
{
    RC_DRIVER_SCANCODE = 0,
    RC_DRIVER_IR_RAW,
};

/**
 * struct rc_scancode_filter - Filter scan codes.
 * @data:   Scancode data to match.
 * @mask:   Mask of bits of scancode to compare.
 */
struct rc_scancode_filter
{
    uint32_t data;
    uint32_t mask;
};

/**
 * enum rc_filter_type - Filter type constants.
 * @RC_FILTER_NORMAL:   Filter for normal operation.
 * @RC_FILTER_WAKEUP:   Filter for waking from suspend.
 * @RC_FILTER_MAX:  Number of filter types.
 */
enum rc_filter_type
{
    RC_FILTER_NORMAL = 0,
    RC_FILTER_WAKEUP,

    RC_FILTER_MAX
};

/**
 * struct rc_dev - represents a remote control device
 * @dev: driver model's view of this device
 * @initialized: 1 if the device init has completed, 0 otherwise
 * @sysfs_groups: sysfs attribute groups
 * @input_name: name of the input child device
 * @input_phys: physical path to the input child device
 * @input_id: id of the input child device (struct input_id)
 * @driver_name: name of the hardware driver which registered this device
 * @map_name: name of the default keymap
 * @rc_map: current scan/key table
 * @lock: used to ensure we've filled in all protocol details before
 *  anyone can call show_protocols or store_protocols
 * @minor: unique minor remote control device number
 * @raw: additional data for raw pulse/space devices
 * @input_dev: the input child device used to communicate events to userspace
 * @driver_type: specifies if protocol decoding is done in hardware or software
 * @idle: used to keep track of RX state
 * @allowed_protocols: bitmask with the supported RC_BIT_* protocols
 * @enabled_protocols: bitmask with the enabled RC_BIT_* protocols
 * @allowed_wakeup_protocols: bitmask with the supported RC_BIT_* wakeup protocols
 * @enabled_wakeup_protocols: bitmask with the enabled RC_BIT_* wakeup protocols
 * @scancode_filter: scancode filter
 * @scancode_wakeup_filter: scancode wakeup filters
 * @scancode_mask: some hardware decoders are not capable of providing the full
 *  scancode to the application. As this is a hardware limit, we can't do
 *  anything with it. Yet, as the same keycode table can be used with other
 *  devices, a mask is provided to allow its usage. Drivers should generally
 *  leave this field in blank
 * @users: number of current users of the device
 * @priv: driver-specific data
 * @keylock: protects the remaining members of the struct
 * @keypressed: whether a key is currently pressed
 * @keyup_jiffies: time (in jiffies) when the current keypress should be released
 * @timer_keyup: timer for releasing a keypress
 * @last_keycode: keycode of last keypress
 * @last_protocol: protocol of last keypress
 * @last_scancode: scancode of last keypress
 * @last_toggle: toggle value of last command
 * @timeout: optional time after which device stops sending data
 * @min_timeout: minimum timeout supported by device
 * @max_timeout: maximum timeout supported by device
 * @rx_resolution : resolution (in ns) of input sampler
 * @tx_resolution: resolution (in ns) of output sampler
 * @change_protocol: allow changing the protocol used on hardware decoders
 * @change_wakeup_protocol: allow changing the protocol used for wakeup
 *  filtering
 * @open: callback to allow drivers to enable polling/irq when IR input device
 *  is opened.
 * @close: callback to allow drivers to disable polling/irq when IR input device
 *  is opened.
 * @s_tx_mask: set transmitter mask (for devices with multiple tx outputs)
 * @s_tx_carrier: set transmit carrier frequency
 * @s_tx_duty_cycle: set transmit duty cycle (0% - 100%)
 * @s_rx_carrier_range: inform driver about carrier it is expected to handle
 * @tx_ir: transmit IR
 * @s_idle: enable/disable hardware idle mode, upon which,
 *  device doesn't interrupt host until it sees IR pulses
 * @s_learning_mode: enable wide band receiver used for learning
 * @s_carrier_report: enable carrier reports
 * @s_filter: set the scancode filter
 * @s_wakeup_filter: set the wakeup scancode filter
 * @s_timeout: set hardware timeout in ns
 */
struct rc_dev
{
    char                *driver_name;
    struct ir_raw_event_ctrl    *raw;
    struct sunxi_input_dev      *input_dev;
    struct rc_map           rc_map;
    enum rc_driver_type     driver_type;
    uint64_t            allowed_protocols;
    uint64_t            enabled_protocols;
    void                *priv;
    int             keypressed;
    uint32_t            last_keycode;
    uint32_t            last_scancode;
};

#define to_rc_dev(d) container_of(d, struct rc_dev, dev)

/*
 * From rc-main.c
 * Those functions can be used on any type of Remote Controller. They
 * basically creates an input_dev and properly reports the device as a
 * Remote Controller, at sys/class/rc.
 */

/**
 * rc_allocate_device - Allocates a RC device
 *
 * returns a pointer to struct rc_dev.
 */
struct rc_dev *rc_allocate_device(void);

/**
 * rc_free_device - Frees a RC device
 *
 * @dev: pointer to struct rc_dev.
 */
void rc_free_device(struct rc_dev *dev);

/**
 * rc_register_device - Registers a RC device
 *
 * @dev: pointer to struct rc_dev.
 */
int rc_register_device(struct rc_dev *dev);

/**
 * rc_unregister_device - Unregisters a RC device
 *
 * @dev: pointer to struct rc_dev.
 */
void rc_unregister_device(struct rc_dev *dev);

/**
 * rc_open - Opens a RC device
 *
 * @rdev: pointer to struct rc_dev.
 */
int rc_open(struct rc_dev *rdev);

/**
 * rc_close - Closes a RC device
 *
 * @rdev: pointer to struct rc_dev.
 */
void rc_close(struct rc_dev *rdev);

void rc_repeat(struct rc_dev *dev);
void rc_keydown(struct rc_dev *dev, enum rc_type protocol, uint32_t scancode, uint8_t toggle);
void rc_keydown_notimeout(struct rc_dev *dev, enum rc_type protocol, uint32_t scancode, uint8_t toggle);
void rc_keyup(struct rc_dev *dev);
uint32_t rc_g_keycode_from_table(struct rc_dev *dev, uint32_t scancode);

/*
 * From rc-raw.c
 * The Raw interface is specific to InfraRed. It may be a good idea to
 * split it later into a separate header.
 */

enum raw_event_type
{
    IR_SPACE        = (1 << 0),
    IR_PULSE        = (1 << 1),
    IR_START_EVENT  = (1 << 2),
    IR_STOP_EVENT   = (1 << 3),
};

struct ir_raw_event
{
    union
    {
        uint32_t             duration;
        uint32_t             carrier;
    };
    uint8_t                      duty_cycle;

    unsigned                pulse: 1;
    unsigned                reset: 1;
    unsigned                timeout: 1;
    unsigned                carrier_report: 1;
};

#define DEFINE_IR_RAW_EVENT(event) struct ir_raw_event event = {}

static inline void init_ir_raw_event(struct ir_raw_event *ev)
{
    memset(ev, 0, sizeof(*ev));
}

#define IR_DEFAULT_TIMEOUT  MS_TO_NS(125)
#define IR_MAX_DURATION         500000000   /* 500 ms */
#define US_TO_NS(usec)      ((usec) * 1000)
#define MS_TO_US(msec)      ((msec) * 1000)
#define MS_TO_NS(msec)      ((msec) * 1000 * 1000)

void ir_raw_event_handle(struct rc_dev *dev);
int ir_raw_event_store(struct rc_dev *dev, struct ir_raw_event *ev);

static inline void ir_raw_event_reset(struct rc_dev *dev)
{
    struct ir_raw_event ev = { .reset = 1 };

    ir_raw_event_store(dev, &ev);
    ir_raw_event_handle(dev);
}

/* extract mask bits out of data and pack them into the result */
static inline uint32_t ir_extract_bits(uint32_t data, uint32_t mask)
{
    uint32_t vbit = 1, value = 0;

    do
    {
        if (mask & 1)
        {
            if (data & 1)
            {
                value |= vbit;
            }
            vbit <<= 1;
        }
        data >>= 1;
    } while (mask >>= 1);

    return value;
}

#endif /* _RC_CORE */
