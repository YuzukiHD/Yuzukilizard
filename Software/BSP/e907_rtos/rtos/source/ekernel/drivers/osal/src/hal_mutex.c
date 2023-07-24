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
#include <stdlib.h>
#include <hal_mutex.h>

#define PEND_TICK_MAX (0x7FFFFFFF - 1)
#define HAL_MUTEX_OK 0

int hal_mutex_init(hal_mutex *mutex)
{
    return rt_mutex_init(mutex, "hal_mutex", RT_IPC_FLAG_FIFO);
}

int hal_mutex_detach(hal_mutex *mutex)
{
    return rt_mutex_detach(mutex);
}

hal_mutex_t hal_mutex_create(void)
{
    return rt_mutex_create("hal_mutex", RT_IPC_FLAG_FIFO);
}

int hal_mutex_delete(hal_mutex_t mutex)
{
    if (!mutex)
    {
        return -1;
    }
    return rt_mutex_delete(mutex);
}

int hal_mutex_lock(hal_mutex_t mutex)
{
    if (!mutex)
    {
        return -1;
    }
    return rt_mutex_take(mutex, PEND_TICK_MAX);
}

int hal_mutex_unlock(hal_mutex_t mutex)
{
    if (!mutex)
    {
        return -1;
    }
    return rt_mutex_release(mutex);
}

int hal_mutex_trylock(hal_mutex_t mutex)
{
    if (!mutex)
    {
        return -1;
    }
    if (rt_mutex_take(mutex, 0) == 0)
    {
        return 0;
    }
    return -2;
}

int hal_mutex_timedwait(hal_mutex_t mutex, int ticks)
{
    if (!mutex)
    {
        return -1;
    }
    return rt_mutex_take(mutex, ticks);
}
