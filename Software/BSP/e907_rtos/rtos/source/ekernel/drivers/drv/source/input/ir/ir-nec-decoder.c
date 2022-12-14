/* ir-nec-decoder.c - handle NEC IR Pulse/Space protocol
 *
 * Copyright (C) 2010 by Mauro Carvalho Chehab <mchehab@redhat.com>
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

#include <init.h>
#include <stdio.h>
#include "rc-core-priv.h"

#define NEC_NBITS       32
#define NEC_UNIT        562500  /* ns */
#define NEC_HEADER_PULSE    (16 * NEC_UNIT)
#define NECX_HEADER_PULSE   (8  * NEC_UNIT) /* Less common NEC variant */
#define NEC_HEADER_SPACE    (8  * NEC_UNIT)
#define NEC_REPEAT_SPACE    (4  * NEC_UNIT)
#define NEC_BIT_PULSE       (1  * NEC_UNIT)
#define NEC_BIT_0_SPACE     (1  * NEC_UNIT)
#define NEC_BIT_1_SPACE     (3  * NEC_UNIT)
#define NEC_TRAILER_PULSE   (1  * NEC_UNIT)
#define NEC_TRAILER_SPACE   (10 * NEC_UNIT) /* even longer in reality */
#define NECX_REPEAT_BITS    1

enum nec_state
{
    STATE_INACTIVE,
    STATE_HEADER_SPACE,
    STATE_BIT_PULSE,
    STATE_BIT_SPACE,
    STATE_TRAILER_PULSE,
    STATE_TRAILER_SPACE,
};

/**
 * ir_nec_decode() - Decode one NEC pulse or space
 * @dev:    the struct rc_dev descriptor of the device
 * @duration:   the struct ir_raw_event descriptor of the pulse/space
 *
 * This function returns -EINVAL if the pulse violates the state machine
 */
static int ir_nec_decode(struct rc_dev *dev, struct ir_raw_event ev)
{
    struct nec_dec *data = &dev->raw->nec;
    u32 scancode;
    u8 address, not_address, command, not_command;
    int send_32bits = 0;

    if (!(dev->enabled_protocols & RC_BIT_NEC))
    {
        return 0;
    }

    if (!is_timing_event(ev))
    {
        if (ev.reset)
        {
            data->state = STATE_INACTIVE;
        }
        return 0;
    }

    IR_dprintk(2, "NEC decode started at state %d (%luus %s)\n",
               data->state, TO_US(ev.duration), TO_STR(ev.pulse));

    switch (data->state)
    {

        case STATE_INACTIVE:
            if (!ev.pulse)
            {
                break;
            }

            if (eq_margin(ev.duration, NEC_HEADER_PULSE, NEC_UNIT * 2))
            {
                data->is_nec_x = 0;
                data->necx_repeat = 0;
            }
            else if (eq_margin(ev.duration, NECX_HEADER_PULSE, NEC_UNIT / 2))
            {
                data->is_nec_x = 1;
            }
            else
            {
                break;
            }

            data->count = 0;
            data->state = STATE_HEADER_SPACE;
            return 0;

        case STATE_HEADER_SPACE:
            if (ev.pulse)
            {
                break;
            }

            if (eq_margin(ev.duration, NEC_HEADER_SPACE, NEC_UNIT))
            {
                data->state = STATE_BIT_PULSE;
                return 0;
            }
            else if (eq_margin(ev.duration, NEC_REPEAT_SPACE, NEC_UNIT / 2))
            {
                if (!dev->keypressed)
                {
                    IR_dprintk(1, "Discarding last key repeat: event after key up\n");
                }
                else
                {
                    rc_repeat(dev);
                    IR_dprintk(1, "Repeat last key\n");
                    data->state = STATE_TRAILER_PULSE;
                }
                return 0;
            }

            break;

        case STATE_BIT_PULSE:
            if (!ev.pulse)
            {
                break;
            }

            if (!eq_margin(ev.duration, NEC_BIT_PULSE, NEC_UNIT / 2))
            {
                break;
            }

            data->state = STATE_BIT_SPACE;
            return 0;

        case STATE_BIT_SPACE:
            if (ev.pulse)
            {
                break;
            }

            if (data->necx_repeat && data->count == NECX_REPEAT_BITS &&
                geq_margin(ev.duration,
                           NEC_TRAILER_SPACE, NEC_UNIT / 2))
            {
                IR_dprintk(1, "Repeat last key\n");
                rc_repeat(dev);
                data->state = STATE_INACTIVE;
                return 0;

            }
            else if (data->count > NECX_REPEAT_BITS)
            {
                data->necx_repeat = 0;
            }

            data->bits <<= 1;
            if (eq_margin(ev.duration, NEC_BIT_1_SPACE, NEC_UNIT / 2))
            {
                data->bits |= 1;
            }
            else if (!eq_margin(ev.duration, NEC_BIT_0_SPACE, NEC_UNIT / 2))
            {
                break;
            }
            data->count++;

            if (data->count == NEC_NBITS)
            {
                data->state = STATE_TRAILER_PULSE;
            }
            else
            {
                data->state = STATE_BIT_PULSE;
            }

            return 0;

        case STATE_TRAILER_PULSE:
            if (!ev.pulse)
            {
                break;
            }

            if (!eq_margin(ev.duration, NEC_TRAILER_PULSE, NEC_UNIT / 2))
            {
                break;
            }

            data->state = STATE_TRAILER_SPACE;
            return 0;

        case STATE_TRAILER_SPACE:
            if (ev.pulse)
            {
                break;
            }

            if (!geq_margin(ev.duration, NEC_TRAILER_SPACE, NEC_UNIT / 2))
            {
                break;
            }

            address     = bitrev8((data->bits >> 24) & 0xff);
            not_address = bitrev8((data->bits >> 16) & 0xff);
            command     = bitrev8((data->bits >>  8) & 0xff);
            not_command = bitrev8((data->bits >>  0) & 0xff);

            if ((command ^ not_command) != 0xff)
            {
                IR_dprintk(1, "NEC checksum error: received 0x%08lx\n",
                           data->bits);
                send_32bits = 1;
            }

            if (send_32bits)
            {
                /* NEC transport, but modified protocol, used by at
                 * least Apple and TiVo remotes */
                scancode = data->bits;
                IR_dprintk(1, "NEC (modified) scancode 0x%08lx\n", scancode);
            }
            else
            {
                /* Extended NEC */
                scancode = address << 8  |
                           not_address << 16 |
                           command;
                IR_dprintk(1, "NEC scancode 0x%06lx\n", scancode);
            }

            if (data->is_nec_x)
            {
                data->necx_repeat = 1;
            }

            rc_keydown(dev, RC_TYPE_NEC, scancode, 0);
            data->state = STATE_INACTIVE;
            return 0;
    }

    IR_dprintk(1, "NEC decode failed at count %d state %d (%luus %s)\n",
               data->count, data->state, TO_US(ev.duration), TO_STR(ev.pulse));
    data->state = STATE_INACTIVE;
    return -1;
}

static struct ir_raw_handler nec_handler =
{
    .protocols  = RC_BIT_NEC,
    .decode     = ir_nec_decode,
};

static int ir_nec_decode_init(void)
{
    ir_raw_handler_register(&nec_handler);

    printf("IR NEC protocol handler initialized\n");
    return 0;
}

static int ir_nec_decode_exit(void)
{
    ir_raw_handler_unregister(&nec_handler);
    return 0;
}

device_initcall(ir_nec_decode_init);
