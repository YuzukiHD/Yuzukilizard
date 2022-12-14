/* cpufreq interlayer head file.
 *
 * Copyright (C) 2021 Allwinnertech.
 */

#ifndef __CPU_FREQ__
#define __CPU_FREQ__

#include <sunxi_hal_common.h>

/* struct platform_cpufreq_ops - platform cpufreq function
 * @set_freq:			set cpu freq
 * @get_freq:			get cpu current freq
 * @get_freq_table_size:	get the size of frequency table
 * @get_freq_table_freq:	get the freq in frequency table
 */
struct platform_cpufreq_ops {
	int	(*set_freq)(u32 clk_rate);
	int	(*get_freq)(void);
	int	(*get_freq_table_size)(void);
	int	(*get_freq_table_freq)(int index);
};

/* struct cpufreq_data - cpufreq instance
 * @ops:	platform cpufreq function
 */
struct cpufreq_data {
	struct platform_cpufreq_ops *ops;
};

int hal_cpufreq_data_register(struct cpufreq_data *data);
int hal_cpufreq_data_unregister(struct cpufreq_data *data);
int hal_cpufreq_set_freq(u32 clk_rate);
int hal_cpufreq_get_freq(void);
int hal_cpufreq_get_freq_table_size(void);
int hal_cpufreq_get_freq_table_freq(int index);

#endif	/* __CPU_FREQ__ */
