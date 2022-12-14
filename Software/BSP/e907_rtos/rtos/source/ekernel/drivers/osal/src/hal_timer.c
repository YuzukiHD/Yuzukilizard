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
#include <hal_timer.h>
#include <ktimer.h>
#include <string.h>

osal_timer_t osal_timer_create(const char *name,
                               timeout_func timeout,
                               void *parameter,
                               unsigned int time,
                               unsigned char flag)
{
    return rt_timer_create(name, timeout, parameter, time, flag);
}

hal_status_t osal_timer_delete(osal_timer_t timer)
{
    return rt_timer_delete(timer);
}

hal_status_t osal_timer_start(osal_timer_t timer)
{
    return rt_timer_start(timer);
}

hal_status_t osal_timer_stop(osal_timer_t timer)
{
    return rt_timer_stop(timer);
}

hal_status_t osal_timer_control(osal_timer_t timer, int cmd, void *arg)
{
    return rt_timer_control(timer, cmd, arg);
}

void sleep(int seconds);
int msleep(unsigned int msecs);
int usleep(unsigned int usecs);
void udelay(unsigned int us);

int hal_sleep(unsigned int secs)
{
    sleep(secs);
    return 0;
}

int hal_msleep(unsigned int msecs)
{
    return msleep(msecs);
}

int hal_usleep(unsigned int usecs)
{
    return usleep(usecs);
}

void hal_udelay(unsigned int us)
{
    udelay(us);
}

uint64_t hal_gettime_ns(void)
{
    struct timespec64 timeval;
    memset(&timeval, 0, sizeof(struct timespec64));
    do_gettimeofday(&timeval);
    return timeval.tv_sec * MSEC_PER_SEC * USEC_PER_MSEC * 1000LL + timeval.tv_nsec;
}

uint64_t hal_gettime_us(void)
{
    struct timespec64 timeval;
    memset(&timeval, 0, sizeof(struct timespec64));
    do_gettimeofday(&timeval);
    return timeval.tv_sec * MSEC_PER_SEC * USEC_PER_MSEC + timeval.tv_nsec;
}
