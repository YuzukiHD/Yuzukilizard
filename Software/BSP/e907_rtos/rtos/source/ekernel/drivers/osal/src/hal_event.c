#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <hal_sem.h>
#include <hal_time.h>
#include <hal_interrupt.h>
#include <hal_event.h>

int hal_event_init(hal_event_t ev)
{
    return rt_event_init(ev, "melis_event", RT_IPC_FLAG_FIFO);
}

int hal_event_datach(hal_event_t ev)
{
    return rt_event_detach(ev);
}

hal_event_t hal_event_create(void)
{
    return rt_event_create("melis_event", 0, RT_IPC_FLAG_FIFO);
}

hal_event_t hal_event_create_initvalue(int init_value)
{
    return rt_event_create("melis_event", init_value, RT_IPC_FLAG_FIFO);
}

int hal_event_delete(hal_event_t ev)
{
    if (!ev)
    {
        return -EINVAL;
    }

    return rt_event_delete(ev);
}

hal_event_bits_t hal_event_wait(hal_event_t ev, hal_event_bits_t evs, uint8_t option, unsigned long timeout)
{
    hal_event_bits_t bits;
    rt_event_recv(ev, evs, option, MS_TO_OSTICK(timeout), &bits);
    return bits;
}

int hal_event_set_bits(hal_event_t ev, hal_event_bits_t evs)
{
    if (!ev)
    {
        return -EINVAL;
    }
    return rt_event_send(ev, evs);
}

hal_event_bits_t hal_event_get(hal_event_t ev)
{
    hal_event_bits_t evs = 0;
    rt_event_control(ev, RT_IPC_CMD_GET_STATE, &evs);
    return evs;
}
