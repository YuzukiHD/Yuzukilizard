#include <typedef.h>
#include <rtthread.h>
#include <rthw.h>
#include <arch.h>
#include <log.h>
#include <dfs_posix.h>
#include <finsh_api.h>
#include <finsh.h>
#include <debug.h>

#define THREAD_STACK_SIZE   1024
#define THREAD_PRIORITY     20
#define THREAD_TIMESLICE    10

static void hook_of_scheduler(struct rt_thread* from, struct rt_thread* to)
{
    rt_kprintf("from: %s -->  to: %s \n", from->name , to->name);
}

static void thread_entry(void* parameter)
{
    rt_uint32_t value;
    rt_uint32_t count = 0;

    value = (rt_uint32_t)parameter;
    while (1)
    {
	if(0 == (count % 5))
        {
            rt_kprintf("thread %d is running ,thread %d count = %d\n", value , value , count);
	    rt_thread_mdelay(100);
            if(count> 10000)
                return;
        }
         count++;
     }
}

int timeslice_sample(int argc, char **argv)
{
    rt_thread_t tid = RT_NULL;

    rt_scheduler_sethook(hook_of_scheduler);

    tid = rt_thread_create("thread1",
                            thread_entry, (void*)1,
                            THREAD_STACK_SIZE,
                            18, THREAD_TIMESLICE);
    if (tid != RT_NULL)
        rt_thread_startup(tid);


    tid = rt_thread_create("thread2",
                            thread_entry, (void*)2,
                            THREAD_STACK_SIZE,
                            19, THREAD_TIMESLICE-5);
    if (tid != RT_NULL)
        rt_thread_startup(tid);

    return 0;
}

FINSH_FUNCTION_EXPORT_ALIAS(timeslice_sample, __cmd_timeslice_sample, timeslice_sample test);

