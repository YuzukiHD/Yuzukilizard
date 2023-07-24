/*
* Copyright (c) 2019-2025 Allwinner Technology Co., Ltd. ALL rights reserved.
*
* Allwinner is a trademark of Allwinner Technology Co.,Ltd., registered in
* the the People's Republic of China and other countries.
* All Allwinner Technology Co.,Ltd. trademarks are used with permission.
*
* DISCLAIMER
* THIRD PARTY LICENCES MAY BE REQUIRED TO IMPLEMENT THE SOLUTION/PRODUCT.
* IF YOU NEED TO INTEGRATE THIRD PARTY��S TECHNOLOGY (SONY, DTS, DOLBY, AVS OR MPEGLA, ETC.)
* IN ALLWINNERS��SDK OR PRODUCTS, YOU SHALL BE SOLELY RESPONSIBLE TO OBTAIN
* ALL APPROPRIATELY REQUIRED THIRD PARTY LICENCES.
* ALLWINNER SHALL HAVE NO WARRANTY, INDEMNITY OR OTHER OBLIGATIONS WITH RESPECT TO MATTERS
* COVERED UNDER ANY REQUIRED THIRD PARTY LICENSE.
* YOU ARE SOLELY RESPONSIBLE FOR YOUR USAGE OF THIRD PARTY��S TECHNOLOGY.
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

#ifndef __SPI_SUN8IW21_H__
#define __SPI_SUN8IW21_H__

#define SUNXI_SPI0_PBASE         (0x04025000ul) /* 4K */
#define SUNXI_SPI1_PBASE         (0x04026000ul) /* 4K */
#define SUNXI_SPI2_PBASE         (0x04027000ul) /* 4K */
#define SUNXI_SPI3_PBASE         (0x04028000ul) /* 4K */
#ifdef CONFIG_SOC_SUN20IW3
#define SUNXI_IRQ_SPI0                 31
#define SUNXI_IRQ_SPI1                 32
#define SUNXI_IRQ_SPI2                 33
#define SUNXI_IRQ_SPI3                 37
#else
#define SUNXI_IRQ_SPI0                 47
#define SUNXI_IRQ_SPI1                 48
#define SUNXI_IRQ_SPI2                 49
#define SUNXI_IRQ_SPI3                 53
#endif

#define SPI_MAX_NUM 4
#define SPI0_PARAMS \
{   .port = 0, \
    .reg_base = SUNXI_SPI0_PBASE, .irq_num = SUNXI_IRQ_SPI0, \
	.pclk_pll_type = HAL_SUNXI_CCU, .pclk_pll_id = HAL_CLK_PLL_PERI0300M, \
	.pclk_hosc_type = HAL_SUNXI_CCU, .pclk_hosc_id = HAL_CLK_SRC_HOSC, \
	.bus_type = HAL_SUNXI_CCU, .bus_id = 0, \
	.mclk_type = HAL_SUNXI_CCU, .mclk_id = HAL_CLK_PERIPH_SPI0, \
	.reset_type = HAL_SUNXI_RESET, .reset_id = 0, \
}

#define SPI1_PARAMS \
{   .port = 1, \
    .reg_base = SUNXI_SPI1_PBASE, .irq_num = SUNXI_IRQ_SPI1, \
	.pclk_pll_type = HAL_SUNXI_CCU, .pclk_pll_id = HAL_CLK_PLL_PERI0300M, \
	.pclk_hosc_type = HAL_SUNXI_CCU, .pclk_hosc_id = HAL_CLK_SRC_HOSC, \
	.bus_type = HAL_SUNXI_CCU, .bus_id = 0, \
	.mclk_type = HAL_SUNXI_CCU, .mclk_id = HAL_CLK_PERIPH_SPI1, \
	.reset_type = HAL_SUNXI_RESET, .reset_id = 0, \
}

#define SPI2_PARAMS \
{   .port = 2, \
    .reg_base = SUNXI_SPI2_PBASE, .irq_num = SUNXI_IRQ_SPI2, \
	.pclk_pll_type = HAL_SUNXI_CCU, .pclk_pll_id = HAL_CLK_PLL_PERI0300M, \
	.pclk_hosc_type = HAL_SUNXI_CCU, .pclk_hosc_id = HAL_CLK_SRC_HOSC, \
	.bus_type = HAL_SUNXI_CCU, .bus_id = 0, \
	.mclk_type = HAL_SUNXI_CCU, .mclk_id = HAL_CLK_PERIPH_SPI2, \
	.reset_type = HAL_SUNXI_RESET, .reset_id = 0, \
}

#define SPI3_PARAMS \
{   .port = 3, \
    .reg_base = SUNXI_SPI3_PBASE, .irq_num = SUNXI_IRQ_SPI3, \
	.pclk_pll_type = HAL_SUNXI_CCU, .pclk_pll_id = HAL_CLK_PLL_PERI0300M, \
	.pclk_hosc_type = HAL_SUNXI_CCU, .pclk_hosc_id = HAL_CLK_SRC_HOSC, \
	.bus_type = HAL_SUNXI_CCU, .bus_id = 0, \
	.mclk_type = HAL_SUNXI_CCU, .mclk_id = HAL_CLK_PERIPH_SPI3, \
	.reset_type = HAL_SUNXI_RESET, .reset_id = 0, \
}

#endif /*__SPI_SUN8IW21_H__  */
