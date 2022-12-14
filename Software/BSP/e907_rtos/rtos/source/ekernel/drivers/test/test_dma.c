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
#include <getopt.h>
#include <log.h>
#include <rtthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <hal_timer.h>
#include <hal_mem.h>
#include <sunxi_hal_dma.h>
#include <sunxi_drv_dma.h>


static int cmd_dma(int argc, const char **argv)
{
	int i;
	/* struct sunxi_dma_chan *chan = NULL; */
	hal_dma_chan_status_t ret;
	struct sunxi_dma_chan *chan = NULL;
	char *buf1 = NULL,*buf2 = NULL;
	struct dma_slave_config config = {0};
	uint32_t size = 0;

	printf("run in dma test\n");

	buf1 = hal_malloc(1024);
	buf2 = hal_malloc(1024);

	memset(buf1, 0, 1024);
	memset(buf2, 0, 1024);

	for (i = 0;i < 1023; i++)
		buf1[i] = 'a';
	buf1[1023] = '\n';
	//sunxi_dma_init();				//init dma,should init once

	//request dma chan
	/* chan = (struct sunxi_dma_chan *)sunxi_dma_chan_request(); */
	ret = hal_dma_chan_request(&chan);
	if (chan == HAL_DMA_CHAN_STATUS_BUSY)
	{
		printf("DMA busy\n");
		return -1;
	}
	config.direction = DMA_MEM_TO_MEM;
	/* config.dst_addr = (uint32_t)__va_to_pa(buf2); */
	/* config.src_addr = (uint32_t)__va_to_pa(buf1); */
	config.dst_addr_width = DMA_SLAVE_BUSWIDTH_8_BYTES;
	config.src_addr_width = DMA_SLAVE_BUSWIDTH_8_BYTES;
	config.dst_maxburst = DMA_SLAVE_BURST_16;
	config.src_maxburst = DMA_SLAVE_BURST_16;
	config.slave_id = sunxi_slave_id(DRQDST_SDRAM, DRQSRC_SDRAM);

	hal_dma_slave_config(chan, &config);

	hal_dma_prep_memcpy(chan, (uint32_t)buf2, (uint32_t)buf1,(uint32_t)1023);

	printf("dma test start\n");
	hal_dma_start(chan);

	//when to stop?
	while (hal_dma_tx_status(chan, &size) != 0) {
		msleep(1000);
		if ( ++i==3 )
		{
			printf("DMA timeout\n");
			break;
		}
	}

	printf("dma test stop\n");
	hal_dma_stop(chan);

	hal_dma_chan_free(chan);

	printf("send buf1:%s\n",buf1);
	printf("receive buf2:%s\n",buf2);

    return 0;
}
FINSH_FUNCTION_EXPORT_ALIAS(cmd_dma, __cmd_dma, rtthread dma test code);

