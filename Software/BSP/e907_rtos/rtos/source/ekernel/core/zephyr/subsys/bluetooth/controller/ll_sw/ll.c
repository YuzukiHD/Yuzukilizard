/*
 * Copyright (c) 2016-2017 Nordic Semiconductor ASA
 * Copyright (c) 2016 Vinayak Kariappa Chettimada
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/types.h>
#include <string.h>

#include <soc.h>
#include <device.h>
#include <clock_control.h>
#ifdef CONFIG_CLOCK_CONTROL_NRF5
#include <drivers/clock_control/nrf5_clock_control.h>
#endif
#include <bluetooth/hci.h>

#define BT_DBG_ENABLED IS_ENABLED(CONFIG_BT_DEBUG_HCI_DRIVER)
#include "common/log.h"

#include "hal/cpu.h"
#include "hal/cntr.h"
#include "hal/rand.h"
#include "hal/ccm.h"
#include "hal/radio.h"
#include "hal/debug.h"

#include "util/util.h"
#include "util/mem.h"
#include "util/mayfly.h"

#include "ticker/ticker.h"

#include "pdu.h"
#include "ctrl.h"
#include "ctrl_internal.h"
#include "ll.h"
#include "ll_filter.h"

/* Global singletons */

/* memory for storing Random number */
#define RAND_THREAD_THRESHOLD 4  /* atleast access address */
#define RAND_ISR_THRESHOLD    12 /* atleast encryption div. and iv */
static u8_t MALIGN(4) rand_context[4 + RAND_THREAD_THRESHOLD + 1];
static u8_t MALIGN(4) rand_isr_context[4 + RAND_ISR_THRESHOLD + 1];

#if defined(CONFIG_SOC_FLASH_NRF5_RADIO_SYNC)
#define FLASH_TICKER_NODES        1 /* No. of tickers reserved for flashing */
#define FLASH_TICKER_USER_APP_OPS 1 /* No. of additional ticker operations */
#else
#define FLASH_TICKER_NODES        0
#define FLASH_TICKER_USER_APP_OPS 0
#endif

#define TICKER_NODES              (RADIO_TICKER_NODES + FLASH_TICKER_NODES)
#define TICKER_USER_APP_OPS       (RADIO_TICKER_USER_APP_OPS + \
                                   FLASH_TICKER_USER_APP_OPS)
#define TICKER_USER_OPS           (RADIO_TICKER_USER_OPS + \
                                   FLASH_TICKER_USER_APP_OPS)

/* memory for ticker nodes/instances */
static u8_t MALIGN(4) _ticker_nodes[TICKER_NODES][TICKER_NODE_T_SIZE];

/* memory for users/contexts operating on ticker module */
static u8_t MALIGN(4) _ticker_users[MAYFLY_CALLER_COUNT][TICKER_USER_T_SIZE];

/* memory for user/context simultaneous API operations */
static u8_t MALIGN(4) _ticker_user_ops[TICKER_USER_OPS][TICKER_USER_OP_T_SIZE];

/* memory for Bluetooth Controller (buffers, queues etc.) */
static u8_t MALIGN(4) _radio[LL_MEM_TOTAL];

static struct k_sem *sem_recv;

static struct
{
    u8_t pub_addr[BDADDR_SIZE];
    u8_t rnd_addr[BDADDR_SIZE];
} _ll_context;

void mayfly_enable_cb(u8_t caller_id, u8_t callee_id, u8_t enable)
{
    (void)caller_id;

    LL_ASSERT(callee_id == MAYFLY_CALL_ID_1);

    if (enable)
    {
        hal_enable_irq(SWI4_IRQn);
    }
    else
    {
        hal_disable_irq(SWI4_IRQn);
    }
}

u32_t mayfly_is_enabled(u8_t caller_id, u8_t callee_id)
{
    (void)caller_id;

    if (callee_id == MAYFLY_CALL_ID_0)
    {
        return irq_is_enabled(RTC0_IRQn);
    }
    else if (callee_id == MAYFLY_CALL_ID_1)
    {
        return irq_is_enabled(SWI4_IRQn);
    }

    LL_ASSERT(0);

    return 0;
}

u32_t mayfly_prio_is_equal(u8_t caller_id, u8_t callee_id)
{
#if (RADIO_TICKER_USER_ID_WORKER_PRIO == RADIO_TICKER_USER_ID_JOB_PRIO)
    return (caller_id == callee_id) ||
           ((caller_id == MAYFLY_CALL_ID_0) &&
            (callee_id == MAYFLY_CALL_ID_1)) ||
           ((caller_id == MAYFLY_CALL_ID_1) &&
            (callee_id == MAYFLY_CALL_ID_0));
#else
    return caller_id == callee_id;
#endif
}

void mayfly_pend(u8_t caller_id, u8_t callee_id)
{
    (void)caller_id;

    switch (callee_id)
    {
        case MAYFLY_CALL_ID_0:
            NVIC_SetPendingIRQ(RTC0_IRQn);
            break;

        case MAYFLY_CALL_ID_1:
            NVIC_SetPendingIRQ(SWI4_IRQn);
            break;

        case MAYFLY_CALL_ID_PROGRAM:
        default:
            LL_ASSERT(0);
            break;
    }
}

void radio_active_callback(u8_t active)
{
}

void radio_event_callback(void)
{
    k_sem_give(sem_recv);
}

ISR_DIRECT_DECLARE(radio_nrf5_isr)
{
    isr_radio();

    ISR_DIRECT_PM();
    return 1;
}

static void rtc0_nrf5_isr(void *arg)
{
    u32_t compare0, compare1;

    /* store interested events */
    compare0 = NRF_RTC0->EVENTS_COMPARE[0];
    compare1 = NRF_RTC0->EVENTS_COMPARE[1];

    /* On compare0 run ticker worker instance0 */
    if (compare0)
    {
        NRF_RTC0->EVENTS_COMPARE[0] = 0;

        ticker_trigger(0);
    }

    /* On compare1 run ticker worker instance1 */
    if (compare1)
    {
        NRF_RTC0->EVENTS_COMPARE[1] = 0;

        ticker_trigger(1);
    }

    mayfly_run(MAYFLY_CALL_ID_0);
}

static void rng_nrf5_isr(void *arg)
{
    isr_rand(arg);
}

static void swi4_nrf5_isr(void *arg)
{
    mayfly_run(MAYFLY_CALL_ID_1);
}

int ll_init(struct k_sem *sem_rx)
{
    struct device *clk_k32;
    struct device *clk_m16;
    u32_t err;

    sem_recv = sem_rx;

    /* TODO: bind and use RNG driver */
    rand_init(rand_context, sizeof(rand_context), RAND_THREAD_THRESHOLD);
    rand_isr_init(rand_isr_context, sizeof(rand_isr_context),
                  RAND_ISR_THRESHOLD);

    clk_k32 = device_get_binding(CONFIG_CLOCK_CONTROL_NRF5_K32SRC_DRV_NAME);
    if (!clk_k32)
    {
        return -ENODEV;
    }

    clock_control_on(clk_k32, (void *)CLOCK_CONTROL_NRF5_K32SRC);

    /* TODO: bind and use counter driver */
    cntr_init();

    mayfly_init();

    _ticker_users[MAYFLY_CALL_ID_0][0] = RADIO_TICKER_USER_WORKER_OPS;
    _ticker_users[MAYFLY_CALL_ID_1][0] = RADIO_TICKER_USER_JOB_OPS;
    _ticker_users[MAYFLY_CALL_ID_2][0] = 0;
    _ticker_users[MAYFLY_CALL_ID_PROGRAM][0] = TICKER_USER_APP_OPS;

    ticker_init(RADIO_TICKER_INSTANCE_ID_RADIO, TICKER_NODES,
                &_ticker_nodes[0], MAYFLY_CALLER_COUNT, &_ticker_users[0],
                TICKER_USER_OPS, &_ticker_user_ops[0]);

    clk_m16 = device_get_binding(CONFIG_CLOCK_CONTROL_NRF5_M16SRC_DRV_NAME);
    if (!clk_m16)
    {
        return -ENODEV;
    }

    err = radio_init(clk_m16, CLOCK_CONTROL_NRF5_K32SRC_ACCURACY,
                     RADIO_CONNECTION_CONTEXT_MAX,
                     RADIO_PACKET_COUNT_RX_MAX,
                     RADIO_PACKET_COUNT_TX_MAX,
                     RADIO_LL_LENGTH_OCTETS_RX_MAX,
                     RADIO_PACKET_TX_DATA_SIZE, &_radio[0], sizeof(_radio));
    if (err)
    {
        BT_ERR("Required RAM size: %d, supplied: %u.", err,
               sizeof(_radio));
        return -ENOMEM;
    }

    ll_filter_reset(true);

    IRQ_DIRECT_CONNECT(NRF5_IRQ_RADIO_IRQn, CONFIG_BT_CTLR_WORKER_PRIO,
                       radio_nrf5_isr, 0);
    IRQ_CONNECT(NRF5_IRQ_RTC0_IRQn, CONFIG_BT_CTLR_WORKER_PRIO,
                rtc0_nrf5_isr, NULL, 0);
    IRQ_CONNECT(NRF5_IRQ_SWI4_IRQn, CONFIG_BT_CTLR_JOB_PRIO, swi4_nrf5_isr,
                NULL, 0);
    IRQ_CONNECT(NRF5_IRQ_RNG_IRQn, 1, rng_nrf5_isr, NULL, 0);

    hal_enable_irq(NRF5_IRQ_RADIO_IRQn);
    hal_enable_irq(NRF5_IRQ_RTC0_IRQn);
    hal_enable_irq(NRF5_IRQ_SWI4_IRQn);
    hal_enable_irq(NRF5_IRQ_RNG_IRQn);

    return 0;
}

void ll_timeslice_ticker_id_get(u8_t *const instance_index, u8_t *const user_id)
{
    *user_id = (TICKER_NODES - FLASH_TICKER_NODES); /* The last index in the total tickers */
    *instance_index = RADIO_TICKER_INSTANCE_ID_RADIO;
}

u8_t *ll_addr_get(u8_t addr_type, u8_t *bdaddr)
{
    if (addr_type > 1)
    {
        return NULL;
    }

    if (addr_type)
    {
        if (bdaddr)
        {
            memcpy(bdaddr, _ll_context.rnd_addr, BDADDR_SIZE);
        }

        return _ll_context.rnd_addr;
    }

    if (bdaddr)
    {
        memcpy(bdaddr, _ll_context.pub_addr, BDADDR_SIZE);
    }

    return _ll_context.pub_addr;
}

void ll_addr_set(u8_t addr_type, u8_t const *const bdaddr)
{
    if (addr_type)
    {
        memcpy(_ll_context.rnd_addr, bdaddr, BDADDR_SIZE);
    }
    else
    {
        memcpy(_ll_context.pub_addr, bdaddr, BDADDR_SIZE);
    }
}
