#ifndef SUNXI_HAL_EVENT_H
#define SUNXI_HAL_EVENT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

#ifdef CONFIG_KERNEL_FREERTOS
#include <FreeRTOS.h>
#include <task.h>
#include <aw_list.h>

#define HAL_EVENT_OPTION_CLEAR		(1 << 0)
#define HAL_EVENT_OPTION_AND		(1 << 1)
#define HAL_EVENT_OPTION_OR			(1 << 2)

typedef StaticEventGroup_t hal_event;
typedef EventGroupHandle_t hal_event_t;
typedef EventBits_t hal_event_bits_t;


#elif defined(CONFIG_RTTKERNEL)
#include <rtdef.h>
#include <rtthread.h>

#define HAL_EVENT_OPTION_CLEAR		RT_EVENT_FLAG_CLEAR
#define HAL_EVENT_OPTION_AND		RT_EVENT_FLAG_AND
#define HAL_EVENT_OPTION_OR         RT_EVENT_FLAG_OR

typedef rt_event_t hal_event_t;
typedef rt_uint32_t hal_event_bits_t;

#else
#error "can not support the RTOS!!"
#endif

int hal_event_init(hal_event_t ev);
int hal_event_datach(hal_event_t ev);

hal_event_t hal_event_create(void);
hal_event_t hal_event_create_initvalue(int init_value);
int hal_event_delete(hal_event_t ev);

/*
 * wait for events
 * @ev: hal_event_t handler
 * @evs: events
 * @option: it can be HAL_EVENT_OPTION_*
 * @timeout:wait time(ms)
 *
 * @return: return new events value if success,otherwise return negative value
 */
hal_event_bits_t hal_event_wait(hal_event_t ev, hal_event_bits_t evs, uint8_t option, unsigned long timeout);

#define hal_ev_wait_all(ev, evs, timeout) hal_event_wait(ev, evs, HAL_EVENT_OPTION_CLEAR | HAL_EVENT_OPTION_AND, timeout)
#define hal_ev_wait_any(ev, evs, timeout) hal_event_wait(ev, evs, HAL_EVENT_OPTION_CLEAR | HAL_EVENT_OPTION_OR, timeout)
#define hal_ev_wait_all_no_clear(ev, evs, timeout) hal_event_wait(ev, evs, HAL_EVENT_OPTION_AND, timeout)
#define hal_ev_wait_any_no_clear(ev, evs, timeout) hal_event_wait(ev, evs, HAL_EVENT_OPTION_OR, timeout)

/*
 * set event bit
 * @ev: hal_event_t handler
 * @evs: event bit to set
 *
 * @return: return 0 if success
 */
int hal_event_set_bits(hal_event_t ev, hal_event_bits_t evs);

hal_event_bits_t hal_event_get(hal_event_t ev);

#ifdef __cplusplus
}
#endif
#endif

