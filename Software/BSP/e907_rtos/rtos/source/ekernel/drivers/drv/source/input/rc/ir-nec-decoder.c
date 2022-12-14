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

#include "rc-core-prive.h"
#include <stdio.h>

#define NEC_NBITS		32
#define NEC_UNIT		562500  /* ns */
#define NEC_HEADER_PULSE	(16 * NEC_UNIT)
#define NECX_HEADER_PULSE	(8  * NEC_UNIT) /* Less common NEC variant */
#define NEC_HEADER_SPACE	(8  * NEC_UNIT)
#define NEC_REPEAT_SPACE	(4  * NEC_UNIT)
#define NEC_BIT_PULSE		(1  * NEC_UNIT)
#define NEC_BIT_0_SPACE		(1  * NEC_UNIT)
#define NEC_BIT_1_SPACE		(3  * NEC_UNIT)
#define	NEC_TRAILER_PULSE	(1  * NEC_UNIT)
#define	NEC_TRAILER_SPACE	(10 * NEC_UNIT) /* even longer in reality */
#define NECX_REPEAT_BITS	1

enum nec_state {
	STATE_INACTIVE,
	STATE_HEADER_SPACE,
	STATE_BIT_PULSE,
	STATE_BIT_SPACE,
	STATE_TRAILER_PULSE,
	STATE_TRAILER_SPACE,
};

void nec_desc_init(nec_desc *nec)
{
   nec->state = STATE_INACTIVE;
   nec->count = 0;
   nec->bits = 0;
   nec->is_nec_x = false;
   nec->necx_repeat = false;
}

/**
 * ir_nec_decode() - Decode one NEC pulse or space
 * @dev:	the struct rc_dev descriptor of the device
 * @duration:	the struct ir_raw_event descriptor of the pulse/space
 *
 * This function returns -EINVAL if the pulse violates the state machine
 */
int ir_nec_decode(hal_cir_info_t *info, cir_raw_event ev)
{
	nec_desc *nec = &info->nec;
	static uint32_t scancode;
	uint8_t address, not_address, command, not_command;
	bool send_32bits = false;
	bool extended_nec = false;

	switch (nec->state) {

	case STATE_INACTIVE:
		if (!ev.pulse)
			break;

		if (eq_margin(ev.duration, NEC_HEADER_PULSE, NEC_UNIT * 2)) {
			nec->is_nec_x = false;
			nec->necx_repeat = false;
		} else if (eq_margin(ev.duration, NECX_HEADER_PULSE, NEC_UNIT / 2))
			nec->is_nec_x = true;
		else
			break;

		nec->count = 0;
		nec->state = STATE_HEADER_SPACE;
		return 0;

	case STATE_HEADER_SPACE:
		if (ev.pulse)
			break;

		if (eq_margin(ev.duration, NEC_HEADER_SPACE, NEC_UNIT)) {
			nec->state = STATE_BIT_PULSE;
			return 0;
		} else if (eq_margin(ev.duration, NEC_REPEAT_SPACE, NEC_UNIT / 2)) {
			nec->necx_repeat = true;
			nec->state = STATE_TRAILER_PULSE;
			return 0;
		}

		break;

	case STATE_BIT_PULSE:
		if (!ev.pulse)
			break;

		if (!eq_margin(ev.duration, NEC_BIT_PULSE, NEC_UNIT / 2))
			break;

		nec->state = STATE_BIT_SPACE;
		return 0;

	case STATE_BIT_SPACE:
		if (ev.pulse)
			break;

		if (nec->necx_repeat && nec->count == NECX_REPEAT_BITS &&
			geq_margin(ev.duration, NEC_TRAILER_SPACE, NEC_UNIT / 2)) {
				//rc_repeat(dev);
				nec->necx_repeat = true;
				nec->state = STATE_INACTIVE;
				return 0;
		} else if (nec->count > NECX_REPEAT_BITS)
			nec->necx_repeat = false;

		nec->bits <<= 1;
		if (eq_margin(ev.duration, NEC_BIT_1_SPACE, NEC_UNIT / 2))
			nec->bits |= 1;
		else if (!eq_margin(ev.duration, NEC_BIT_0_SPACE, NEC_UNIT / 2))
			break;
		nec->count++;

		if (nec->count == NEC_NBITS)
			nec->state = STATE_TRAILER_PULSE;
		else
			nec->state = STATE_BIT_PULSE;

		return 0;

	case STATE_TRAILER_PULSE:
		if (!ev.pulse)
			break;

		if (!eq_margin(ev.duration, NEC_TRAILER_PULSE, NEC_UNIT / 2))
			break;

		nec->state = STATE_TRAILER_SPACE;
		return 0;

	case STATE_TRAILER_SPACE:
		if (ev.pulse)
			break;

		if (!geq_margin(ev.duration, NEC_TRAILER_SPACE, NEC_UNIT / 2))
			break;

		if (nec->necx_repeat) {
			 //printf("repeat scancode:0x%x\n", scancode);
#if 0
			hal_cir_put_value(info, scancode);
#else
			if (info->valid == true) {
				if(info->press_cnt < 5) {
					++info->press_cnt;//Clear 0 when lifting the key

					if (info->hTimer)
						rt_timer_start(info->hTimer);
				} else {
					info->crt_value = scancode;
					//info->key_down(scancode);
					hal_cir_long_press_value(info, scancode);
					info->pre_value = info->crt_value;//Record the current key value

					if (info->hTimer)
						rt_timer_start(info->hTimer);
				}
			}
#endif
		} else {
			address     = (nec->bits >> 24) & 0xff;
			not_address = (nec->bits >> 16) & 0xff;
			command	    = (nec->bits >>  8) & 0xff;
			not_command = (nec->bits >>  0) & 0xff;

			if ((command ^ not_command) != 0xff) {
				send_32bits = true;
			}

			if ((address ^ not_address) != 0xff) {
				extended_nec = true;
			}

			if (send_32bits) {
				/* NEC transport, but modified protocol, used by at
				 * least Apple and TiVo remotes */
				scancode = nec->bits;
			} else {
				if (extended_nec) {
					/* Extended NEC */
					scancode = address << 8  |
						   not_address << 16 |
						   command;
				} else {
					/* Normal NEC */
					scancode = address << 24  |
						   not_address << 16 |
						   command << 8 |
						   not_command;
				}
			}

			if (nec->is_nec_x)
				nec->necx_repeat = true;
			if((info->user_code != IR_NOT_CHECK_USER_CODE) &&
				((scancode >> 16) & 0xffff) != (info->user_code & 0xffff))
			{
				info->valid = false;
				printf("info->user_code:0x%x error\n", scancode);
			}
			scancode = scancode >> 8 & 0xff;
			//printf("scancode:0x%x\n", scancode);
#if 0
			hal_cir_put_value(info, scancode);
#else
			if (info->valid == true) {
				info->crt_value = scancode;

				//When several keys are pressed alternately, the "Keyup" message of the previous key is sent
				if((info->pre_value != info->crt_value) && (info->pre_value != 0xff)) {
					if (info->key_up)
						info->key_up(info->pre_value); // send keyup msg,previous key

					printf("previous key up, key = %x \n", info->pre_value);
					info->pre_value = 0xff;
				}
				info->crt_value = scancode;
				info->key_down(scancode);
				info->pre_value = info->crt_value;//Record the current key value

				if (info->hTimer)
					rt_timer_start(info->hTimer);
			}
#endif
		}

		nec->state = STATE_INACTIVE;
		nec->count = 0;
		return 0;
	}

	nec->state = STATE_INACTIVE;
	nec->count = 0;
	return -1;
}
