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
#include <hal_cpufreq.h>
#include <errno.h>

struct cpufreq_data *cpufreq;

int hal_cpufreq_data_register(struct cpufreq_data *data)
{
	int ret;

	if (!data || !data->ops ||
		!data->ops->set_freq || !data->ops->get_freq)
		return -EINVAL;

	cpufreq = data;

	return 0;
}

int hal_cpufreq_data_unregister(struct cpufreq_data *data)
{
	int ret;

	if (!cpufreq || (cpufreq != data))
		return -EINVAL;

	cpufreq = NULL;

	return 0;
}

int hal_cpufreq_set_freq(u32 clk_rate)
{
	if (!cpufreq)
		return -ENODEV;

	return cpufreq->ops->set_freq(clk_rate);
}

int hal_cpufreq_get_freq(void)
{
	if (!cpufreq)
		return -ENODEV;

	return cpufreq->ops->get_freq();
}

/*
 * cpufreq_get_freq_table_size - function to get frequency table size.
 *
 * Return: frequency table size if success, error code or 0 otherwise.
 */
int hal_cpufreq_get_freq_table_size(void)
{
	if (!cpufreq)
		return -ENODEV;

	if (cpufreq->ops->get_freq_table_size)
		return cpufreq->ops->get_freq_table_size();

	return 0;
}

/*
 * cpufreq_get_freq_table_freq - function to get frequency in frequency table.
 *
 * Return: frequency if success, error code or 0 otherwise.
 */
int hal_cpufreq_get_freq_table_freq(int index)
{
	if (!cpufreq)
		return -ENODEV;

	if (cpufreq->ops->get_freq_table_freq)
		return cpufreq->ops->get_freq_table_freq(index);

	return 0;
}
