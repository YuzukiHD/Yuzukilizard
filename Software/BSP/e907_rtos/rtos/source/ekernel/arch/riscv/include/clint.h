/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Western Digital Corporation or its affiliates.
 *
 * Authors:
 *   Anup Patel <anup.patel@wdc.com>
 */

#ifndef __CLINT_H__
#define __CLINT_H__

#include <stdint.h>
#include <stdbool.h>

void clint_ipi_send(uint32_t target_hart);

void clint_ipi_sync(uint32_t target_hart);

void clint_ipi_clear(uint32_t target_hart);

int clint_warm_ipi_init(void);

int clint_cold_ipi_init(unsigned long base, uint32_t hart_count);

uint64_t clint_timer_value(void);

void clint_timer_event_stop(void);

void clint_timer_event_start(uint64_t next_event);

int clint_warm_timer_init(void);

int clint_cold_timer_init(unsigned long base, uint32_t hart_count,
			  bool has_64bit_mmio);

#endif
