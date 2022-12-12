/*  cpufreq-test Tool
 *
 *  Copyright (C) 2014 sujiajia <sujiajia@allwinnertech.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 */

#include "cpu_lib.h"

static u32 monitor_magic_switch = 0;
static u32 monitor_start = 0;
static u32 monitor_cnt = 0;

static struct option monitor_long_options[] = {
	{"output",        1,    0,    'o'},
	{"attch",        1,    0,    'd'},
	{"complex",         1,    0,    'c'},
	{"tempreture",    1,    0,    't'},
	{"simple",      1,  0,  's'},
	{"file",        1,    0,    'f'},
	{"memory",      1,    0,    'm'},
	{"kswap",      1,    0,    'k'},
	{"help",        0,    0,    'h'},
	{"vresion",     0,  0,  'v'},
	{0,             0,     0,      0}
};

const char* const monitor_short_options = "h:o:d:c:t:f:v:s:k:";

/*******************************************************************
  monitor_help_usage_show
 *******************************************************************/

void monitor_help_usage_show()
{
	dprintf(" show how to use: cpu_monitor\n"
			" Options:\n"
			" 1.{ Show All Cpus with (Freq + Usage + Iowait) + (Temperature) } pre N millisecond \n"
			"     cpu_monitor -c [N]\t\t\t An example: cpu_monitor -c 500 \n\n"
			" 2.{ Show All Cpus with (Freq + Usage ) + (Temperature) } pre N millisecond \n"
			"     cpu_monitor -s [N]\t\t\t An example: cpu_monitor -s 500 \n\n"
			" 3.{ Show All Cpus with (Freq + Usage) + (Temperature) } pre N millisecond \n"
			"     Store log in dir/cpu-monitor_HOST_KERNEL-VERSION_TIMESTAMP.log \n"
			"     cpu_monitor -o [dir] -f [N]\t An example: cpu_monitor -o /data -f 500\n\n"
			" 4.{ Show meminfo per N millisecond ,unit default '0' as MB, set '1' as KB\n"
			"     cpu_monitor -u [unit] -m [N]\t An example: cpu_monitor -u 1 -m 500\n\n"
			" 5.{ Trace Kernel Memory vm_event } per N millisecond\n"
			"     cpu_monitor -k \t An example: cpu_monitor -k 500\n\n"
			" 6. -h, --help\t\t\t\t show this help screen\n\n"
			" 7. -v, --cpu monitor version\n\n"
			" 8. if you find any bug, pls email to author sujiajia@allwinnertech.com\n");
	exit(1);
}

FILE *monitor_prepare_output_file(const char *dirname)
{
	FILE *output = NULL;
	DIR *dir;
	s32 len;
	char *filename = NULL;
	char tmbuf[64];
	struct utsname sysdata;
	struct timeval tv;
	struct tm *nowtm;
	time_t nowtime;

	dir = opendir(dirname);
	if (dir == NULL) {
		if (mkdir(dirname, 0755)) {
			perror("mkdir");
			fprintf(stderr, "error: Cannot create dir %s\n",
					dirname);
			return NULL;
		}
	}

	len = strlen(dirname) + 125;
	filename = malloc(sizeof(char) * len);
	if (filename == NULL)
		return NULL;

	gettimeofday(&tv, NULL);
	nowtime = tv.tv_sec;
	nowtm = localtime(&nowtime);
	strftime(tmbuf, sizeof(tmbuf), "%F-%H-%M-%S", nowtm);

	if (uname(&sysdata) == 0) {
		len += strlen(sysdata.nodename) + strlen(sysdata.release);
		snprintf(filename, len - 1, "%s/cpu-monitor_%s_%s_%s.log",
				dirname, sysdata.nodename, sysdata.release, tmbuf);
	} else {
		snprintf(filename, len - 1, "%s/cpu-monitor_%s.log",
				dirname, tmbuf);
	}

	output = fopen(filename, "w+");
	if (output == NULL) {
		perror("fopen");
		fprintf(stderr, "error: unable to open logfile\n");
	}

	fprintf(stdout, "Logfile: %s\n", filename);

	free(filename);
	return output;
}

s32 monitor_system_platform(struct config *config)
{
	s32 ret;
	s32 i;
	char buf[32];
	char *plat_str[5] = { "Sun50iw1p1","Sun8iw1p1","Sun8iw7p1","Sun9iw1p1","sun8iw11p1",NULL };
	char *plat_info[5] = { "H64","H8","H3","A80","V40", NULL };
	char *plat_arch[3] = { "ARMv7", "AArch64", NULL};
	enum ARCH_PLAT {
		H64_ARCH_64 =0,
		H3_ARCH_32,
		H8_ARCH_32  = H3_ARCH_32,
		A80_ARCH_32 = H8_ARCH_32,
	};

	if (!config)
		return -1;
	ret = read_sys_info_stat(buf);
	if (ret < 0)
		return -1;

	for(i=0; plat_str[i]!=NULL; i++)
	{
		if (!strncmp(buf, plat_str[i], strlen(plat_str[i]))) {
			strncpy(config->platform_info, plat_info[i], strlen(plat_info[i]));
			/******
			H64 arch64  <=4GB  	 mem_type '0': mean platform just has ZONE DMA
			H3/H8 arch32 512MB   mem_type '1': mean platform just has ZONE NORMAL
			H3/H8 arch32 >=1GMB  mem_type '2': mean platform just has ZONE NORMAL and ZONE HIGH
			*******/
			if (i==H64_ARCH_64)
				config->mem_type = 0;
			else
				config->mem_type = 1;
			break;
		}
	}

	memset(buf, 0, sizeof(buf));
	ret = read_cpu_info_stat(buf);
	if (ret < 0)
		return -1;

	for(i=0; plat_arch[i]!=NULL; i++)
	{
		if (!strncmp(buf, plat_arch[i], strlen(plat_arch[i]))) {
			strncpy(config->platform_arch, plat_arch[i], strlen(plat_arch[i]));
			break;
		}
	}
	return 0;
}


s32 monitor_current_max_temperature(struct config *cfg)
{
	s32 themp_value = 0;
	if (!cfg)
		return -1;
	themp_value    = read_proc_thermal_stat();
	if (themp_value < 0)
		return -1;
	cfg->themp = themp_value;
	return 0;
}


s32 monitor_current_dram_dvfs(struct config *cfg)
{
	s32 dram_dvfs = 0;
	if (!cfg)
		return -1;
	dram_dvfs = read_proc_dram_dvfs();
	if (dram_dvfs < 0)
		dram_dvfs = 0;
	else
		cfg->dram_dvfs = dram_dvfs/1000; //Mhz
	return 0;
}


s32 monitor_current_gpu_dvfs(struct config *cfg)
{
	s32 gpu_dvfs = 0;
	if (!cfg)
		return -1;
	gpu_dvfs = read_proc_gpu_dvfs();
	if (gpu_dvfs < 0)
		gpu_dvfs = 0;
	else
		cfg->gpu_dvfs = gpu_dvfs/1000000;//Mhz
	return 0;
}


s32 monitor_percpu_freq_calculate(struct config *cfg)
{
	u32 i = 0;
	u32 cpu_max = CPU_NRS_MAX;
	s32 is_online;
	u32 pcpu_freq;

	if(!cfg)
		return -1;

	for (i=0; i<CPU_NRS_MAX; i++)
	{
		is_online = find_cpu_online(i);
		switch(is_online) {
			case CPU_IS_ONLINE:
				pcpu_freq = 0;
				pcpu_freq = get_cpufreq_by_cpunr(i);
				if (pcpu_freq != 0) {
					cfg->cpu_freq.cpu_online[i] = 1;
					cfg->cpu_freq.cpufreq_list[i] = pcpu_freq/1000;
				}
				break;
			case CPU_IS_OFFLINE:
				cfg->cpu_freq.cpu_online[i] = 0;
				cfg->cpu_freq.cpufreq_list[i] = 0;
				break;
			case CPU_IS_INVALID:
				cpu_max--;
			case CPU_IS_FAULT:
				cfg->cpu_freq.cpu_online[i] = 0;
				cfg->cpu_freq.cpufreq_list[i] = 0;
				break;
		}
	}
	cfg->max_cpu = cpu_max;
	return 0;
}

s32 monitor_percpu_usage_calculate(struct config *cfg)
{
	struct cpu_usage cur;
	s32 ret =0;
	s32 i = 0;
	float usage = 0;
	float total, busy, idle, iowait = 0;

	if (!cfg)
		return -1;
	memset(&cur, 0, sizeof(cur));
	ret = read_proc_precpu_stat(&cur);
	if (ret <0)
		return -1;

	if (cfg->pre_usages.active == 0) {
		memcpy(&cfg->pre_usages, &cur, sizeof(cur));
		memset(&cur, 0, sizeof(cur));
		usleep(300000);
		if (read_proc_precpu_stat(&cur)<0)
			return -1;
	}

	for(i=0; i<(CPU_NRS_MAX+1); i++)
	{
		if (cur.active&(0x01<<i)) {
			if (cfg->pre_usages.active&(0x01<<i)) {
				total = cur.stats[i].totalcpuTime-
					cfg->pre_usages.stats[i].totalcpuTime;
				busy = cur.stats[i].totalbusyTime -
					cfg->pre_usages.stats[i].totalbusyTime;
				idle = cur.stats[i].itime -
					cfg->pre_usages.stats[i].itime;
				iowait =  cur.stats[i].iowtime -
					cfg->pre_usages.stats[i].iowtime;

				if ((idle > total) || (total <= 0)){
					cfg->cpu_load[i].iowait = iowait;
					cfg->cpu_load[i].idle = idle;
					cfg->cpu_load[i].busy = busy;
				} else {
					usage = (float)busy/total*100;
					cfg->cpu_load[i].busy = (u32)usage;
					usage = (float)idle/total*100;
					cfg->cpu_load[i].idle = (u32)usage;
					usage = (float)iowait/total*100;
					cfg->cpu_load[i].iowait = (u32)usage;
				}
			}
		} else {
			cfg->cpu_load[i].iowait = 0;
			cfg->cpu_load[i].idle = 0;
			cfg->cpu_load[i].busy = 0;
		}
	}

	memcpy(&cfg->pre_usages, &cur, sizeof(cur));
	return 0;
}


s32 monitor_precpu_simple_show(struct config *config)
{
	char buf[512] = {0};
	u32 blen = 0;
	s32 ret = 0;

	if (!config)
		return -1;

	ret = monitor_percpu_usage_calculate(config);
	if (ret < 0)
		return -1;

	ret = monitor_current_max_temperature(config);
	if (ret < 0)
		return -1;

	ret = monitor_percpu_freq_calculate(config);
	if (ret < 0)
		return -1;

	ret = monitor_current_dram_dvfs(config);
	if (ret < 0)
		return -1;

	ret = monitor_current_gpu_dvfs(config);
	if (ret < 0)
		return -1;

	if ((monitor_magic_switch&0xf) == 0) {
		if (config->max_cpu == CPU_NR_C1) {
			blen += snprintf(buf, 512,
					"--------------------------------------------------------"
					"%s CPU[x]<Freq:MHZ>-<Usage:%%>"
					"-------------------------------------------------------\n"
					" %9s"
					" %12s"
					" %12s"
					" %12s"
					" %12s"
					" %12s"
					" %12s"
					" %12s"
					" %8s"
					" %5s"
					" %5s"
					" %6s"
					"\n",
					config->platform_info,
					"CPU0", "CPU1", "CPU2", "CPU3",
					"CPU4", "CPU5", "CPU6", "CPU7", "Temp", "GPU", "DDR" ,"Times");
		} else if (config->max_cpu ==CPU_NR_C0) {
			blen += snprintf(buf, 512,
					"-------------------------"
					"%s CPU[x]<Freq:MHZ>-<Usage:%%>"
					"-------------------------\n"
					" %9s"
					" %12s"
					" %12s"
					" %12s"
					" %8s"
					" %5s"
					" %5s"
					" %7s"
					"\n",
					config->platform_info,
					"CPU0", "CPU1", "CPU2", "CPU3",
					"Temp", "GPU", "DDR", "Times");
		}
		monitor_magic_switch = 0;
	}

	if (config->max_cpu == CPU_NR_C1) {
		blen += snprintf((buf + blen), 512,
				"  |%4d %3d%%|"
				"  |%4d %3d%%|"
				"  |%4d %3d%%|"
				"  |%4d %3d%%|"
				"  |%4d %3d%%|"
				"  |%4d %3d%%|"
				"  |%4d %3d%%|"
				"  |%4d %3d%%|"
				"  %4d "
				"  %3d "
				"  %3d "
				"  %3d \n",
				config->cpu_freq.cpufreq_list[0], config->cpu_load[0].busy,
				config->cpu_freq.cpufreq_list[1], config->cpu_load[1].busy,
				config->cpu_freq.cpufreq_list[2], config->cpu_load[2].busy,
				config->cpu_freq.cpufreq_list[3], config->cpu_load[3].busy,
				config->cpu_freq.cpufreq_list[4], config->cpu_load[4].busy,
				config->cpu_freq.cpufreq_list[5], config->cpu_load[5].busy,
				config->cpu_freq.cpufreq_list[6], config->cpu_load[6].busy,
				config->cpu_freq.cpufreq_list[7], config->cpu_load[7].busy,
				config->themp,config->gpu_dvfs, config->dram_dvfs,
				monitor_cnt++);
	} else if (config->max_cpu ==CPU_NR_C0) {
		blen += snprintf((buf + blen), 512,
				"  |%4d %3d%%|"
				"  |%4d %3d%%|"
				"  |%4d %3d%%|"
				"  |%4d %3d%%|"
				"  %4d "
				"  %3d "
				"  %3d "
				"  %3d \n",
				config->cpu_freq.cpufreq_list[0], config->cpu_load[0].busy,
				config->cpu_freq.cpufreq_list[1], config->cpu_load[1].busy,
				config->cpu_freq.cpufreq_list[2], config->cpu_load[2].busy,
				config->cpu_freq.cpufreq_list[3], config->cpu_load[3].busy,
				config->themp,config->gpu_dvfs, config->dram_dvfs,
				monitor_cnt++);
	}
	printf("%s",buf);
	if (monitor_cnt > 0xffffff00)
		monitor_cnt = 0;
	monitor_magic_switch ++;

	return 0;
}


s32 monitor_precpu_complex_show(struct config *config)
{
	char buf[1024] = {0};
	u32 blen = 0;
	s32 ret = 0;

	if (!config)
		return -1;
	ret = monitor_percpu_usage_calculate(config);
	if (ret < 0)
		return -1;

	ret = monitor_current_max_temperature(config);
	if (ret < 0)
		return -1;

	ret = monitor_percpu_freq_calculate(config);
	if (ret < 0)
		return -1;

	ret = monitor_current_dram_dvfs(config);
	if (ret < 0)
		return -1;

	ret = monitor_current_gpu_dvfs(config);
	if (ret < 0)
		return -1;

	if ((monitor_magic_switch&0x7) == 0) {
		if (config->max_cpu == CPU_NR_C1) {
			blen += snprintf(buf, 512,
					"-------------------------------------------------------------"
					"%s CPU[x]-------------------------------------------------------------------\n"
					"-------------------------------------------------"
					"<Freq:MHZ> <Usage:%%>-<Iowait:%%>------------------------------------------------------\n"
					" %9s"
					" %15s"
					" %16s"
					" %15s"
					" %15s"
					" %15s"
					" %15s"
					" %15s"
					" %8s"
					" %5s"
					" %5s"
					"\n",
					config->platform_info,
					"CPU0", "CPU1", "CPU2", "CPU3",
					"CPU4", "CPU5", "CPU6", "CPU7", "Temp", "GPU", "DDR");
		}  else if (config->max_cpu ==CPU_NR_C0) {
			blen += snprintf(buf, 512,
					"-------------------------------"
					"%s CPU[x]----------------------------------\n"
					"------------------"
					"<Freq:MHZ> <Usage:%%>-<Iowait:%%>----------------------\n"
					" %9s"
					" %15s"
					" %16s"
					" %15s"
					" %8s"
					" %5s"
					" %5s"
					"\n",
					config->platform_info,
					"CPU0", "CPU1", "CPU2", "CPU3",
					"Temp", "GPU", "DDR");
		}
		monitor_magic_switch = 0;
	}
	if (config->max_cpu == CPU_NR_C1) {
		blen += snprintf((buf + blen), 512,
				"%5d %3d%%%3d%%\t"
				"%5d %3d%%%3d%%\t"
				"%5d %3d%%%3d%%\t"
				"%5d %3d%%%3d%%\t"
				"%5d %3d%%%3d%%\t"
				"%5d %3d%%%3d%%\t"
				"%5d %3d%%%3d%%\t"
				"%5d %3d%%%3d%%\t"
				"%3d %6d %5d\n",
				config->cpu_freq.cpufreq_list[0], config->cpu_load[0].busy, config->cpu_load[0].iowait,
				config->cpu_freq.cpufreq_list[1], config->cpu_load[1].busy, config->cpu_load[1].iowait,
				config->cpu_freq.cpufreq_list[2], config->cpu_load[2].busy, config->cpu_load[2].iowait,
				config->cpu_freq.cpufreq_list[3], config->cpu_load[3].busy, config->cpu_load[3].iowait,
				config->cpu_freq.cpufreq_list[4], config->cpu_load[4].busy, config->cpu_load[4].iowait,
				config->cpu_freq.cpufreq_list[5], config->cpu_load[5].busy, config->cpu_load[5].iowait,
				config->cpu_freq.cpufreq_list[6], config->cpu_load[6].busy, config->cpu_load[6].iowait,
				config->cpu_freq.cpufreq_list[7], config->cpu_load[7].busy, config->cpu_load[7].iowait,
				config->themp, config->gpu_dvfs, config->dram_dvfs);
	}else if (config->max_cpu ==CPU_NR_C0) {
		blen += snprintf((buf + blen), 512,
				"%5d %3d%%%3d%%\t"
				"%5d %3d%%%3d%%\t"
				"%5d %3d%%%3d%%\t"
				"%5d %3d%%%3d%%\t"
				"%3d %6d %5d \n",
				config->cpu_freq.cpufreq_list[0], config->cpu_load[0].busy, config->cpu_load[0].iowait,
				config->cpu_freq.cpufreq_list[1], config->cpu_load[1].busy, config->cpu_load[1].iowait,
				config->cpu_freq.cpufreq_list[2], config->cpu_load[2].busy, config->cpu_load[2].iowait,
				config->cpu_freq.cpufreq_list[3], config->cpu_load[3].busy, config->cpu_load[3].iowait,
				config->themp, config->gpu_dvfs, config->dram_dvfs);
	}
	printf("%s",buf);
	monitor_magic_switch ++;

	return 0;
}


s32 monitor_precpu_complex_store(struct config *config)
{
	char buf[1024] = {0};
	u32 blen = 0;
	s32 ret = 0;

	if (!config)
		return -1;

	ret = monitor_percpu_usage_calculate(config);
	if (ret < 0)
		return -1;

	ret = monitor_current_max_temperature(config);
	if (ret < 0)
		return -1;

	ret = monitor_percpu_freq_calculate(config);
	if (ret < 0)
		return -1;

	ret = monitor_current_dram_dvfs(config);
	if (ret < 0)
		return -1;

	ret = monitor_current_gpu_dvfs(config);
	if (ret < 0)
		return -1;

	if (monitor_magic_switch == 0) {
		if (config->max_cpu == CPU_NR_C1) {
			blen += snprintf(buf, 512,
					"----------------------------------------------"
					"%s CPU[x]-------------------------------------------------------------------\n"
					"-----------------------------------"
					"<Freq:MHZ> <Usage:%%>-<Iowait:%%>------------------------------------------------------\n"
					" %9s"
					" %12s"
					" %13s"
					" %13s"
					" %13s"
					" %13s"
					" %13s"
					" %13s"
					" %8s"
					" %5s"
					" %5s"
					"\n",
					config->platform_info,
					"CPU0", "CPU1", "CPU2", "CPU3",
					"CPU4", "CPU5", "CPU6", "CPU7", "Temp", "GPU", "DDR");
		} else if (config->max_cpu ==CPU_NR_C0) {
			blen += snprintf(buf, 512,
					"--------------------------------"
					"%s CPU[x]-------------------------------------------------\n"
					"---------------------------"
					"<Freq:MHZ> <Usage:%%>-<Iowait:%%>-----------------------\n"
					" %9s"
					" %12s"
					" %13s"
					" %13s"
					" %8s"
					" %5s"
					" %5s"
					"\n",
					config->platform_info,
					"CPU0", "CPU1", "CPU2", "CPU3",
					"Temp", "GPU", "DDR");
		}
		monitor_magic_switch++;
	}
	if (config->max_cpu == CPU_NR_C1) {
		blen += snprintf((buf + blen), 512,
				"%4d %3d %3d  "
				"%4d %3d %3d  "
				"%4d %3d %3d  "
				"%4d %3d %3d  "
				"%4d %3d %3d  "
				"%4d %3d %3d  "
				"%4d %3d %3d  "
				"%4d %3d %3d  "
				"%4d %5d %5d\n",
				config->cpu_freq.cpufreq_list[0],config->cpu_load[0].busy, config->cpu_load[0].iowait,
				config->cpu_freq.cpufreq_list[1],config->cpu_load[1].busy, config->cpu_load[1].iowait,
				config->cpu_freq.cpufreq_list[2],config->cpu_load[2].busy, config->cpu_load[2].iowait,
				config->cpu_freq.cpufreq_list[3],config->cpu_load[3].busy, config->cpu_load[3].iowait,
				config->cpu_freq.cpufreq_list[4],config->cpu_load[4].busy, config->cpu_load[4].iowait,
				config->cpu_freq.cpufreq_list[5],config->cpu_load[5].busy, config->cpu_load[5].iowait,
				config->cpu_freq.cpufreq_list[6],config->cpu_load[6].busy, config->cpu_load[6].iowait,
				config->cpu_freq.cpufreq_list[7],config->cpu_load[7].busy, config->cpu_load[7].iowait,
				config->themp, config->gpu_dvfs, config->dram_dvfs);
	} else if (config->max_cpu ==CPU_NR_C0) {
		blen += snprintf((buf + blen), 512,
				"%4d %3d %3d  "
				"%4d %3d %3d  "
				"%4d %3d %3d  "
				"%4d %3d %3d  "
				"%4d %5d %5d\n",
				config->cpu_freq.cpufreq_list[0],config->cpu_load[0].busy, config->cpu_load[0].iowait,
				config->cpu_freq.cpufreq_list[1],config->cpu_load[1].busy, config->cpu_load[1].iowait,
				config->cpu_freq.cpufreq_list[2],config->cpu_load[2].busy, config->cpu_load[2].iowait,
				config->cpu_freq.cpufreq_list[3],config->cpu_load[3].busy, config->cpu_load[3].iowait,
				config->themp, config->gpu_dvfs, config->dram_dvfs);
	}
	printf("%s", buf);
	if (config->output != NULL) {
		fseek(config->output, 0L, SEEK_END);
		fwrite(buf, 1, blen, config->output);
	}

	return 0;
}


s32 monitor_percpu_freq_show(struct cpufreq_info *cfs)
{
	struct timeval now;
	gettimeofday(&now,NULL);
	if (monitor_magic_switch == 0) {
		cfs->blen += snprintf(cfs->buf, 256,
				"%11s %7s %8s %14s %16s %20s\n",
				"Time","CPU","Cur-F","<Min-Max-F>",
				"Scaling-Cur-F","<Scaling-Min-Max-F>");
		monitor_magic_switch = 1;
	}
	cfs->blen += snprintf((cfs->buf + cfs->blen), 256,
			"%7.3f %3d  %8d %7d-%4d %12d %15d-%4d \n",
			now.tv_sec+now.tv_usec*1e-6,
			cfs->online_cpu,
			cfs->cur_freq/1000,
			cfs->min_freq/1000,
			cfs->max_freq/1000,
			cfs->scaling_cur_freq/1000,
			cfs->scaling_min_freq/1000,
			cfs->scaling_max_freq/1000);
	printf("%s",cfs->buf);

	return 0;
}


s32 monitor_presensor_tempe(struct config *config)
{
	s32 ret = 0;
	char strout[512] = {0};
	u32 strlen = 0;

	if (!config)
		return -1;

	ret = read_sysfs_tempe_state(config->thempre);
	if (ret <0)
		return -1;
	if ((monitor_magic_switch&0x1f) == 0) {
		strlen += snprintf(strout, 512,
				//" %13s"
				" %12s"
				" %12s"
				" %12s"
				" %12s"
				" %12s"
				" %12s"
				"\n",
				//"Time",
				"A15 Temp", "A7 Temp",
				"Dram Temp", "GPU Temp",
				"avg Temp", "max Temp");
		monitor_magic_switch = 0;
	}

	//gettimeofday(&now,NULL);
	strlen += snprintf((strout + strlen), 512,
			//" %15.2f"
			" %7d"
			" %12d"
			" %9d"
			" %13d"
			" %13d"
			" %12d\n",
			//now.tv_sec+now.tv_usec*1e-6,
			config->thempre[0],
			config->thempre[3],
			config->thempre[1],
			config->thempre[2],
			config->thempre[5],
			config->thempre[4]);

	printf("%s",strout);
	monitor_magic_switch ++;

	return 0;
}

s32 monitor_system_meminfo(struct config *config)
{
	static s32 pre_vm[VMSTATE_MAX];
	struct vmstat_value vmstat;
	char strout[512] = {0};
	u32 strlen = 0;
	s32 ret = 0;

	if (!config)
		return -1;

	monitor_system_platform(config);
	memset(&vmstat, 0 , sizeof(vmstat));
	ret = read_proc_vmstat(&vmstat);
	if (ret < 0) {
		ERROR("aceess vmstat fail!\n");
	}

	ret = read_proc_meminfo(&vmstat);
	if (ret < 0) {
		ERROR("aceess meminfo fail!\n");
	}
	if (monitor_start == 0) {
		memcpy((void *)pre_vm, (void *)vmstat.vm_value, sizeof(pre_vm));
		monitor_start = 1;
		return 0;
	}

	if ((monitor_magic_switch&0xf) == 0) {
		strlen += snprintf(strout, 512,
				" --------------------------------------%s %s--Mem State {unit:%s}------------------------------------------\n"
				" %7s %6d  "
				" %7s %6d  "
				" %7s %6d  "
				" %7s %6d  "
				" %7s %6d  "
				" \n"
				" %7s %6d  "
				" %7s %6d  "
				" \n",
				config->platform_info,
				config->platform_arch,
				(config->unit==0)?"MB":"KB",
				"Memory-Total:",
				(config->unit==0)?vmstat.mm_value[Total]:vmstat.mm_value[Total]/1024,
				"Swap-Total:",
				(config->unit==1)?vmstat.mm_value[SWAPTOTAL]:vmstat.mm_value[SWAPTOTAL]/1024,
				"Swap-Free:",
				(config->unit==1)?vmstat.mm_value[SWAPFREE]:(vmstat.mm_value[SWAPFREE])/1024,
				"Vma-Total:",
				(config->unit==1)?vmstat.mm_value[VMTOTAL]:vmstat.mm_value[VMTOTAL]/1024,
				"Vma-Free:",
				(config->unit==1)?(vmstat.mm_value[VMTOTAL] - vmstat.mm_value[VMUESD]):
					((vmstat.mm_value[VMTOTAL] - vmstat.mm_value[VMUESD])/1024),
				"Buffer:",
				(config->unit==1)?vmstat.mm_value[BUFFER]:vmstat.mm_value[BUFFER]/1024,
				"KernStack:",
				(config->unit==1)?vmstat.vm_value[NR_KERNEL_STACK]*4:(vmstat.vm_value[NR_KERNEL_STACK])*4/1024);

		strlen += snprintf((strout + strlen), 512,
				"%7s"
				"%7s"
				"%7s"
				"%7s"
				"%7s| "
				"%10s"
				"%7s"
				"%7s"
				"%7s"
				"%7s"
				"%7s"
				"%7s"
				"%7s"
				"%7s"
				"\n",
				"Anon",
				"Slab",
				"Cache",
				"Sysfre",
				"Cmafre",
				(config->mem_type==0)?"pgal{dm}":"pgal{nor}",
				"pgfree",
				"pgfalt",
				"fimap",
				"kswap",
				"dirty",
				"wback",
				"rbio",
				"wbio");
		monitor_magic_switch = 0;
	}
	if (config->unit == 1) {
		strlen += snprintf((strout + strlen), 512,
				"%7d"
				"%7d"
				"%7d"
				"%7d"
				"%7d| "
				"%10d"
				"%7d"
				"%7d"
				"%7d"
				"%7d"
				"%7d"
				"%7d"
				"%7d"
				"%7d"
				"\n",
				vmstat.vm_value[NR_ANON_PAGES]*4,
				(vmstat.vm_value[NR_SLAB_RECLAIMABLE] + vmstat.vm_value[NR_SLAB_UNRECLAIMABLE])*4,
				vmstat.vm_value[NR_FILE_PAGES]*4,
				(vmstat.vm_value[NR_FREE_PAGES] - vmstat.vm_value[NR_FREE_CMA])*4,
				vmstat.vm_value[NR_FREE_CMA]*4,
				((config->mem_type==0)?(vmstat.vm_value[PGALLOC_DMA]-pre_vm[PGALLOC_DMA]):
				(vmstat.vm_value[PGALLOC_NORMAL]-pre_vm[PGALLOC_NORMAL])),
				vmstat.vm_value[PGFREE]-pre_vm[PGFREE],
				vmstat.vm_value[PGFAULT]-pre_vm[PGFAULT],
				vmstat.vm_value[PGMAJFAULT]-pre_vm[PGMAJFAULT],
				vmstat.vm_value[PAGEOUTRUN]-pre_vm[PAGEOUTRUN],
				vmstat.vm_value[NR_DIRTIED] -pre_vm[NR_DIRTIED],
				vmstat.vm_value[NR_WRITTEN] -pre_vm[NR_WRITTEN],
				vmstat.vm_value[PGPGIN]-pre_vm[PGPGIN],
				vmstat.vm_value[PGPGOUT]-pre_vm[PGPGOUT]);
	} else {
		strlen += snprintf((strout + strlen), 512,
				"%7d"
				"%7d"
				"%7d"
				"%7d"
				"%7d| "
				"%10d"
				"%7d"
				"%7d"
				"%7d"
				"%7d"
				"%7d"
				"%7d"
				"%7d"
				"%7d"
				"\n",
				(vmstat.vm_value[NR_ANON_PAGES])*4/1024,
				((vmstat.vm_value[NR_SLAB_RECLAIMABLE] + vmstat.vm_value[NR_SLAB_UNRECLAIMABLE]))*4/1024,
				(vmstat.vm_value[NR_FILE_PAGES])*4/1024,
				((vmstat.vm_value[NR_FREE_PAGES] - vmstat.vm_value[NR_FREE_CMA]))*4/1024,
				(vmstat.vm_value[NR_FREE_CMA])*4/1024,
				((config->mem_type==0)?(vmstat.vm_value[PGALLOC_DMA]-pre_vm[PGALLOC_DMA]):
				(vmstat.vm_value[PGALLOC_NORMAL]-pre_vm[PGALLOC_NORMAL])),
				vmstat.vm_value[PGFREE]-pre_vm[PGFREE],
				vmstat.vm_value[PGFAULT]-pre_vm[PGFAULT],
				vmstat.vm_value[PGMAJFAULT]-pre_vm[PGMAJFAULT],
				vmstat.vm_value[PAGEOUTRUN]-pre_vm[PAGEOUTRUN],
				vmstat.vm_value[NR_DIRTIED] -pre_vm[NR_DIRTIED],
				vmstat.vm_value[NR_WRITTEN] -pre_vm[NR_WRITTEN],
				vmstat.vm_value[PGPGIN]-pre_vm[PGPGIN],
				vmstat.vm_value[PGPGOUT]-pre_vm[PGPGOUT]);
	}
	monitor_magic_switch++;

	printf("%s",strout);
	memcpy((void *)pre_vm, (void *)vmstat.vm_value, sizeof(pre_vm));
	return 0;
fail:
	return -1;
}

s32 monitor_system_kswap(struct config *config)
{
	static s32 pre_vm_value[VMSTATE_MAX];
	struct vmstat_value vmstat;
	char strout[1024] = {0};
	u32 strlen = 0;
	s32 ret = 0;

	if (!config)
		return -1;

	monitor_system_platform(config);
	memset(&vmstat, 0 , sizeof(vmstat));
	ret = read_proc_vmstat(&vmstat);
	if (ret < 0) {
		ERROR("aceess vmstat fail!\n");
	}

	ret = read_proc_meminfo(&vmstat);
	if (ret < 0) {
		ERROR("aceess meminfo fail!\n");
	}
	if (monitor_start == 0) {
		memcpy((void *)pre_vm_value, (void *)vmstat.vm_value, sizeof(pre_vm_value));
		monitor_start = 1;
		return 0;
	}

	if ((monitor_magic_switch&0xf) == 0) {
		strlen += snprintf(strout, 512,
				" -----------------------------------------------------%s %s Zone %s memory event-------------------------------------------------------\n"
				" ----Global-Pg----|"
				" ----------Global-io----------|"
				" ------Kswap------|"
				" --Direct Reclaim--|"
				" -----Other scan------|"
				" ----------Cma----------|-----\n",
				config->platform_info,
				config->platform_arch,
				(config->mem_type==0)?"Dma":"Normal");

		strlen += snprintf((strout + strlen), 512,
				"%6s"
				"%6s"
				"%6s"//
				"%7s"
				"%6s"
				"%6s"
				"%6s"
				"%6s"//		
				"%6s"
				"%6s"
				"%7s"//	
				"%6s"
				"%6s"
				"%7s"//
				"%10s"
				"%7s"
				"%7s"//
				"%7s"
				"%6s"
				"%6s"
				"%6s"
				"%6s\n",
				"alloc",
				"free",
				"fault",//
				"fimap",
				"rbio",
				"wbio",
				"dirty",
				"wback",//
				"run",
				"scan",
				"reclai",//
				"run",
				"scan",
				"reclai",//	
				"pgrefill",
				"slabs",
				"wback",//
				"alloc",
				"free",
				"migs",
				"migf",
				"time");
		monitor_magic_switch = 0;
	}

	strlen += snprintf((strout + strlen), 512,
				"%6d"
				"%6d"
				"%6d"//
				"%7d"
				"%6d"
				"%6d"
				"%6d"
				"%6d"//
				"%6d"
				"%6d"
				"%7d"//
				"%6d"
				"%6d"
				"%7d"//
				"%10d"
				"%7d"
				"%7d"//
				"%7d"
				"%6d"
				"%6d"
				"%6d"
				"%6d\n",
				((config->mem_type==0)?(vmstat.vm_value[PGALLOC_DMA]-pre_vm_value[PGALLOC_DMA]):
				(vmstat.vm_value[PGALLOC_NORMAL]-pre_vm_value[PGALLOC_NORMAL])),
				vmstat.vm_value[PGFREE]-pre_vm_value[PGFREE],
				vmstat.vm_value[PGFAULT]-pre_vm_value[PGFAULT],//
				vmstat.vm_value[PGMAJFAULT]-pre_vm_value[PGMAJFAULT],
				vmstat.vm_value[PGPGIN]-pre_vm_value[PGPGIN],
				vmstat.vm_value[PGPGOUT]-pre_vm_value[PGPGOUT],
				vmstat.vm_value[NR_DIRTIED]-pre_vm_value[NR_DIRTIED],
				vmstat.vm_value[NR_WRITTEN]-pre_vm_value[NR_WRITTEN],//
				vmstat.vm_value[PAGEOUTRUN]-pre_vm_value[PAGEOUTRUN],
				((config->mem_type==0)?(vmstat.vm_value[PGSCAN_KSWAPD_DMA]-pre_vm_value[PGSCAN_KSWAPD_DMA]):
				(vmstat.vm_value[PGSCAN_KSWAPD_NORMAL]-pre_vm_value[PGSCAN_KSWAPD_NORMAL])),
				//vmstat.vm_value[PGSCAN_KSWAPD_NORMAL]-pre_vm_value[PGSCAN_KSWAPD_NORMAL],
				((config->mem_type==0)?(vmstat.vm_value[PGSTEAL_KSWAPD_DMA]-pre_vm_value[PGSTEAL_KSWAPD_DMA]):
				(vmstat.vm_value[PGSTEAL_KSWAPD_NORMAL]-pre_vm_value[PGSTEAL_KSWAPD_NORMAL])),//
				//vmstat.vm_value[PGSTEAL_KSWAPD_NORMAL]-pre_vm_value[PGSTEAL_KSWAPD_NORMAL],
				vmstat.vm_value[ALLOCSTALL]-pre_vm_value[ALLOCSTALL],
				((config->mem_type==0)?(vmstat.vm_value[PGSCAN_DIRECT_DMA]-pre_vm_value[PGSCAN_DIRECT_DMA]):
				(vmstat.vm_value[PGSCAN_DIRECT_NORMAL]-pre_vm_value[PGSCAN_DIRECT_NORMAL])),//
				//vmstat.vm_value[PGSCAN_DIRECT_NORMAL]-pre_vm_value[PGSCAN_DIRECT_NORMAL],
				((config->mem_type==0)?(vmstat.vm_value[PGSTEAL_DIRECT_DMA]-pre_vm_value[PGSTEAL_DIRECT_DMA]):
				(vmstat.vm_value[PGSTEAL_DIRECT_NORMAL]-pre_vm_value[PGSTEAL_DIRECT_NORMAL])),
				//vmstat.vm_value[PGSTEAL_DIRECT_NORMAL]-pre_vm_value[PGSTEAL_DIRECT_NORMAL],
				((config->mem_type==0)?(vmstat.vm_value[PGREFILL_DMA]-pre_vm_value[PGREFILL_DMA]):
				(vmstat.vm_value[PGREFILL_NORMAL]-pre_vm_value[PGREFILL_NORMAL])),				
				//vmstat.vm_value[PGREFILL_NORMAL]-pre_vm_value[PGREFILL_NORMAL],
				vmstat.vm_value[SLABS_SCANNED]-pre_vm_value[SLABS_SCANNED],
				vmstat.vm_value[NR_VMSCAN_WRITE]-pre_vm_value[NR_VMSCAN_WRITE],//	
				vmstat.vm_value[PGCMAIN]-pre_vm_value[PGCMAIN],
				vmstat.vm_value[PGCMAOUT]-pre_vm_value[PGCMAOUT],
				vmstat.vm_value[PGMIGRATE_CMA_SUCCESS]-pre_vm_value[PGMIGRATE_CMA_SUCCESS],
				vmstat.vm_value[PGMIGRATE_CMA_FAIL]-pre_vm_value[PGMIGRATE_CMA_FAIL],
				monitor_cnt++);		
	monitor_magic_switch++;
	printf("%s",strout);
	memcpy((void *)pre_vm_value, (void *)vmstat.vm_value, sizeof(pre_vm_value));
	if (monitor_cnt > 0xffffff00)
		monitor_cnt = 0;

	return 0;
fail:
	return -1;
}

struct config *monitor_init(void)
{
	struct config *config = malloc(sizeof(struct config));
	memset(config, 0, sizeof(struct config));
	return config;
}

s32 monitor_release(struct config *cfg)
{
	if (cfg){
		if (cfg->output)
			fclose(cfg->output);
		free(cfg);
	}
	return 0;
}

s32 monitor_attach_process(const char *procs_attach, int record)
{
	#define COMM_LEN	128
	int child[3] = { -1, -1, -1 };
	int status[3]= { 0, 0, 0 };
	int id = 0;
	int fd = -1;
	int procs_attach_pid = 0;
	int procs_start = 1100;
	char procs_entry[COMM_LEN];
	char procs_cmdline[COMM_LEN];
	char procs_attach_str[10];
	char procs_record_pwd[60];
	DIR* proc_dir;
	struct dirent *proc_entry;
	FILE *fp;

	if (!procs_attach)
		return -1;

    proc_dir = opendir("/proc");
    if (proc_dir == NULL)
        return -1;

__search:
    while((proc_entry = readdir(proc_dir)) != NULL) {
        id = atoi(proc_entry->d_name);
        if ((id != 0) && (id > procs_start)){
            snprintf(procs_entry, sizeof(procs_entry), "/proc/%d/cmdline", id);
            fp = fopen(procs_entry, "r");
            if (fp) {
                fgets(procs_cmdline, sizeof(procs_cmdline), fp);
                //printf("---path:[%s] [%s]\n",procs_entry, procs_cmdline);
                fclose(fp);
                fp = NULL;
				//printf("--- pid %d %s\n",id, procs_cmdline);
                if (strcmp(procs_cmdline, procs_attach) == 0) {/* process found */
                    procs_attach_pid = id;
					snprintf(procs_attach_str, sizeof(procs_attach_str), "%d", id);
					printf("attach pid %d ,cmdline %s %s ok\n",
							procs_attach_pid, procs_attach, procs_attach_str);
                    break;
                }
            }
        }
    }

	if ((procs_start < 0) && (!procs_attach_pid)) { //not found
		closedir(proc_dir);
		return -1;
	}

	if (!procs_attach_pid ) {  //retry
		procs_start -= 200;
		goto __search;
	}
	closedir(proc_dir);

	snprintf(procs_record_pwd, sizeof(procs_record_pwd),
				"/data/local/tmp/memattach%d.log", record);
	fd = open(procs_record_pwd,(O_RDWR | O_CREAT), 0644);
	if (fd > 0) {
		close(fd);
		fflush(stdout);
		setvbuf(stdout,NULL,_IONBF,0);
		fd = freopen(procs_record_pwd, "w", stdout);
	} else {
		printf("create log %s failed %s\n", procs_record_pwd, strerror(errno));
	}

	child[0] = fork();
	if (child[0] == 0) {
		if (execl("/system/xbin/procmem", "/system/xbin/procmem",
				procs_attach_str, NULL)) {
			printf("pid%d start exec failed %s \n", getpid(), strerror(errno));
			exit(-1);
		}
		exit(0);
	}
	while (waitpid(-1, &status[0], 0) != child[0]);
	  
	printf("\n\n");

	child[1] = fork();
	if (child[1] == 0) {
		printf("pid%d start exec dumpsys \n", getpid());
		printf("pid%d start exec dumpsys \n", getpid());
		if (execl("/system/bin/dumpsys","/system/bin/dumpsys",
				"meminfo", procs_attach, NULL)) {
			printf("pid%d start exec failed %s \n", getpid(), strerror(errno));
			exit(-1);
		}
		exit(0);
	}
	while (waitpid(-1, &status[1], 0) != child[1]);

	printf("\n\n");

	child[2] = fork();
	if (child[2] == 0) {
		if (execl("/system/bin/dumpsys","/system/bin/dumpsys",
				"meminfo", NULL)) {
			printf("pid%d start exec failed %s \n", getpid(), strerror(errno));
			exit(-1);
		}
		exit(0);
	}
	while (waitpid(-1, &status[2], 0) != child[2]);

	return 0;
}


/*******************************************************************
  main
 *******************************************************************/

s32 main(s32 argc, char **argv)
{
	s32 c;
	s32 ret;
	s32 option_index = 0;
	struct config *config = NULL;
	u32 i = ~0;
	u32 ucnt = 0;
	u32 unit = 0;
	u32 mtimes = 0;
	config = monitor_init();
	monitor_system_platform(config);
	while (1) {
		c = getopt_long (argc, argv, "h:o:d:c:t:f:v:s:p:m:u:k:",
				monitor_long_options, &option_index);
		if (c == -1)
			break;

		switch (c) {
			case 'o':
				if (config->output != NULL)
					fclose(config->output);
				config->output = monitor_prepare_output_file(optarg);
				if (config->output == NULL) {
					dprintf("Cannot Create The %s/*.log File, Need Root Permissions!\n",optarg);
					dprintf("you may try adb remount!\n");
					return TEST_EXIT_FAILURE;
				}
				break;

			case 't':
				sscanf(optarg, "%u", &i);
				dprintf("user load time -> %s\n", optarg);
				mtimes = i >0?i*1000:500000;
				do {
					ret = monitor_presensor_tempe(config);
					usleep(mtimes);
				}while(ret==0);
				break;

			case 'c':
				sscanf(optarg, "%u", &i);
				mtimes = i >0?i*1000:500000;
				do {
					ret = monitor_precpu_complex_show(config);
					usleep(mtimes);
				}while(ret==0);
				break;

			case 's':
				sscanf(optarg, "%u", &i);
				mtimes = i >0?i*1000:500000;
				do {
					ret = monitor_precpu_simple_show(config);
					usleep(mtimes);
				}while(ret==0);
				break;

			case 'f':
				sscanf(optarg, "%u", &i);
				mtimes = i >0?i*1000:500000;
				do {
					ret = monitor_precpu_complex_store(config);
					usleep(mtimes);
				}while(ret==0);
				break;
			case 'd':
				mtimes = 3600;
				do {
					ret = monitor_attach_process(optarg, ucnt++);
					sleep(mtimes);
				}while(ret==0);
				break;

			case 'm':
				sscanf(optarg, "%u", &i);
				mtimes = i >0?i*1000:500000;
				do {
					ret = monitor_system_meminfo(config);
					usleep(mtimes);
				}while(ret==0);
				break;

			case 'k':
				sscanf(optarg, "%u", &i);
				mtimes = i >0?i*1000:500000;
				do {
					ret = monitor_system_kswap(config);
					usleep(mtimes);
				}while(ret==0);
				break;

			case 'u':
				sscanf(optarg, "%u", &unit);
				if (unit)
					config->unit = 1;
				else
					config->unit = 0;
				break;
			case 'p':
				sscanf(optarg, "%u", &i);
				break;
			case 'v':
				sscanf(optarg, "%u", &i);
				dprintf("Cpu Monitor Version Debug 1.2.3 For V40 debug\n");
				dprintf("Cpu monitor git update time:2016-08-04\n");
				dprintf("Author:sujiajia@allwinnertech.com\n");
				goto out;
			case 'h':
			case '?':
			default:
				monitor_help_usage_show();
				goto out;
		}
	}

	do {
		mtimes = 500000;
		ret = monitor_precpu_simple_show(config);
		usleep(mtimes);
	}while(ret==0);

out:
	monitor_release(config);
	exit(0);
}

