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
#include <hal_sem.h>
#include <rtdef.h>
#include <log.h>

hal_sem_t hal_sem_create(unsigned int cnt)
{
     return (hal_sem_t) rt_sem_create("hal_layer", cnt, RT_IPC_FLAG_FIFO);
}

int hal_sem_delete(hal_sem_t sem)
{
    if (sem == NULL)
    {
        __err("fatal error, parameter is invalid.");
        return -1;
    }

    rt_sem_delete(sem);
    return 0;
}

int hal_sem_getvalue(hal_sem_t sem, int *val)
{
    if (sem == NULL || val == NULL)
    {
        __err("fatal error, parameter is invalid.");
        return -1;
    }

    rt_sem_control(sem, RT_IPC_CMD_GET_STATE, val);

    return 0;
}

int hal_sem_post(hal_sem_t sem)
{
    if (sem == NULL)
    {
        __err("fatal error, parameter is invalid.");
        return -1;
    }

    // must be success.
    rt_sem_release(sem);

    return 0;
}

int hal_sem_timedwait(hal_sem_t sem, unsigned long ticks)
{
    rt_err_t ret;

    if (sem == NULL)
    {
        __err("fatal error, parameter is invalid.");
        return -1;
    }

    ret = rt_sem_take(sem, ticks);
    if (ret != RT_EOK)
    {
        // timeout.
        return -2;
    }

    return 0;
}

int hal_sem_trywait(hal_sem_t sem)
{
    rt_err_t ret;

    if (sem == NULL)
    {
        __err("fatal error, parameter is invalid.");
        return -1;
    }

    ret = rt_sem_trytake(sem);
    if (ret != RT_EOK)
    {
        // timeout.
        return -2;
    }

    return 0;
}

int hal_sem_wait(hal_sem_t sem)
{
    rt_err_t ret;

    if (sem == NULL)
    {
        __err("fatal error, parameter is invalid.");
        return -1;
    }

    ret = rt_sem_take(sem, RT_WAITING_FOREVER);
    if (ret != RT_EOK)
    {
        // timeout.
        return -2;
    }

    return 0;
}

int hal_sem_clear(hal_sem_t sem)
{
    rt_err_t ret;

    if (sem == NULL)
    {
        __err("fatal error, parameter is invalid.");
        return -1;
    }

    ret = rt_sem_control(sem, RT_IPC_CMD_RESET, NULL);
    if (ret != RT_EOK) {
        __err("rt_sem_control fail\n");
        return -1;
    }

    return 0;
}