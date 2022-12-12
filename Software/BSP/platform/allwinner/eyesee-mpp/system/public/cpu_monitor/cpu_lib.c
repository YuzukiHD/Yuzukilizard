#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "cpu_lib.h"


const char *cpufreq_value_files[MAX_CPUFREQ_VALUE_READ_FILES] = {
	[AFFECTED_CPUS]     = "affected_cpus",
	[CPUINFO_BOOT_FREQ] = "cpuinfo_boot_freq",
	[CPUINFO_BURST_FREQ]="cpuinfo_burst_freq",
	[CPUINFO_CUR_FREQ] = "cpuinfo_cur_freq",
	[CPUINFO_MAX_FREQ] = "cpuinfo_max_freq",
	[CPUINFO_MIN_FREQ] = "cpuinfo_min_freq",
	[CPUINFO_LATENCY]  = "cpuinfo_transition_latency",
	[SCALING_CUR_FREQ] = "scaling_cur_freq",
	[SCALING_MIN_FREQ] = "scaling_min_freq",
	[SCALING_MAX_FREQ] = "scaling_max_freq",
	[STATS_NUM_TRANSITIONS] = "stats/total_trans"
};

const char *cpufreq_string_files[MAX_CPUFREQ_STRING_FILES] = {
	[SCALING_DRIVER] = "scaling_driver",
	[SCALING_GOVERNOR] = "scaling_governor",
};

const char *cpufreq_write_files[MAX_CPUFREQ_WRITE_FILES] = {
	[WRITE_SCALING_MIN_FREQ] = "scaling_min_freq",
	[WRITE_SCALING_MAX_FREQ] = "scaling_max_freq",
	[WRITE_SCALING_GOVERNOR] = "scaling_governor",
	[WRITE_SCALING_SET_SPEED] = "scaling_setspeed",
};

static s32 sysfs_read_file(const s8 *path, s8 *buf, size_t buflen)
{
	int fd;
	ssize_t numread;

	fd = open(path, O_RDONLY);
	if (fd == -1)
		return -1;

	numread = read(fd, buf, buflen - 1);
	if (numread < 1) {
		close(fd);
		return 0;
	}

	buf[numread] = '\0';
	close(fd);

	return (s32) numread;
}

static u32 sysfs_write_file(const s8 *path, const s8 *buf, size_t buflen)
{
	int fd;
	ssize_t numwrite;

	fd = open(path, O_WRONLY);
	if (fd == -1)
		return 0;

	numwrite = write(fd, buf, buflen);
	if (numwrite < 1) {
		close(fd);
		return 0;
	}

	close(fd);

	return (u32) numwrite;

}


static u32 sysfs_cpufreq_read_file(u32 cpu, const s8 *fname,
		s8 *buf, size_t buflen)
{
	s8 path[SYSFS_PATH_MAX];

	snprintf(path, sizeof(path), PATH_TO_CPU "cpu%u/cpufreq/%s",
			cpu, fname);
	return sysfs_read_file(path, buf, buflen);
}


static u32 sysfs_cpufreq_get_one_value(u32 cpu,
		enum cpufreq_value which)
{
	u32 value;
	u32 len;
	s8 linebuf[MAX_LINE_LEN];
	s8 *endp;

	if (which >= MAX_CPUFREQ_VALUE_READ_FILES)
		return 0;

	len = sysfs_cpufreq_read_file(cpu, cpufreq_value_files[which],
			linebuf, sizeof(linebuf));

	if (len == 0)
		return 0;

	value = strtoul(linebuf, &endp, 0);

	if (endp == linebuf || errno == ERANGE)
		return 0;

	return value;
}
#ifdef HW_EXTEN_USED

static u32 sysfs_cpufreq_write_file(u32 cpu,
		const s8 *fname,
		const s8 *value, size_t len)
{
	s8 path[SYSFS_PATH_MAX];

	snprintf(path, sizeof(path), PATH_TO_CPU "cpu%u/cpufreq/%s",
			cpu, fname);

	return sysfs_write_file(path, value, len);
}


static int sysfs_cpufreq_set_one_value(u32 cpu,
		enum cpufreq_write which,
		const s8 *new_value, size_t len)
{
	if (which >= MAX_CPUFREQ_WRITE_FILES)
		return 0;

	if (sysfs_cpufreq_write_file(cpu, cpufreq_write_files[which],
				new_value, len) != len)
		return -ENODEV;

	return 0;
};



static s8 *sysfs_cpufreq_get_one_string(u32 cpu,
		enum cpufreq_string which)
{
	s8 linebuf[MAX_LINE_LEN];
	s8 *result;
	u32 len;

	if (which >= MAX_CPUFREQ_STRING_FILES)
		return NULL;

	len = sysfs_cpufreq_read_file(cpu, cpufreq_string_files[which],
			linebuf, sizeof(linebuf));
	if (len == 0)
		return NULL;

	result = strdup(linebuf);
	if (result == NULL)
		return NULL;

	if (result[strlen(result) - 1] == '\n')
		result[strlen(result) - 1] = '\0';

	return result;
}

#endif

static s32 sysfs_temperature_get_rawdata(char *them_path, char *tme_buf)
{

	FILE *file = NULL;
	s8 buf[MAX_LINE_LEN];
	s32 i, value =0;
	u32 tmep[6] = { 0 };

	if (!them_path || !tme_buf)
		return -1;
	file = fopen(them_path, "r");
	if (!file) {
		dprintf("Could not open %s.\n", them_path);
		return -1;
	}

	fseek(file,0,SEEK_SET);
	while(fgets(buf,100,file) != NULL)
	{
		if(sscanf(buf, "temperature[%d]:%d", &i, &value) <0) {
			fclose(file);
			return -1;
		}
		tmep[i] = value;
	}
	memcpy(tme_buf, tmep,sizeof(tmep));
	fclose(file);

	return 0;
}

void dump_cpuinfo(char *buf, struct cpu_stat *cur_cpu)
{
	dprintf( "%s cpu info u:%d n:%d s:%d "
			"i:%d iow:%d irq:%d sirq:%d "
			"total-cpu:%d total-busy:%d\n",
			buf,
			cur_cpu->utime, cur_cpu->ntime,
			cur_cpu->stime, cur_cpu->itime, cur_cpu->iowtime,
			cur_cpu->irqtime, cur_cpu->sirqtime,
			cur_cpu->totalcpuTime, cur_cpu->totalbusyTime);
}


u32 get_cpufreq_by_cpunr(u32 cpu)
{
	u32 freq = 0;
	freq = sysfs_cpufreq_get_one_value(cpu, CPUINFO_CUR_FREQ);
	return freq;
}


/************************CPU Test API*****************************/

s32 find_cpu_online(u32 cpu)
{
	s8 path[SYSFS_PATH_MAX];
	s8 linebuf[MAX_LINE_LEN];
	s8 *endp;
	u32 value;
	s32 len;

	snprintf(path, sizeof(path), PATH_TO_CPU"cpu%u/online", cpu);

	len = sysfs_read_file(path, linebuf, sizeof(linebuf));
	if (len < 0)
		return -1;

	value = strtoul(linebuf, &endp, 0);

	if (endp == linebuf || errno == ERANGE)
		return 0;

	return (value + 1);
}


u32 get_cpufre_params(u32 cpu, struct cpufreq_info *ins)
{
	u32 is_online;
	if (ins == NULL)
		return -1;
	is_online = find_cpu_online(cpu);
	if (is_online == CPU_IS_ONLINE) {
		ins->online_cpu = cpu;
		ins->boot_freq     = sysfs_cpufreq_get_one_value(cpu,CPUINFO_BOOT_FREQ);
		ins->burst_freq = sysfs_cpufreq_get_one_value(cpu,CPUINFO_BURST_FREQ);
		ins->cur_freq     = sysfs_cpufreq_get_one_value(cpu,CPUINFO_CUR_FREQ);
		ins->max_freq     = sysfs_cpufreq_get_one_value(cpu,CPUINFO_MAX_FREQ);
		ins->min_freq     = sysfs_cpufreq_get_one_value(cpu,CPUINFO_MIN_FREQ);
		ins->scaling_cur_freq     = sysfs_cpufreq_get_one_value(cpu,SCALING_CUR_FREQ);
		ins->scaling_max_freq     = sysfs_cpufreq_get_one_value(cpu,SCALING_MAX_FREQ);
		ins->scaling_min_freq     = sysfs_cpufreq_get_one_value(cpu,SCALING_MIN_FREQ);
		return 0;
	} else {
		return -1;
	}
}

s32 read_proc_thermal_stat(void)
{
	char *path = NULL;
	u32 value;
	s32 len;
	s8 buf[MAX_LINE_LEN];
	s8 *endp;

	path = "/sys/class/thermal/thermal_zone0/temp";
	len  = sysfs_read_file(path, buf, sizeof(buf));
	if (len <= 0)
		return -1;

	value = strtoul(buf, &endp, 0);

	if (endp == buf || errno == ERANGE)
		return -1;
	return (s32)value;
}

s32 read_proc_dram_dvfs(void)
{
	char *path = NULL;
	char *path_retry = NULL;
	u32 value;
	s32 len;
	s8 buf[MAX_LINE_LEN];
	s8 *endp;

	path = "/sys/devices/platform/sunxi-ddrfreq/devfreq/sunxi-ddrfreq/cur_freq";
	path_retry = "/sys/class/devfreq/dramfreq/cur_freq";
	len  = sysfs_read_file(path, buf, sizeof(buf));

	if (len <= 0) {
		if (sysfs_read_file(path_retry, buf, sizeof(buf))<=0)
			return -1;
	}
	value = strtoul(buf, &endp, 0);

	if (endp == buf || errno == ERANGE)
		return -1;
	return (s32)value;
}

s32 read_proc_gpu_dvfs(void)
{
	char *path = NULL;
	u32 value;
	s32 len;
	s8 buf[MAX_LINE_LEN];
	s8 *endp;

	path = "/sys/kernel/debug/clk/hosc/pll_gpu/clk_rate";
	len  = sysfs_read_file(path, buf, sizeof(buf));
	if (len <= 0)
		return -1;

	value = strtoul(buf, &endp, 0);

	if (endp == buf || errno == ERANGE)
		return -1;
	return (s32)value;
}



s32 read_sys_info_stat(char *des_buf)
{
	FILE *file;
	char buf[68];
	char head[32];
	char item[32];
	char *platform_src = "sunxi_platform";
	if (!des_buf)
		return -1;

	char *sys_patch = "/sys/class/sunxi_info/sys_info";
	file = fopen(sys_patch, "r");
	if (!file) {
		//dprintf("Could not open %s .\n", sys_patch);
		return -1;
	}

	fseek(file,0,SEEK_SET);
	while(fgets(buf,68,file) != NULL){
		if (!strncmp(buf, platform_src, strlen(platform_src))) {
			if(sscanf(buf, "%s : %s", head, item) < 0) {
				fclose(file);
				return -1;
			}
			strncpy(des_buf, item, strlen(item));
			des_buf[strlen(item)] = '\0';
		}
	}
	fclose(file);
	return 0;
}

s32 read_cpu_info_stat(char *des_buf)
{
	FILE *file;
	char buf[68];
	char head[32];
	char item[32];
	char *platform_src = "Processor";
	if (!des_buf)
		return -1;

	char *sys_patch = "/proc/cpuinfo";
	file = fopen(sys_patch, "r");
	if (!file) {
		dprintf("Could not open %s .\n", sys_patch);
		return -1;
	}

	fseek(file,0,SEEK_SET);
	while(fgets(buf,68,file) != NULL){
		if (!strncmp(buf, platform_src, strlen(platform_src))) {
			if(sscanf(buf, "%s : %s ", head, item) < 0) {
				fclose(file);
				return -1;
			}
			strncpy(des_buf, item, strlen(item));
			des_buf[strlen(item)] = '\0';
		}
	}
	fclose(file);
	return 0;
}

s32 read_proc_precpu_stat(struct cpu_usage *usages)
{
	FILE *file;
	char buf[100];
	s32 i = 0;
	s32 item = 0;
	struct cpu_stat cur_cpu;
	if (!usages)
		return -1;

	file = fopen("/proc/stat", "r");
	if (!file) {
		dprintf("Could not open /proc/stat.\n");
		return -1;
	}

	fseek(file,0,SEEK_SET);
	while((fgets(buf,100,file) != NULL) && (!strncmp(buf, "cpu",3)))
	{
		item = 0;
		memset(&cur_cpu, 0 , sizeof(cur_cpu));
		if (i == 0) {
			//printf("+++++ buf %s\n",buf);
			if(sscanf(buf, "cpu  %d %d %d %d %d %d %d",
						&cur_cpu.utime, &cur_cpu.ntime,
						&cur_cpu.stime, &cur_cpu.itime,
						&cur_cpu.iowtime, &cur_cpu.irqtime,
						&cur_cpu.sirqtime) < 0) {
				fclose(file);
				return -1;
			}
			item = 8;
			i++;
		} else {
			//printf("+++++ buf %s\n",buf);
			if (sscanf(buf, "cpu%d  %d %d %d %d %d %d %d",
						&cur_cpu.id, &cur_cpu.utime, &cur_cpu.ntime,
						&cur_cpu.stime, &cur_cpu.itime,
						&cur_cpu.iowtime, &cur_cpu.irqtime,
						&cur_cpu.sirqtime) < 0) {
				fclose(file);
				return -1;
			}
			item = cur_cpu.id;
		}
		cur_cpu.totalbusyTime = cur_cpu.utime + cur_cpu.stime +
			cur_cpu.ntime + cur_cpu.irqtime +
			cur_cpu.sirqtime;

		cur_cpu.totalcpuTime =     cur_cpu.totalbusyTime + cur_cpu.itime +
			cur_cpu.iowtime;

		//dump_cpuinfo(&cur_cpu);
		usages->active |= (1UL<<item);
		memcpy(&usages->stats[item], &cur_cpu, sizeof(cur_cpu));
		//dump_cpuinfo(&cur_cpu);
	}
	fclose(file);
	return 0;
}


s32 read_sysfs_tempe_state(char *buf)
{
	int find = -1;
	const char *dirname = "/sys/class/input";
	char devname[256];
	char *filename;
	DIR *root;
	DIR *sub;
	struct dirent *dir_root;
	struct dirent *dir_sub;

	if (!buf)
		return -1;
	root = opendir(dirname);
	if(root == NULL)
		return -1;

	filename = &devname[0];
	strcpy(devname, dirname);
	filename +=  strlen(devname);
	*filename++ = '/';

	while((dir_root = readdir(root))) {
		if ((dir_root->d_type & 0x0a)&&
				(strncmp(dir_root->d_name, "input",5)==0)){
			strcpy(filename, dir_root->d_name);
			filename += strlen(dir_root->d_name);
			sub = opendir(devname);
			if(sub == NULL)
				return -1;
			while((dir_sub = readdir(sub))) {
				if ((strcmp(dir_sub->d_name,"temperature")==0)) {
					*filename++ = '/';
					strcpy(filename, dir_sub->d_name);
					find = 1;
					break;
				}
			}
			if (find == 1) {
				break;
			} else {
				filename -= strlen(dir_root->d_name);
			}
			closedir(sub);
		}
	}

	closedir(root);

	if (find != 1) {
		printf("%s Not found temp sysfs\n",__func__);
		return -1;
	}

	if (sysfs_temperature_get_rawdata(devname, buf) <0) {
		printf("%s Read Temp sysfs Err\n",__func__);
		return -1;
	}

	return 0;
}

s32 read_proc_vmstat(struct vmstat_value *vmstat)
{
	char *vmstat_sys = NULL;
	char buf[100];
	char temp[40];
	s32 tmp_value;
	int i = 0;

	const char *vmstat_info[VMSTATE_MAX+1] = {
		"nr_free_pages",
		"nr_inactive_anon",
		"nr_active_anon",
		"nr_inactive_file",
		"nr_active_file",
		"nr_unevictable",
		"nr_mlock",
		"nr_anon_pages",
		"nr_mapped",
		"nr_file_pages",
		"nr_dirty",
		"nr_writeback",
		"nr_slab_reclaimable",
		"nr_slab_unreclaimable",
		"nr_page_table_pages",
		"nr_kernel_stack",
		"nr_unstable",
		"nr_bounce",
		"nr_vmscan_write",
		"nr_vmscan_immediate_reclaim",
		"nr_writeback_temp",
		"nr_isolated_anon",
		"nr_isolated_file",
		"nr_shmem",
		"nr_dirtied",
		"nr_written",
#if defined(ARCH_NUMA)
		"numa_hit",
		"numa_miss",
		"numa_foreign",
		"numa_interleave",
		"numa_local",
		"numa_other",
#endif
		"nr_anon_transparent_hugepages",
		"nr_free_cma",
		"nr_dirty_threshold",
		"nr_dirty_background_threshold",
#if defined(ARCH_VM_EVENT_COUNTERS)
		"pgpgin",
		"pgpgout",
		"pswpin",
		"pswpout",

		"pgalloc_dma",
		"pgalloc_normal",
		"pgalloc_high",
		"pgalloc_movable",

		"pgfree",
		"pgactivate",
		"pgdeactivate",
		
		"pgfault",
		"pgmajfault",

		"pgrefill_dma",
		"pgrefill_normal",
		"pgrefill_high",
		"pgrefill_movable",

		"pgsteal_kswapd_dma",
		"pgsteal_kswapd_normal",
		"pgsteal_kswapd_high",
		"pgsteal_kswapd_movable",

		"pgsteal_direct_dma",
		"pgsteal_direct_normal",
		"pgsteal_direct_high",
		"pgsteal_direct_movable",

		"pgscan_kswapd_dma",
		"pgscan_kswapd_normal",
		"pgscan_kswapd_high",
		"pgscan_kswapd_movable",

		"pgscan_direct_dma",
		"pgscan_direct_normal",
		"pgscan_direct_high",
		"pgscan_direct_movable",
		
#if defined(ARCH_NUMA)
		"zone_reclaim_failed",
#endif
		"pginodesteal",
		"slabs_scanned",
		"kswapd_inodesteal",
		"kswapd_low_wmark_hit_quickly",
		"kswapd_high_wmark_hit_quickly",		
		"pageoutrun",
		"allocstall",
		"pgrotated",
		
#if defined(ARCH_COMPACTION)
		"compact_blocks_moved",
		"compact_pages_moved",
		"compact_pagemigrate_failed",
		"compact_stall",
		"compact_fail",
		"compact_success",
#endif

#if defined(ARCH_HUGETLB_PAGE)
		"htlb_buddy_alloc_success",
		"htlb_buddy_alloc_fail",
#endif

#if defined(ARCH_UNEVICTABLE_PAGE)
		"unevictable_pgs_culled",
		"unevictable_pgs_scanned",
		"unevictable_pgs_rescued",
		"unevictable_pgs_mlocked",
		"unevictable_pgs_munlocked",
		"unevictable_pgs_cleared",
		"unevictable_pgs_stranded",
		"unevictable_pgs_mlockfreed",
#endif		
		
#if defined(ARCH_TRANSPARENT_HUGEPAGE)
		"thp_fault_alloc",
		"thp_fault_fallback",
		"thp_collapse_alloc",
		"thp_collapse_alloc_failed",
		"thp_split",
#endif

#if defined(ARCH_CMA)
       "pgcmain",
       "pgcmaout",
       "pgmigrate_cma_success",
       "pgmigrate_cma_fail",
#endif

#endif /*ARCH_VM_EVENT_COUNTERS*/
		NULL,
};
	if (vmstat == NULL) {
		ERROR("Invalid value!\n");
		goto fail;
	}
	vmstat->vm_slot = VMSTATE_MAX;

	vmstat_sys = "/proc/vmstat";
	FILE *file = fopen(vmstat_sys, "r");
	if (!file) {
		ERROR("Could not open %s .\n", vmstat_sys);
		goto fail;
	}
	fseek(file,0,SEEK_SET);
	while(fgets(buf,100,file) != NULL)
	{
		//dprintf("+++ buf %s", buf);
		if (vmstat->vm_active[vmstat->vm_slot-1] == 1)
			break;

		for (i=0; i<VMSTATE_MAX; i++) {
			//dprintf("i:%d %d", i, vmstat->vm_active[i]);
			if (vmstat->vm_active[i] != 0)
				continue;
			if (!strncmp(buf, vmstat_info[i], strlen(vmstat_info[i]))) {
				if(sscanf(buf, "%s %d", temp, &tmp_value) < 0) {
					ERROR("buf %s get value fail! \n",buf);
					fclose(file);
					goto fail;
				}
				//dprintf("--- cpu monitor get buf: %s i:%d  value:%d \n", temp, i, tmp_value);
				vmstat->vm_value[i]= tmp_value;
				vmstat->vm_active[i] = 1;
				break;
			}
		}
	}

	fclose(file);

	return 0;
fail:
	return -1;
}

s32 read_proc_meminfo(struct vmstat_value *vmstat)
{
	s32 meminfo_value[29];
	char *meminfo_sys = NULL;
	char buf[100];
	char temp[38];
	s32 tmp_value;
	int i = 0;

	const char *mem_info[7] = {
		"MemTotal",
		"Buffers",
		"SwapCached",
		"SwapTotal",
		"SwapFree",
		"VmallocTotal",
		"VmallocUsed",
	};

	if (vmstat == NULL) {
		ERROR("Invalid value!\n");
		goto fail;
	}
	vmstat->mm_slot = MEMTATE_MAX;

	meminfo_sys = "/proc/meminfo";
	FILE *file = fopen(meminfo_sys, "r");
	if (!file) {
		ERROR("Could not open %s .\n", meminfo_sys);
		goto fail;
	}
	fseek(file,0,SEEK_SET);
	while(fgets(buf,100,file) != NULL)
	{
		//dprintf("+++ buf %s", buf);
		if (vmstat->mm_active[vmstat->mm_slot-1] == 1)
			break;

		for (i=0; i<MEMTATE_MAX; i++) {
			//dprintf("i:%d %d", i, vmstat->mm_active[i]);
			if (vmstat->mm_active[i] != 0)
				continue;
			if (!strncmp(buf, mem_info[i], strlen(mem_info[i]))) {
				if(sscanf(buf+strlen(mem_info[i])+1, "%d %s", &tmp_value, temp) < 0) {
					ERROR("buf %s get value fail! \n",buf);
					fclose(file);
					goto fail;
				}
				//dprintf("--- buf: %s i:%d  value:%d \n", temp, i, tmp_value);
				vmstat->mm_value[i]= tmp_value;
				vmstat->mm_active[i] = 1;
				break;
			}
		}
	}
	fclose(file);

	return 0;
fail:
	return -1;
}

static int read_process_procinfo(char *filename, struct proc_stat *proc)
{
	FILE *file = NULL;
	char *start_p = NULL;
	char *end_p = NULL;
	int ret = -1;
	char buf[256];

	long unsigned temp[10];

	if (!filename || !proc) {
		ERROR("invalid param !\n");
		goto out;
	}

	file = fopen(filename, "r");
	if (!file) {
		ERROR("Could not open %s.\n", filename);
		goto out;
	}
	fgets(buf, 256, file);
	fclose(file);

	start_p = strrchr(buf, ')');

	if (!start_p)
		goto out;
	B_LOG("%s \n ", start_p);
	//sscanf(start_p + 1, " %c %ld %ld",
	//	&proc->state, &proc->ut, &proc->st);
	/* Scan rest of string. */
	//sscanf(start_p + 1, " %c %*d %*d %*d %*d %*d %*d %*d %*d %*d %*d "
	//					"%lu %lu %*d %*d %*d %*d %*d %*d %*d %lu %ld "
	//					"%*d %*d %*d %*d %*d %*d %*d %*d %*d %*d %*d %*d %*d %*d %d",
	//					&proc->state, &proc->ut, &proc->st);

	sscanf(start_p + 1, " %c %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu",
			&proc->state, &temp[0], &temp[1], &temp[2], &temp[3], &temp[4], &temp[5], &temp[6], &temp[7], &temp[8], &temp[9],
			&proc->ut, &proc->st);
	B_LOG("%c %ld %ld \n ", proc->state, proc->ut, proc->st);

	ret = 0;
out:
	return ret;
}






