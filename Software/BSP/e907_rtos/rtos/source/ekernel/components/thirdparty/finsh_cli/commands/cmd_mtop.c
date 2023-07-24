#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>

#include "sunxi_hal_mbus.h"

#include <hal_cmd.h>

#define MTOP_VERSION "0.5"


typedef struct NodeInfo
{
    enum mbus_pmu type;
    char *name;
    unsigned int curcnt;
    unsigned int precnt;
    unsigned long long delta;
} NodeInfo;

typedef struct NodeUnit
{
    int  id;
    char *name;
    unsigned int div;
} NodeUnit;

/**
 * NOTE: we allway put totddr at first array whether this node exit or not,
 * fot the totddr is caculated every time.
 */

NodeInfo nodeInfo_sun8i[] =
{
    { MBUS_PMU_TOTAL, "totddr", 0, 0, 0},
    { MBUS_PMU_VE, "ve_ddr", 0, 0, 0},
    { MBUS_PMU_DISP, "de_ddr", 0, 0, 0},
    { MBUS_PMU_RV_SYS, "rv_ddr", 0, 0, 0},
    { MBUS_PMU_DI, "di_ddr", 0, 0, 0},
    { MBUS_PMU_CSI, "csi_ddr", 0, 0, 0},
    { MBUS_PMU_TVD, "tvd_ddr", 0, 0, 0},
    { MBUS_PMU_G2D, "g2d_ddr", 0, 0, 0},
    { MBUS_PMU_OTH, "othddr", 0, 0, 0},
};
NodeUnit nodeUnit_sun8i[] =
{
    { 0, "Byte",  1},
    { 1, "KB",  1024},
    { 2, "MB",  1024 * 1024},
};


static NodeInfo *nodeInfo;
static NodeUnit *nodeUnit;

static unsigned int max;
static unsigned long long total;
static unsigned long long idx;

static int nodeCnt;

static int delay;
static int iter;
static int unit;

static int exit_flag = 0;

static int mtop_read(void);
static void mtop_post(void);
static void mtop_update(void);

static void usage(char *program)
{
    fprintf(stdout, "\n");
    fprintf(stdout, "Usage: %s [-n iter] [-d delay] [-u unit]  [-h]\n", program);
    fprintf(stdout, "    -n NUM   Updates to show before exiting.\n");
    fprintf(stdout, "    -d NUM   Seconds to wait between update.\n");
    fprintf(stdout, "    -u unit  0-Byte, 1-KB, 2-MB. default :%s.\n", nodeUnit[unit].name);
    fprintf(stdout, "    -v Display mtop version.\n");
    fprintf(stdout, "    -h Display this help screen.\n");
    fprintf(stdout, "\n");
}

static void version(void)
{
    fprintf(stdout, "\n");
    fprintf(stdout, "mtop version: %s\n", MTOP_VERSION);
    fprintf(stdout, "\n");
}


static int mtop_read(void)
{
    int i;

    for (i = 1; i < nodeCnt; i++)
    {
        hal_mbus_pmu_get_value(nodeInfo[i].type, &nodeInfo[i].curcnt);
    }

    return 0;
}

static void mtop_post(void)
{
    int i;

    for (i = 1; i < nodeCnt; i++)
    {
        nodeInfo[i].precnt = nodeInfo[i].curcnt;
    }
}

static void mtop_update(void)
{
    int i;
    unsigned int cur_total;
    unsigned int average;
    char buf[1024];

    cur_total = 0;

    nodeInfo[0].delta = 0;
    for (i = 1; i < nodeCnt; i++)
    {
        if (nodeInfo[i].precnt <= nodeInfo[i].curcnt)
        {
            nodeInfo[i].delta = nodeInfo[i].curcnt - nodeInfo[i].precnt;
        }
        else
        {
            nodeInfo[i].delta = (unsigned int)((nodeInfo[i].curcnt + (unsigned long long)(2 ^ 32)) - nodeInfo[i].precnt);
        }

        nodeInfo[i].delta /= nodeUnit[unit].div;
        cur_total += nodeInfo[i].delta;
    }
    nodeInfo[0].delta = cur_total;

    if (cur_total > max)
    {
        max = cur_total;
    }
    total += cur_total;
    idx++;
    average = total / idx;

    fprintf(stdout, "\nPlease enter Ctrl-C or 'q' to quit the command!\n");
    fprintf(stdout, "total: %llu, ", total);
    fprintf(stdout, "num: %llu, ", idx);
    fprintf(stdout, "Max: %u, ", max);
    fprintf(stdout, "Average: %u\n", average);

    if (cur_total == 0)
    {
        cur_total++;
    }

    for (i = 0; i < nodeCnt; i++)
    {
        fprintf(stdout, " %14s    ", nodeInfo[i].name);
        fprintf(stdout, " %14llu %s/s  ", nodeInfo[i].delta / delay, nodeUnit[unit].name);
        fprintf(stdout, " %14.2f\n", (float)nodeInfo[i].delta * 100 / cur_total);
    }
    fprintf(stdout, "\n\n");
}

static void *mtop_start(void *param)
{
    mtop_read();
    mtop_post();

    while (iter == -1 || iter-- > 0)
    {
        sleep(delay);
        mtop_read();
        mtop_update();
        mtop_post();
        if (exit_flag == 1)
        {
            exit_flag = 0;
            break;
        }
    }

    pthread_exit(NULL);
    return NULL;
}

static int cmd_mtop(int argc, char *argv[])
{
    int i;
    unsigned long value, bandwidth;
    int ret;

    pthread_attr_t attr;
    struct sched_param param;
    pthread_t pmtop;

    memset(&param, 0, sizeof(struct sched_param));
    memset(&attr, 0, sizeof(pthread_attr_t));

    param.sched_priority = 31;
    pthread_attr_init(&attr);
    pthread_attr_setschedparam(&attr, &param);

    nodeCnt = sizeof(nodeInfo_sun8i) / sizeof(nodeInfo_sun8i[0]);
    nodeInfo = nodeInfo_sun8i;
    nodeUnit = nodeUnit_sun8i;

    total = 0;
    idx = 0;
    max = 0;
    unit = 0;
    delay = 1;
    iter = -1;

    for (i = 1; i < argc; i++)
    {
        if (!strcmp(argv[i], "-n"))
        {
            if (i + 1 >= argc)
            {
                fprintf(stderr, "Option -n expects an argument.\n");
                usage(argv[0]);
                return -1;
            }
            iter = atoi(argv[++i]);
            // FIXME
            continue;
        }

        if (!strcmp(argv[i], "-d"))
        {
            if (i + 1 >= argc)
            {
                fprintf(stderr, "Option -d expects an argument.\n");
                usage(argv[0]);
                return -1;
            }
            delay = atoi(argv[++i]);
            // FIXME
            continue;
        }

        if (!strcmp(argv[i], "-u"))
        {
            if (i + 1 >= argc)
            {
                fprintf(stderr, "Option -n expects an argument.\n");
                usage(argv[0]);
                return -1;
            }
            unit = atoi(argv[++i]);
            // FIXME
            continue;
        }


        if (!strcmp(argv[i], "-v"))
        {
            version();
            return 0;
        }

        if (!strcmp(argv[i], "-h"))
        {
            usage(argv[0]);
            return 0;
        }

        fprintf(stderr, "Invalid argument \"%s\".\n", argv[i]);
        usage(argv[0]);
        return -1;
    }

    fprintf(stdout, "\n");
    fprintf(stdout, "iter: %d\n", iter);
    fprintf(stdout, "dealy: %d\n", delay);
    fprintf(stdout, "unit: %s\n", nodeUnit[unit].name);
    fprintf(stdout, "\n");

    hal_mbus_pmu_enable();

    ret = pthread_create(&pmtop, &attr, mtop_start, NULL);
    if (ret)
    {
        printf("Error creating task, status was %d\n", ret);
        return -1;
    }

    pthread_setname_np(pmtop, "mtop");

    while (1)
    {
        char cRxed = 0;

        cRxed = getchar();
        if (cRxed == 'q' || cRxed == 3)
        {
            exit_flag = 1;
            return 0;
        }
    }

    return 0;
}
FINSH_FUNCTION_EXPORT_ALIAS(cmd_mtop, __cmd_mtop, test bus width);
