/*
* Copyright (c) 2019-2025 Allwinner Technology Co., Ltd. ALL rights reserved.
*
* Allwinner is a trademark of Allwinner Technology Co.,Ltd., registered in
* the the People's Republic of China and other countries.
* All Allwinner Technology Co.,Ltd. trademarks are used with permission.
*
* DISCLAIMER
* THIRD PARTY LICENCES MAY BE REQUIRED TO IMPLEMENT THE SOLUTION/PRODUCT.
* IF YOU NEED TO INTEGRATE THIRD PARTY’S TECHNOLOGY (SONY, DTS, DOLBY, AVS OR MPEGLA, ETC.)
* IN ALLWINNERS’SDK OR PRODUCTS, YOU SHALL BE SOLELY RESPONSIBLE TO OBTAIN
* ALL APPROPRIATELY REQUIRED THIRD PARTY LICENCES.
* ALLWINNER SHALL HAVE NO WARRANTY, INDEMNITY OR OTHER OBLIGATIONS WITH RESPECT TO MATTERS
* COVERED UNDER ANY REQUIRED THIRD PARTY LICENSE.
* YOU ARE SOLELY RESPONSIBLE FOR YOUR USAGE OF THIRD PARTY’S TECHNOLOGY.
*
*
* THIS SOFTWARE IS PROVIDED BY ALLWINNER"AS IS" AND TO THE MAXIMUM EXTENT
* PERMITTED BY LAW, ALLWINNER EXPRESSLY DISCLAIMS ALL WARRANTIES OF ANY KIND,
* WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING WITHOUT LIMITATION REGARDING
* THE TITLE, NON-INFRINGEMENT, ACCURACY, CONDITION, COMPLETENESS, PERFORMANCE
* OR MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
* IN NO EVENT SHALL ALLWINNER BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
* NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS, OR BUSINESS INTERRUPTION)
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
* STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
* OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include <stdint.h>
#include <hal_thread.h>
#include <kconfig.h>
#include <log.h>
#include <init.h>
#include <rtthread.h>

void *kthread_create(void (*threadfn)(void *data), void *data, const char *namefmt, int stacksize, int priority)
{
    rt_thread_t thr;

    thr = rt_thread_create(namefmt, threadfn, data, \
                           stacksize, \
                           priority, \
                           HAL_THREAD_TIMESLICE);

    RT_ASSERT(thr != RT_NULL);

    return (void *)thr;
}

int kthread_start(void *thread)
{
    rt_thread_t thr;
    rt_err_t ret;

    RT_ASSERT(thread != RT_NULL);

    thr = (rt_thread_t)thread;

    ret = rt_thread_startup(thr);

    return ret;
}

int kthread_mdelay(int ms)
{
	return rt_thread_mdelay(ms);
}

int kthread_stop(void *thread)
{
    rt_thread_t thr;

    RT_ASSERT(thread != RT_NULL);

    thr = (rt_thread_t)thread;

    rt_thread_delete(thr);

    return 0;
}

int kthread_wakeup(void *thread)
{
    rt_thread_t thr;
    rt_err_t err;

    RT_ASSERT(thread != RT_NULL);

    thr = (rt_thread_t)thread;

    err = rt_thread_resume(thr);
    if (err)
    {
        return -1;
    }

    return 0;
}

int kthread_suspend(void *thread)
{
    rt_thread_t thr;
    rt_err_t err;

    RT_ASSERT(thread != RT_NULL);

    thr = (rt_thread_t)thread;

    err = rt_thread_suspend(thr);
    if (err)
    {
        return -1;
    }

    return 0;
}
