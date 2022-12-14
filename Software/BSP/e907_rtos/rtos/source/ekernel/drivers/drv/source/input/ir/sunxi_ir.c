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
#include <stdio.h>
#include <stdlib.h>
#include <sunxi_input.h>
#include <log.h>
#include <init.h>
#include "rc-core.h"
#include "rc-core-priv.h"
#include <sunxi_drv_ir.h>

#define IRADC_ERR(fmt, arg...) printf("%s()%d " fmt, __func__, __LINE__, ##arg)
#define IRADC_INIT(fmt, arg...) printf("%s()%d " fmt, __func__, __LINE__, ##arg)

#ifdef IRADC_DEBUG
#define IR_INFO(fmt, arg...) printf("%s()%d " fmt, __func__, __LINE__, ##arg)
#define IR_INFO_IRQ(fmt, arg...) __log("%s()%d " fmt, __func__, __LINE__, ##arg)
#else
#define IR_INFO(fmt, arg...)                                                  \
    do {                                                                   \
    } while (0);
#define IR_INFO_IRQ(fmt, arg...)                                                  \
    do {                                                                   \
    } while (0);
#endif

#define SUNXI_IR_DRIVER_NAME "sunxi-rc-recv"

#define RC5_UNIT        889000  /* ns */
#define NEC_UNIT        562500
#define NEC_BOOT_CODE       (16 * NEC_UNIT)
#define NEC         0x0
#define RC5         0x1
#define RC5ANDNEC       0x2
DEFINE_IR_RAW_EVENT(rawir);
static u32 is_receiving;
static u32 threshold_low = RC5_UNIT + RC5_UNIT / 2;
static u32 threshold_high = 2 * RC5_UNIT + RC5_UNIT / 2;
static bool pluse_pre;
static char protocol;
static bool boot_code;

struct sunxi_ir_data *ir_data;

static void sunxi_ir_recv(u32 reg_data, struct sunxi_ir_data *ir_data)
{
    bool pluse_now = 0;
    u32 ir_duration = 0;

    pluse_now = reg_data >> 7; /* get the polarity */
    ir_duration = reg_data & 0x7f; /* get duration, number of clocks */

    if (pluse_pre == pluse_now)
    {
        /* the signal sunperposition */
        rawir.duration += ir_duration;
        IR_INFO_IRQ("raw: polar=%d; dur=%d\n", pluse_now, ir_duration);
    }
    else
    {
        if (is_receiving)
        {
            rawir.duration *= IR_SIMPLE_UNIT;
            IR_INFO_IRQ("pusle :polar=%d, dur: %u ns\n",
                        rawir.pulse, rawir.duration);
            if (boot_code == 0)
            {
                boot_code = 1;
                if (eq_margin(rawir.duration, NEC_BOOT_CODE,
                              NEC_UNIT * 2))
                {
                    protocol = NEC;
                    ir_raw_event_store(ir_data->rcdev,
                                       &rawir);
                }
                else
                {
                    protocol = RC5;
                    ir_raw_event_store(ir_data->rcdev,
                                       &rawir);
                }
            }
            else
            {
                if (((rawir.duration > threshold_low) &&
                     (rawir.duration < threshold_high)) &&
                    (protocol == RC5))
                {
                    rawir.duration = rawir.duration / 2;
                    ir_raw_event_store(ir_data->rcdev,
                                       &rawir);
                    ir_raw_event_store(ir_data->rcdev,
                                       &rawir);
                }
                else
                    ir_raw_event_store(ir_data->rcdev,
                                       &rawir);
            }
            rawir.pulse = pluse_now;
            rawir.duration = ir_duration;
            IR_INFO_IRQ("raw: polar=%d; dur=%d\n",
                        pluse_now, ir_duration);
        }
        else
        {
            /* get the first pluse signal */
            rawir.pulse = pluse_now;
            rawir.duration = ir_duration;
            is_receiving = 1;
            IR_INFO_IRQ("get frist pulse,add head !!\n");
            IR_INFO_IRQ("raw: polar=%d; dur=%d\n", pluse_now,
                        ir_duration);
        }
        pluse_pre = pluse_now;
    }
}


static int ir_irq_callback(uint32_t data_type, uint32_t data)
{
    uint32_t reg_data = data;
    if (data_type == IR_RXINTS_RXDA)
    {
        sunxi_ir_recv(reg_data, ir_data);
    }
    else if (data_type == IR_RXINTS_RXPE)
    {
        if (rawir.duration)
        {
            rawir.duration *= IR_SIMPLE_UNIT;
            IR_INFO_IRQ("pusle :polar=%d, dur: %u ns\n",
                        rawir.pulse, rawir.duration);
            ir_raw_event_store(ir_data->rcdev, &rawir);
        }
        IR_INFO_IRQ("handle raw data.\n");
        /* handle ther decoder theread */
        ir_raw_event_handle(ir_data->rcdev);
        is_receiving = 0;
        boot_code = 0;
        pluse_pre = false;
    }
    else if (data_type == IR_RXINTS_RXOF)
    {
        IR_INFO_IRQ("ir_rx_irq_service: Rx FIFO Overflow!!\n");
        is_receiving = 0;
        boot_code = 0;
        pluse_pre = false;
    }

    return 0;
}

int sunxi_ir_init(void)
{
    struct rc_dev *sunxi_rcdev;
    int ret;

    ir_data = malloc(sizeof(struct sunxi_ir_data));
    if (!ir_data)
    {
        return  -1;
    }

    sunxi_rcdev = rc_allocate_device();
    if (!sunxi_rcdev)
    {
        return -1;
    }

    ir_data->ir_protocols = NEC;
    if (ir_data->ir_protocols == NEC)
    {
        sunxi_rcdev->allowed_protocols = (uint64_t)RC_BIT_NEC;
    }
    sunxi_rcdev->driver_type = RC_DRIVER_IR_RAW;
    sunxi_rcdev->enabled_protocols = sunxi_rcdev->allowed_protocols;
    sunxi_rcdev->driver_name =  SUNXI_IR_DRIVER_NAME;

    init_sunxi_ir_map();

    ret = rc_register_device(sunxi_rcdev);
    if (ret < 0)
    {
        printf("failed to register rc device\n");
        return -1;
    }
    ir_data->rcdev = sunxi_rcdev;

    ret = hal_ir_init();
    if (ret != IR_OK)
    {
        return -1;
    }
    hal_ir_register_callback(ir_irq_callback);
    return 0;
}

late_initcall(sunxi_ir_init);
