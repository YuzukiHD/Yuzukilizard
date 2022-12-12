
#ifndef __CPU_LIB_INCLUDE_H_
#define __CPU_LIB_INCLUDE_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <stdarg.h>
#include <time.h>
#include <dirent.h>
#include <limits.h>
#include <fcntl.h>

#include <sys/time.h>
#include <sys/utsname.h>
#include <sys/types.h>
#include <sys/stat.h>

#if 0
#include <cutils/properties.h>
#endif

#undef DEBUG
#define DEBUG
#ifdef DEBUG
#define dprintf(fmt...)            printf(fmt)
#define B_LOG(fmt...)      		printf(fmt)
#define ERROR(fmt...)              printf(fmt)
#else
#define dprintf(x...)
#define fprintf(fmt, args...)
#define B_LOG(fmt, args...)
#define ERROR(x...)
#endif


#undef  u8
#define u8     unsigned char
#undef  s8
#define s8     char
#undef  u32
#define u32    unsigned int
#undef  s32
#define s32    int

#define TEST_EXIT_SUCCESS     (0)
#define    TEST_EXIT_FAILURE     (-1)
#define CPU_NRS_MAX     (8)
#define MAX_LINE_LEN     255
#define SYSFS_PATH_MAX     255
#define PATH_TO_CPU "/sys/devices/system/cpu/"
#define CPU_IS_ONLINE    	2
#define CPU_IS_OFFLINE      1
#define CPU_IS_FAULT        0
#define CPU_IS_INVALID     (-1)
#define CPU_NR_C0           4
#define CPU_NR_C1           8

#define diff_abs(a,b) (a-b)?(a-b):(b-a);


struct cpufreq_info
{
    u32 online_cpu;
    u32 boot_freq;
    u32 burst_freq;
    u32 cur_freq;
    u32 max_freq;
    u32 min_freq;
    //u32 transition_latency;
    u32 scaling_cur_freq;
    u32 scaling_max_freq;
    u32 scaling_min_freq;
    char buf[1024];
    unsigned int blen;
};

struct cpufreq_governor_info
{
    u32 above_hispeed_delay;
    u32 boost;
    u32 boostpulse;
    u32 boostpulse_duration;
    u32 boosttop;
    u32 boosttop_duration;
    u32 go_hispeed_load;
    u32 io_is_busy;
    u32 min_sample_time;
    u32 target_loads;
    u32 timer_rate;
    u32 timer_slack;
};

struct cpufreq_list
{
    u32 cpufreq_list[CPU_NRS_MAX];
    u32 cpu_online[CPU_NRS_MAX];
};

struct cpu_stat {
    s32 id;
    s32 utime, ntime, stime, itime;
    s32 iowtime, irqtime, sirqtime;
    s32 totalcpuTime;
    s32 totalbusyTime;
};

struct cpu_usage {
    struct cpu_stat stats[CPU_NRS_MAX+1];
    u32 active;
};

struct cpu_loads {
    s32 iowait;
    s32 idle;
    s32 busy;
};

struct config
{
    char governor[15];    /* cpufreq governor */
	char platform_info[32]; /*platform name*/
	char platform_arch[32]; 
    FILE *output;        /* logfile */
    char *output_filename;    /* logfile name, must be freed at the end*/
    struct cpufreq_list cpu_freq;
    struct cpu_usage pre_usages;
    struct cpu_loads cpu_load[CPU_NRS_MAX+1];
    char themp;
    char thempre[6];
	unsigned int max_cpu;
	unsigned int mem_type;/*platform memory type */
	s32 dram_dvfs;
	s32 gpu_dvfs;
	u32 unit;
};

struct proc_stat {
	char comm[128];
	char cmdline[128];
	char state;
	int ppid, pgid, sid, num_threads;
	long cpu_id, priority, nice;
	long unsigned vss, rss, rsslim;
	long ut, st, iot, irt, sirt, delta_busy, delta_total;
};

#define MS_SLEEP(time) usleep((unsigned int)time*1000)

#define ARCH_VM_EVENT_COUNTERS 1
#define ARCH_CMA 1
enum meminfo {
	Total = 0,
	BUFFER,
	SWAPCACHED,
	SWAPTOTAL,
	SWAPFREE,
	VMTOTAL,
	VMUESD,
	MEMTATE_MAX,
};

enum vmstat {
		NR_FREE_PAGES = 0,
		NR_INACTIVE_ANON,
		NR_ACTIVE_ANON,
		NR_INACTIVE_FILE,
		NR_ACTIVE_FILE,
		NR_UNEVICTABLE,
		NR_MLOCK,
		NR_ANON_PAGES,
		NR_MAPPED,
		NR_FILE_PAGES,
		NR_DIRTY,
		NR_WRITEBACK,
		NR_SLAB_RECLAIMABLE,
		NR_SLAB_UNRECLAIMABLE,
		NR_PAGE_TABLE_PAGES,
		NR_KERNEL_STACK,
		NR_UNSTABLE,
		NR_BOUNCE,
		NR_VMSCAN_WRITE,
		NR_VMSCAN_IMMEDIATE_RECLAIM,
		NR_WRITEBACK_TEMP,
		NR_ISOLATED_ANON,
		NR_ISOLATED_FILE,
		NR_SHMEM,
		NR_DIRTIED,
		NR_WRITTEN,
#if defined(ARCH_NUMA)
		NUMA_HIT,
		NUMA_MISS,
		NUMA_FOREIGN,
		NUMA_INTERLEAVE,
		NUMA_LOCAL,
		NUMA_OTHER,
#endif
		NR_ANON_TRANSPARENT_HUGEPAGES,
		NR_FREE_CMA,
		NR_DIRTY_THRESHOLD,
		NR_DIRTY_BACKGROUND_THRESHOLD,
#if defined(ARCH_VM_EVENT_COUNTERS)
		PGPGIN,
		PGPGOUT,
		PSWPIN,
		PSWPOUT,

		PGALLOC_DMA,
		PGALLOC_NORMAL,
		PGALLOC_HIGH,
		PGALLOC_MOVABLE,

		PGFREE,
		PGACTIVATE,
		PGDEACTIVATE,
		
		PGFAULT,
		PGMAJFAULT,

		PGREFILL_DMA,
		PGREFILL_NORMAL,
		PGREFILL_HIGH,
		PGREFILL_MOVABLE,

		PGSTEAL_KSWAPD_DMA,
		PGSTEAL_KSWAPD_NORMAL,
		PGSTEAL_KSWAPD_HIGH,
		PGSTEAL_KSWAPD_MOVABLE,

		PGSTEAL_DIRECT_DMA,
		PGSTEAL_DIRECT_NORMAL,
		PGSTEAL_DIRECT_HIGH,
		PGSTEAL_DIRECT_MOVABLE,

		PGSCAN_KSWAPD_DMA,
		PGSCAN_KSWAPD_NORMAL,
		PGSCAN_KSWAPD_HIGH,
		PGSCAN_KSWAPD_MOVABLE,

		PGSCAN_DIRECT_DMA,
		PGSCAN_DIRECT_NORMAL,
		PGSCAN_DIRECT_HIGH,
		PGSCAN_DIRECT_MOVABLE,
		
#if defined(ARCH_NUMA)
		ZONE_RECLAIM_FAILED,
#endif
		PGINODESTEAL,
		SLABS_SCANNED,
		KSWAPD_INODESTEAL,
		KSWAPD_LOW_WMARK_HIT_QUICKLY,
		KSWAPD_HIGH_WMARK_HIT_QUICKLY,		
		PAGEOUTRUN,
		ALLOCSTALL,
		PGROTATED,
		
#if defined(ARCH_COMPACTION)
		COMPACT_BLOCKS_MOVED,
		COMPACT_PAGES_MOVED,
		COMPACT_PAGEMIGRATE_FAILED,
		COMPACT_STALL,
		COMPACT_FAIL,
		COMPACT_SUCCESS,
#endif

#if defined(ARCH_HUGETLB_PAGE)
		HTLB_BUDDY_ALLOC_SUCCESS,
		HTLB_BUDDY_ALLOC_FAIL,
#endif

#if defined(ARCH_UNEVICTABLE_PAGE)
		UNEVICTABLE_PGS_CULLED,
		UNEVICTABLE_PGS_SCANNED,
		UNEVICTABLE_PGS_RESCUED,
		UNEVICTABLE_PGS_MLOCKED,
		UNEVICTABLE_PGS_MUNLOCKED,
		UNEVICTABLE_PGS_CLEARED,
		UNEVICTABLE_PGS_STRANDED,
		UNEVICTABLE_PGS_MLOCKFREED,
#endif

#if defined(ARCH_TRANSPARENT_HUGEPAGE)
		THP_FAULT_ALLOC,
		THP_FAULT_FALLBACK,
		THP_COLLAPSE_ALLOC,
		THP_COLLAPSE_ALLOC_FAILED,
		THP_SPLIT,
#endif

#if defined(ARCH_CMA)
     	PGCMAIN,
     	PGCMAOUT,
     	PGMIGRATE_CMA_SUCCESS,
     	PGMIGRATE_CMA_FAIL,
#endif

#endif /*ARCH_VM_EVENT_COUNTERS*/
		VMSTATE_MAX	
};

struct vmstat_value {
	s32 vm_slot;
	s32 vm_active[VMSTATE_MAX];
	s32 vm_value[VMSTATE_MAX];
	s32 mm_slot;
	s32 mm_active[12];
	s32 mm_value[12];
};

/* read access to files which contain one numeric value */

enum cpufreq_value {
    AFFECTED_CPUS = 0,
    CPUINFO_BOOT_FREQ,
    CPUINFO_BURST_FREQ,
    CPUINFO_CUR_FREQ,
    CPUINFO_MAX_FREQ,
    CPUINFO_MIN_FREQ,
    CPUINFO_LATENCY,
    SCALING_CUR_FREQ,
    SCALING_MIN_FREQ,
    SCALING_MAX_FREQ,
    STATS_NUM_TRANSITIONS,
    MAX_CPUFREQ_VALUE_READ_FILES
};


enum cpufreq_write {
    WRITE_SCALING_MIN_FREQ,
    WRITE_SCALING_MAX_FREQ,
    WRITE_SCALING_GOVERNOR,
    WRITE_SCALING_SET_SPEED,
    MAX_CPUFREQ_WRITE_FILES
};

enum cpufreq_string {
    SCALING_DRIVER,
    SCALING_GOVERNOR,
    MAX_CPUFREQ_STRING_FILES
};

#ifdef HW_EXTED_USED
static const char *cpufreq_value_files[MAX_CPUFREQ_VALUE_READ_FILES] = {
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

static const char *cpufreq_string_files[MAX_CPUFREQ_STRING_FILES] = {
    [SCALING_DRIVER] = "scaling_driver",
    [SCALING_GOVERNOR] = "scaling_governor",
};

static const char *cpufreq_write_files[MAX_CPUFREQ_WRITE_FILES] = {
    [WRITE_SCALING_MIN_FREQ] = "scaling_min_freq",
    [WRITE_SCALING_MAX_FREQ] = "scaling_max_freq",
    [WRITE_SCALING_GOVERNOR] = "scaling_governor",
    [WRITE_SCALING_SET_SPEED] = "scaling_setspeed",
};

#endif

extern u32 sysfs_cpufreq_get_cpu_list(struct cpufreq_list *list);
extern u32 get_cpufre_params(unsigned int cpu, struct cpufreq_info *ins);
extern s32 read_proc_precpu_stat(struct cpu_usage *usages);
extern s32 read_proc_thermal_stat(void);
extern s32 find_cpu_online(u32 cpu);
extern u32 get_cpufreq_by_cpunr(u32 cpu);
extern s32 read_sysfs_tempe_state(char *buf);
extern s32 read_proc_gpu_dvfs(void);
extern s32 read_proc_dram_dvfs(void);
extern void dump_cpuinfo(char *buf, struct cpu_stat *cur_cpu);
extern s32 read_proc_vmstat(struct vmstat_value *vmstat);
extern s32 read_proc_meminfo(struct vmstat_value *vmstat);
extern s32 read_sys_info_stat(char *buf);
extern s32 read_cpu_info_stat(char *des_buf);

#endif
