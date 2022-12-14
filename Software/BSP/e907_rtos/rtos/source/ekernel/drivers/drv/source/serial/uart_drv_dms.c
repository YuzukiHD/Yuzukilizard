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
#include <hal_uart.h>
#include <rtthread.h>
#include <log.h>
#include <kapi.h>
#include <arch.h>
#include <mod_uart.h>

typedef struct __DEV_UART
{
    uint32_t        port;
    uint32_t        used;
    void            *hdle;
    void            *hReg;
    void            *sem;
    uint32_t        baudrate;
    __uart_para_t   para;
} __dev_uart_t;

__dev_uart_t dev_uart[UART_PORT_COUNT] = {0};


static const uint32_t g_uart_baudrate_map[] =
{
    300,
    600,
    1200,
    2400,
    4800,
    9600,
    19200,
    38400,
    57600,
    115200,
    230400,
    576000,
    921600,
    1000000,
    1500000,
    3000000,
    4000000,
};

static int32_t uart_convert_baudrate_code(uint32_t baudrate)
{
    int32_t num = 0, i = 0;

    num = sizeof(g_uart_baudrate_map)/sizeof(g_uart_baudrate_map[0]);

    for(i = 0; i < num; i ++)
    {
        if(baudrate == g_uart_baudrate_map[i])
        {
            break;
        }
    }

    if(i >= num)
    {
        return UART_BAUDRATE_MAX;
    }
    return i;
}

static void* DEV_UART_Open(void *open_arg, __u32 mode)
{
    __dev_uart_t    *pDev = (__dev_uart_t *)open_arg;
    rt_device_t     phdl = (rt_device_t)(pDev->hdle);
    rt_base_t       level;

    if((NULL == pDev) || (NULL == phdl))
    {
        __err("parameter error! pDev=%p phdle=%p", pDev, phdl);
        return NULL;
    }

    rt_device_open(phdl, RT_DEVICE_FLAG_RDWR | RT_DEVICE_FLAG_STREAM);


    if (1 == pDev->used)
    {
        __wrn("uart%d open failed, maybe used by someone else", pDev->port);
        return NULL;
    }

    level   = awos_arch_lock_irq();

    pDev->used = 1;

    awos_arch_unlock_irq(level);
    return (__hdle)pDev;
}

static int32_t DEV_UART_Close(__hdle hDev)
{
    __dev_uart_t    *pDev = (__dev_uart_t *)hDev;
    rt_device_t     phdl = (rt_device_t)(pDev->hdle);
    rt_base_t       level;

    if((NULL == pDev) || (NULL == phdl))
    {
        __err("parameter error! pDev=%p phdle=%p", pDev, phdl);
        return EPDK_FAIL;
    }

    rt_device_close(phdl);


    if (0 == pDev->used)
    {
        return EPDK_FAIL;
    }

    level   = awos_arch_lock_irq();

    pDev->used = 0;

    awos_arch_unlock_irq(level);
    return EPDK_OK;
}

static uint32_t DEV_UART_Read(void *pBuffer, uint32_t nSize, uint32_t nCount, __hdle hDev)
{
    __dev_uart_t    *pDev = (__dev_uart_t *)hDev;
    rt_device_t     phdl = (rt_device_t)(pDev->hdle);
    int32_t         ret = EPDK_OK;
    int8_t          err = 0;

    if((NULL == pDev) || (NULL == phdl) || (NULL == pDev->sem))
    {
        __err("parameter error! pDev=%p phdle=%p pDev->sem=%p", pDev, phdl, pDev->sem);
        return 0;
    }

    __inf("read from uart%d...", pDev->port);
    esKRNL_SemPend(pDev->sem, 0, &err);
    if(OS_NO_ERR != err)
    {
        __err("semphore pend is err(%d)", err);
        return 0;
    }

    ret = rt_device_read(phdl, 0, pBuffer, nSize * nCount);

    esKRNL_SemPost(pDev->sem);
    __inf("read from uart%d end", pDev->port);
    return ret;
}

static uint32_t DEV_UART_Write(const void *pBuffer, uint32_t nSize, uint32_t nCount, __hdle hDev)
{
    __dev_uart_t    *pDev = (__dev_uart_t *)hDev;
    rt_device_t     phdl = (rt_device_t)(pDev->hdle);
    int32_t         ret = EPDK_OK;
    int8_t          err = 0;

    if((NULL == pDev) || (NULL == phdl) || (NULL == pDev->sem))
    {
        __err("parameter error! pDev=%p phdle=%p pDev->sem=%p", pDev, phdl, pDev->sem);
        return 0;
    }

    __inf("write to uart%d: %s", pDev->port, (const char *)pBuffer);
    esKRNL_SemPend(pDev->sem, 0, &err);
    if(OS_NO_ERR != err)
    {
        __err("semphore pend is err(%d)", err);
        return 0;
    }

    ret = rt_device_write(phdl, 0, pBuffer, nSize * nCount);

    esKRNL_SemPost(pDev->sem);
    __inf("write to uart%d end", pDev->port);
    return ret;
}

static int32_t DEV_UART_Ioctl(__hdle hDev, uint32_t Cmd, long Aux, void *pBuffer)
{
    __dev_uart_t    *pDev = (__dev_uart_t *)hDev;
    rt_device_t     phdl = (rt_device_t)(pDev->hdle);
    int32_t         ret = EPDK_OK;
    int8_t          err = 0;

    if((NULL == pDev) || (NULL == phdl) || (NULL == pDev->sem))
    {
        __err("parameter error! pDev=%p phdle=%p pDev->sem=%p", pDev, phdl, pDev->sem);
        return EPDK_FAIL;
    }


    switch (Cmd)
    {
        case UART_CMD_SET_PARA:
        {
            __uart_para_t   *pUartPara = (__uart_para_t *)pBuffer;
            _uart_config_t  uart_config = {0};
            rt_base_t       level;

            /*1. check baudrate valid*/
            uart_config.baudrate        = uart_convert_baudrate_code(pDev->baudrate);

            if(uart_config.baudrate >= UART_BAUDRATE_MAX)
            {
                __err("baudrate(%d) invalid, convert err", pDev->baudrate);
                return EPDK_FAIL;
            }

            if(0 == pUartPara->nParityEnable)
            {
                uart_config.parity      = UART_PARITY_NONE;
            }
            else if(0 == pUartPara->nEvenParity)
            {
                uart_config.parity      = UART_PARITY_ODD;
            }
            else if(1 == pUartPara->nEvenParity)
            {
                uart_config.parity      = UART_PARITY_EVEN;
            }
            else
            {
                __err("Parity value invalid!");
                return EPDK_FAIL;
            }

            uart_config.stop_bit        = pUartPara->nStopBit;
            uart_config.word_length     = pUartPara->nDataLen;

            /*2. update old para value of dev_uart*/
            level   = awos_arch_lock_irq();
            pDev->para.nParityEnable    = pUartPara->nParityEnable;
            pDev->para.nEvenParity      = pUartPara->nEvenParity;
            pDev->para.nStopBit         = pUartPara->nStopBit;
            pDev->para.nDataLen         = pUartPara->nDataLen;
            awos_arch_unlock_irq(level);

            /*3. check mutex for multi-thread OS's option*/
            esKRNL_SemPend(pDev->sem, 0, &err);
            if(OS_NO_ERR != err)
            {
                __err("semphore pend is err(%d)", err);
                return EPDK_FAIL;
            }

            /*4. send the parameter converted to the hal by control api*/
            ret = rt_device_control(phdl, Cmd, &uart_config);

            esKRNL_SemPost(pDev->sem);
            return ret;
        }

        case UART_CMD_SET_BAUDRATE:
        {
            uint32_t        baudrate = *((uint32_t*)pBuffer);/*different old method mandatorily convert pointer to uint32_t type*/
            _uart_config_t  uart_config = {0};
            rt_base_t       level;

            /*1. check baudrate valid*/
            uart_config.baudrate        = uart_convert_baudrate_code(baudrate);

            if(uart_config.baudrate >= UART_BAUDRATE_MAX)
            {
                __err("baudrate(%d) invalid, convert err", pDev->baudrate);
                return EPDK_FAIL;
            }

            if(0 == pDev->para.nParityEnable)
            {
                uart_config.parity      = UART_PARITY_NONE;
            }
            else if(0 == pDev->para.nEvenParity)
            {
                uart_config.parity      = UART_PARITY_ODD;
            }
            else if(1 == pDev->para.nEvenParity)
            {
                uart_config.parity      = UART_PARITY_EVEN;
            }
            else
            {
                __err("Parity value invalid!");
                return EPDK_FAIL;
            }

            uart_config.stop_bit        = pDev->para.nStopBit;
            uart_config.word_length     = pDev->para.nDataLen;

            /*2. update only baudrate value of dev_uart*/
            level   = awos_arch_lock_irq();

            pDev->baudrate      = baudrate;

            awos_arch_unlock_irq(level);

            /*3. check mutex for multi-thread OS's option*/
            __msg("begin setting the baudrate of uart%d", pDev->port);
            esKRNL_SemPend(pDev->sem, 0, &err);
            if(OS_NO_ERR != err)
            {
                __err("semphore pend is err(%d)", err);
                return EPDK_FAIL;
            }

            /*4. send the parameter converted to the hal by control api*/
            ret = rt_device_control(phdl, Cmd, &uart_config);

            esKRNL_SemPost(pDev->sem);
            __msg("end setting the baudrate of uart%d", pDev->port);

            return ret;
        }

        case UART_CMD_FLUSH:
        {
            __msg("begin flush the recv buffer of uart%d", pDev->port);
            esKRNL_SemPend(pDev->sem, 0, &err);
            if(OS_NO_ERR != err)
            {
                __err("semphore pend is err(%d)", err);
                return EPDK_FAIL;
            }
            //ret = BSP_UART_FlushRecvBuffer(pDev->hdle); /*caution: buffer flush api for c800's old bsp*/
            esKRNL_SemPost(pDev->sem);
            __msg("end flush the recv buffer of uart%d", pDev->port);
            return ret;
        }

        default:
            break;
    }

    return EPDK_FAIL;
}


__dev_devop_t uart_devop =
{
    DEV_UART_Open,
    DEV_UART_Close,
    DEV_UART_Read,
    DEV_UART_Write,
    DEV_UART_Ioctl
};

void sunxi_driver_uart_dms_init(void)
{
    char            uart1[8] = "UART0";
    char            uart2[8] = "uart0";
    int32_t         i = 0;


    for(i = 0; i < UART_PORT_COUNT; i ++)
    {
        __msg("uart%d plugin...", i);
        dev_uart[i].port                = i;
        dev_uart[i].used                = 0;
        dev_uart[i].baudrate            = 115200;
        dev_uart[i].para.nDataLen       = 0x03;
        dev_uart[i].para.nParityEnable  = 0;
        dev_uart[i].para.nEvenParity    = 0;
        dev_uart[i].para.nStopBit       = 0;

        uart2[4]                        = i + '0';
        dev_uart[i].hdle                = rt_device_find(uart2);
        if(NULL == dev_uart[i].hdle)
        {
            __wrn("find uart%d rt-device failed!", i);
            continue;
        }

        dev_uart[i].sem                 = esKRNL_SemCreate(1);
        if(NULL == dev_uart[i].sem)
        {
            __wrn("create uart%d semphore failed!", i);
            continue;
        }

        uart1[4]                        = i + '0';
        dev_uart[i].hReg                = esDEV_DevReg("BUS", uart1, &uart_devop,(void * )&dev_uart[i]);
        if(NULL == dev_uart[i].hReg)
        {
            __wrn("esDEV_DevReg uart%d failed!", i);
            continue;
        }
    }

    return;
}

void sunxi_driver_uart_dms_uninit(void)
{
    //TODO:
    return;
}

int test_dms_uart(int argc, char** argv)
{
    char    *senddata = argv[1];
    void    *fp = NULL;

    if(NULL == senddata)
    {
        __err("send string is NULL");
        return EPDK_FAIL;
    }

    fp = esFSYS_fopen("b:\\BUS\\UART0", "rb+");
    if(fp)
    {
        esFSYS_fwrite(senddata, 1, rt_strlen(senddata), fp);
        esFSYS_fwrite("\r\n",   1, rt_strlen("\r\n"),   fp);

        esFSYS_fclose(fp);

        fp = NULL;
    }
    else
    {
        __wrn("b:\\BUS\\UART0 open failed!");
    }
    
    return 0;
}
FINSH_FUNCTION_EXPORT_ALIAS(test_dms_uart, __cmd_test_dms_uart, test dms uart);