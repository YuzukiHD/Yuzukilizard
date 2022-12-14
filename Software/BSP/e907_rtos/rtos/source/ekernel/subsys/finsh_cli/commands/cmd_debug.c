#include <stdio.h>
#include <stdlib.h>
#ifdef CONFIG_DRIVER_SPINOR
#include <drv/sunxi_drv_spinor.h>
#endif
#include <ktimer.h>

#include <backtrace.h>

#include <hal_cmd.h>
#include <inttypes.h>

#ifdef CONFIG_DRIVERS_SDMMC
#include <sdmmc/card.h>
#include <sdmmc/sdmmc.h>
#endif

void rt_perf_init(void);
void rt_perf_exit(void);

extern void     dump_memory(uint32_t *buf, int32_t len);
extern int32_t  check_virtual_address(unsigned long vaddr);
extern void     esKRNL_MemLeakChk(uint32_t en);

#ifdef CONFIG_COMMAND_BACKTRACE
static int msh_backtrace(int argc, const char **argv)
{
    if (argc < 2)
    {
        backtrace(NULL, NULL, 0, 0, printf);
    }
    else
    {
        backtrace((char *)argv[1], NULL, 0, 0, printf);
    }
    return 0;
}
FINSH_FUNCTION_EXPORT_ALIAS(msh_backtrace, __cmd_backtrace, backtrace command);
#endif

#ifdef CONFIG_COMMAND_MMC_WRITE
int cmd_mmc_write(int argc, char **argv)
{
    uint32_t addr;
    char *err = NULL;
    uint32_t len;
    char *buf;
    uint32_t value;

    if (argc < 4) {
        printf("Usage:mmc_write addr len value (sector)!\n");
        return -1;
    }

    addr = strtoul(argv[1], &err, 0);
    len = strtoul(argv[2], &err, 0);
    value = strtoul(argv[3], &err, 0);

    buf = malloc(len * 512);
    if (!buf) {
        printf("alloc memory failed!\n");
        return -1;
    }
    memset(buf, value, len * 512);
    struct mmc_card *card = mmc_card_open(0);
    mmc_block_write(card, buf, addr, len);
    mmc_card_close(0);

    free(buf);
    return 0;
}
FINSH_FUNCTION_EXPORT_CMD(cmd_mmc_write, mmc_write, mmc card write);
#endif

#ifdef CONFIG_COMMAND_MMC_READ
int cmd_mmc_read(int argc, char **argv)
{
    uint32_t addr;
    char *err = NULL;
    uint32_t len;
    char *buf;

    if (argc < 3) {
        printf("Usage:nor_read addr len (sector)!\n");
        return -1;
    }

    addr = strtoul(argv[1], &err, 0);
    len = strtoul(argv[2], &err, 0);

    buf = malloc(len * 512);
    if (!buf) {
        printf("alloc memory failed!\n");
        return -1;
    }
    memset(buf, 0, len * 512);
    struct mmc_card *card = mmc_card_open(0);
    mmc_block_read(card, buf, addr, len);
    mmc_card_close(0);

    printf("====================\n");
    dump_memory(buf, len * 512 / 4);
    printf("====================\n");

    free(buf);
    return 0;
}
FINSH_FUNCTION_EXPORT_CMD(cmd_mmc_read, mmc_read, mmc card read);
#endif

#ifdef CONFIG_COMMAND_MMLK
static int cmd_mmlk(int argc, const char **argv)
{
    static int i = 0;
    i = (i == 0) ? 1 : 0;
    esKRNL_MemLeakChk(i);
    return 0;
}
FINSH_FUNCTION_EXPORT_ALIAS(cmd_mmlk, __cmd_mmlk, memory leak check command);
#endif

#ifdef CONFIG_RT_USING_TASK_PERF_MONITOR
static int cmd_top(int argc, const char **argv)
{
    rt_perf_init();

    while (1)
    {
        char ch = getchar();
        if (ch == 'q' || ch == 3)
        {
            rt_perf_exit();

            return 0;
        }
        printf("    Please enter 'q' or Ctrl-C to quit top command.\n");
    }
    return 0;
}
FINSH_FUNCTION_EXPORT_ALIAS(cmd_top, __cmd_top, top task);
#endif

#ifdef CONFIG_COMMAND_REBOOT
static int cmd_reboot(int argc, const char **argv)
{
    int i = 0;
    if (argc == 2)
    {
        if (!strcmp(argv[1], "efex"))
        {
            printf("**** Jump to efex! *****\n");
#ifdef CONFIG_DRIVER_RTC
            hal_rtc_set_fel_flag();
#endif
        }
    }
#ifdef CONFIG_DRIVER_SPINOR
    sunxi_driver_spinor_deinit();
#endif
#ifdef CONFIG_DRIVER_WDT
    reset_cpu();
#endif
    return 0;
}
FINSH_FUNCTION_EXPORT_ALIAS(cmd_reboot, __cmd_reboot, reboot or jump to efex);
#endif

#ifdef CONFIG_COMMAND_PANIC
static int cmd_panic(int argc, const char **argv)
{
#if defined CONFIG_ARCH_RISCV
    __asm__ volatile("ebreak \r\n" :::"memory");
#else
    __asm__ volatile(".word 0xe7ffdeff" :::"memory");
#endif
    return 0;
}
FINSH_FUNCTION_EXPORT_ALIAS(cmd_panic, __cmd_panic, enter panic mode);
#endif

static int mmu_check_valid_address(void *start_addr, void *end_addr)
{
    if (start_addr > end_addr && end_addr != NULL)
    {
        return 0;
    }

    if (check_virtual_address((unsigned long)start_addr) != 1)
    {
        return 0;
    }

    if (end_addr != NULL)
    {
        if (check_virtual_address((unsigned long)end_addr) != 1)
        {
            return 0;
        }
    }

    return 1;
}

#ifdef CONFIG_COMMAND_PRINT_MEM
static int cmd_print_mem_value(int argc, const char **argv)
{
    char *err = NULL;
    unsigned long start_addr, end_addr;
    uint32_t len;

    if (argc < 2)
    {
        printf("Unsage: p addr [len]\n");
        return 0;
    }

    if (argc > 2)
    {
        start_addr = strtoul(argv[1], &err, 0);

        len = strtoul(argv[2], &err, 0);
        end_addr = start_addr + len;
        if (mmu_check_valid_address((void *)start_addr, (void *)end_addr) && end_addr != 0)
        {
            dump_memory((uint32_t *)start_addr, len / sizeof(uint32_t));
        }
        else
        {
            printf("Invalid address!\n");
        }
    }
    else
    {
        start_addr = strtoul(argv[1], &err, 0);
        if (mmu_check_valid_address((void *)start_addr, NULL))
        {
            dump_memory((uint32_t *)start_addr, sizeof(unsigned long) / sizeof(uint32_t));
        }
        else
        {
            printf("Invalid address!\n");
        }
    }

    return 0;
}
FINSH_FUNCTION_EXPORT_ALIAS(cmd_print_mem_value, __cmd_p, print memory or register value);
#endif

#ifdef CONFIG_COMMAND_WRITE_MEM
static int cmd_modify_mem_value(int argc, const char **argv)
{
    unsigned long addr, value;
    char *err = NULL;

    if (argc < 3)
    {
        printf("Unsage: m addr value\n");
        return -1;
    }

    addr = strtoul(argv[1], &err, 0);
    value = strtoul(argv[2], &err, 0);

    if (mmu_check_valid_address((void *)addr, NULL))
    {
        *((volatile unsigned int *)addr) = value;
    }
    else
    {
        printf("Invalid address!\n");
    }

    return 0;
}
FINSH_FUNCTION_EXPORT_ALIAS(cmd_modify_mem_value, __cmd_m, modify memory or register value);
#endif

#ifdef CONFIG_SAMPLE_GETTIMEOFDAY
int cmd_gettimeofday(int argc, const char **argv)
{
    struct timespec64 ts = {0LL, 0L};

    do_gettimeofday(&ts);

    printf("SEC: %lld, NSEC: %ld\n", ts.tv_sec, ts.tv_nsec);

    return 0;
}
FINSH_FUNCTION_EXPORT_ALIAS(cmd_gettimeofday, __cmd_gettimeofday, getimeofday);
#endif

#ifdef CONFIG_COMMAND_LISTIRQ
int cmd_listirq(int argc, const char **argv)
{
    void show_irqs(void);
    show_irqs();
    return 0;
}
FINSH_FUNCTION_EXPORT_ALIAS(cmd_listirq, __cmd_listirq, list irq handler.);
#endif

#ifdef CONFIG_COMMAND_SLABINFO
int cmd_slabinfo(int argc, const char **argv)
{
    void slab_info(void);
    slab_info();
    return 0;
}
FINSH_FUNCTION_EXPORT_ALIAS(cmd_slabinfo, __cmd_slabinfo, slab information.);
#endif

#ifdef CONFIG_COMMAND_UNAME
int cmd_uname(int argc, const char **argv)
{
	printf("Melis\n");
	return 0;
}
FINSH_FUNCTION_EXPORT_ALIAS(cmd_uname, __cmd_uname, cat system.);
#endif

#ifdef CONFIG_DYNAMIC_LOG_LEVEL_SUPPORT
void set_log_level(int level);
int  get_log_level(void);

static int cmd_set_log_level(int argc, char **argv)
{
    unsigned long level;
    char *end;

    if (argc != 2)
    {
        printf("Usage: set_log_level level\n");
		printf("       set_log_level 0 : disable __log/__err/__wrn/__inf/__msg and printk output\n");
        printf("       set_log_level 1 : disable __err/__wrn/__inf/__msg output\n");
        printf("       set_log_level 2 : disable __wrn/__inf/__msg output\n");
        printf("       set_log_level 3 : disable __inf/__msg output\n");
        printf("       set_log_level 4 : disable __msg output\n");
        printf("       set_log_level 5 : enable all log output\n");
        return -1;
    }

    level = strtoul(argv[1], &end, 0);
    set_log_level((int)level);
    printf("current level is %d\n", get_log_level());

    return 0;
}
FINSH_FUNCTION_EXPORT_ALIAS(cmd_set_log_level, __cmd_set_log_level, set log level);

static int cmd_get_log_level(int argc, char **argv)
{
    printf("current level is %d\n", get_log_level());
    return 0;
}
FINSH_FUNCTION_EXPORT_ALIAS(cmd_get_log_level, __cmd_get_log_level, get log level);
#endif
