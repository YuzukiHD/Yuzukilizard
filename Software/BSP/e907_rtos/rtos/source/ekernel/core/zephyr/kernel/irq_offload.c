#include <kernel.h>
#include <irq_offload.h>

volatile irq_offload_routine_t offload_routine;
static void *offload_param;

/* Called by __svc */
void _irq_do_offload(void)
{
    offload_routine(offload_param);
}

void irq_offload(irq_offload_routine_t routine, void *parameter)
{
    k_sched_lock();
    offload_routine = routine;
    offload_param = parameter;

    //TODO: SVC To trigger the offload routine.
    offload_routine = NULL;
    k_sched_unlock();
}

