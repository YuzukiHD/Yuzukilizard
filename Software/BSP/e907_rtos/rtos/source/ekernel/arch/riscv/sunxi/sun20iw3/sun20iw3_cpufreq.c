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
#include <sunxi_hal_common.h>
#include <hal_osal.h>
#include <hal_cpufreq.h>
#include <hal_clk.h>
#include <errno.h>
#include <hal_mem.h>
#include <stdlib.h>
#include <stdio.h>

#define E907_CLOCK_REGISTER	(0x02001d00)
#define OSC24M			(0)
#define PLL_PERI_600M		(3)
#define PLL_PERI_480M		(4)

struct cpufreq_data *data;

/* struct e907_freq - E907 freqiency point with clock source relationship
 * @rate:	frequency in Hz
 * @pclk:	parent clk
 * @factor_m:	E907_CLK = ClockSource / (m + 1)
 * @factor_n:	E907_AXI_CLK = E907_CLK / (n + 1)
 */
struct e907_freq {
	const u32 rate;
	const u32 pclk;
	const u32 factor_n;
	const u32 factor_m;
};

struct e907_freq e907_freq_table[] = {
	{ 24000000,	0,	1,	0},
	{ 40000000,	3,	1,	14},
	{ 60000000,	3,	1,	9},
	{ 80000000,	4,	1,	5},
	{100000000,	3,	1,	5},
	{120000000,	3,	1,	4},
	{150000000,	3,	1,	3},
	{160000000,	4,	1,	2},
	{200000000,	3,	1,	2},
	{240000000,	4,	1,	1},
	{300000000,	3,	1,	1},
	{480000000,	4,	1,	0},
	{600000000,	3,	1,	0},
};

int cpufreq_get_e907_freq_table_size(void)
{
	return (sizeof(e907_freq_table) / sizeof(e907_freq_table[0]));
}

int cpufreq_get_e907_freq_table_freq(int index)
{
	if (index < 0 || index > cpufreq_get_e907_freq_table_size())
		return -EINVAL;

	return (int)e907_freq_table[index].rate;
}

int cpufreq_set_e907_freq(u32 clk_rate)
{
	int i;
	int freq_table_size;
	u32 reg_val;
	u32 e907_clk_reg = (u32)E907_CLOCK_REGISTER;

	freq_table_size = cpufreq_get_e907_freq_table_size();
	for (i = 0; i < freq_table_size; i++) {
		if (e907_freq_table[i].rate == clk_rate)
			break;
	}

	/* rate isn't match*/
	if (i == freq_table_size)
		return -EINVAL;

	reg_val = e907_freq_table[i].factor_m +
			(e907_freq_table[i].factor_n << 8) +
			(e907_freq_table[i].pclk << 24);

	writel(reg_val, e907_clk_reg);

	return 0;
}

int cpufreq_get_e907_freq(void)
{
	int i;
	int rate = 0;
	u32 reg_val, pclk, factor_m;
	u32 e907_clk_reg = (u32)E907_CLOCK_REGISTER;

	reg_val = readl(e907_clk_reg);

	pclk = (reg_val >> 24);
	factor_m = reg_val & 0x0000001f;
	switch (pclk) {
	case OSC24M:
		rate =  24000000 / (factor_m + 1);
		break;
	case PLL_PERI_480M:
		rate = 480000000 / (factor_m + 1);
		break;
	case PLL_PERI_600M:
		rate = 600000000 / (factor_m + 1);
		break;
	dafult:
		rate = 0;
	}


	return rate;
}

struct platform_cpufreq_ops ops = {
	.set_freq = cpufreq_set_e907_freq,
	.get_freq = cpufreq_get_e907_freq,
	.get_freq_table_size = cpufreq_get_e907_freq_table_size,
	.get_freq_table_freq = cpufreq_get_e907_freq_table_freq,
};

int cpufreq_e907_init(void)
{
	if (data)
		return -EINVAL;

	data = hal_malloc(sizeof(struct cpufreq_data));
	if (!data)
		return -ENOMEM;

	data->ops = &ops;

	hal_cpufreq_data_register(data);

	return 0;
}

int cpufreq_e907_exit(void)
{
	if (!data || !data->ops)
		return -ENODEV;

	hal_cpufreq_data_unregister(data);

	data->ops = NULL;
	hal_free(data);

	return 0;
}
