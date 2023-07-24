#include <rtthread.h>
#include <rtdef.h>
#include <rthw.h>
void caller_thumb(void)
{
    rt_uint32_t level;
    int main_fft(void);
    level = rt_hw_interrupt_disable();
    main_fft();
    rt_hw_interrupt_enable(level);
}
