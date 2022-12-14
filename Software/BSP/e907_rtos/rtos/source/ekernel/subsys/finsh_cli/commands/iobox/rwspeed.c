#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <stdlib.h>
#include <fcntl.h>
#include <blkpart.h>
#include <sys/time.h>
#include <dfs.h>
#include <rtthread.h>
#include <stdarg.h>
#include <stdint.h>

#include <inttypes.h>
#include <pthread.h>

#define KB ((uint64_t)1024)
#define MB (KB * 1024)
#define GB (KB * 1024)

#define DEFAULT_RUN_TIMES 3
#define DEFAULT_ORG "rwspeed.tmp"
static long DEFAULT_BUF_SIZE = 4 * 1024;

#define VERSION "v0.1.0"
#define COMPILE "Compiled in " __DATE__ " at " __TIME__

#define min(x, y) ({                    \
        typeof(x) min1 = (x);           \
        typeof(y) min2 = (y);           \
        (void) (&min1 == &min2);        \
        min1 < min2 ? min1 : min2; })
#define max(x, y) ({                    \
        typeof(x) max1 = (x);           \
        typeof(y) max2 = (y);           \
        (void) (&max1 == &max2);        \
        max1 < max2 ? max2 : max1; })

int statfs(const char *path, struct statfs *buf);
int msleep(int);

struct rwtask
{
    char *file;
    char *dir;
    char *path;
    unsigned int avg;
    int mul_thread;
    uint64_t size;
    uint64_t r_ms;
    uint64_t w_ms;
    pthread_t pth;
    pthread_barrier_t *barrier;

    struct sys
    {
        uint32_t free_ddr;
        uint32_t total_ddr;
        uint64_t total_flash;
    } sys;
};

#define MAX_TASKS 32

static uint64_t str_to_bytes(const char *str)
{
    uint64_t size;
    char c;

    c = str[strlen(str) - 1];
    size = atoll(str);
    if (size == 0)
    {
        return 0;
    }

    switch (c)
    {
        case '0'...'9':
            return size;
        case 'k':
        case 'K':
            return size * KB;
        case 'm':
        case 'M':
            return size * MB;
        case 'g':
        case 'G':
            return size * GB;
        default:
            return 0;
    }
}

static int randbuf(char *buf, int len)
{
    int x;

    srand((unsigned int)time(NULL));
    while (len)
    {
        int l = min(len, (int)sizeof(int));

        x = rand();
        memcpy(buf, &x, l);

        len -= l;
        buf += l;
    }
    return 0;
}

static inline uint32_t get_free_ddr(void)
{
    rt_uint32_t total = 0;
    rt_uint32_t used = 0;

    rt_memory_info(&total, &used, NULL);
    return (total - used);
}

static inline uint32_t get_total_ddr(void)
{
    rt_uint32_t total = 0;

    rt_memory_info(&total, NULL, NULL);
    return total;
}

static inline uint64_t get_total_flash(const char *dir)
{
    struct statfs sfs;

    if (!dir)
    {
        return 0;
    }

    if (statfs(dir, &sfs) < 0)
    {
        printf("statfs %s failed\n", dir);
        return 0;
    }

    return (uint64_t)sfs.f_bsize * (uint64_t)sfs.f_blocks;
}

static inline uint64_t get_free_flash(const char *dir)
{
    struct statfs sfs;

    if (!dir)
    {
        return 0;
    }

    if (statfs(dir, &sfs) < 0)
    {
        return 0;
    }

    /* get free by f_bavail rather than f_bfree */
    return (uint64_t)sfs.f_bsize * (uint64_t)sfs.f_bfree;
}

static inline int64_t get_file_size(const char *file)
{
    struct stat s;
    int ret;

    ret = stat(file, &s);
    if (ret)
    {
        return 0;
    }
#if 0
    if (!S_ISREG(s.st_mode))
    {
        return 0;
    }
#endif
    return s.st_size;
}

static void show_help(void)
{
    printf("    Usage: rwspeed <-d dir> [-h] [-t avg] [-s size]\n");
    printf("\n");
    printf("\t-h : show this massage and exit\n");
    printf("\t-d : # : the diretory to check [default currect path]\n");
    printf("\t-t # : check times for average\n");
    printf("\t-s # : set file size\n");

    printf("\n");
    printf("  size trailing with k|m|g or not\n");
}

static void print_head(struct rwtask *task, int multi_thread)
{
    time_t t = time(NULL);
    struct sys *sys = &task->sys;

    printf("\n\trwspeed: get seq read/write speed\n\n");
    printf("\tversion: %s\n", VERSION);
    printf("\tbuild: %s\n", COMPILE);
    printf("\tdate: %s\n", ctime(&t));
    printf("\tfree/total ddr: %"PRIu64"/%"PRIu64" KB\n",
           sys->free_ddr / KB, sys->total_ddr / KB);
    if (task->dir)
        printf("\tfree/total flash: %"PRIu64"/%"PRIu64" KB\n",
               get_free_flash(task->dir) / KB, sys->total_flash / KB);
    else
    {
        printf("\tfile size: %"PRIu64" KB\n", get_file_size(task->file));
    }
    printf("\tset file size to %"PRIu64" KB\n", task->size / KB);
    printf("\tset average to %u\n", task->avg);
    printf("\tset multi_thread to %d\n", multi_thread);
    if (task->file)
    {
        printf("\tset check file as %s\n", task->file);
    }
    else
    {
        printf("\tset check diretory as %s\n", task->dir);
    }
    printf("\tset r/w file as %s\n", task->path);
    printf("\tset DEFAULT_BUF_SIZE as %"PRIu64" KB\n", DEFAULT_BUF_SIZE / KB);
}

static void print_end(struct rwtask *task, int multi_thread)
{

    printf("\n");
    printf("\tread average speed: %.2f KB/s\n",
           ((double)task->size * task->avg / KB) / ((double)task->r_ms / 1000));
    printf("\twrite average speed: %.2f KB/s\n",
           ((double)task->size * task->avg / KB) / ((double)task->w_ms / 1000));
}

static int do_remove(struct rwtask *task)
{
    int ret = 0;

    if (task->dir)
    {
        ret = unlink(task->path);
        if (!ret)
        {
            printf("\tremove\t: %s ... OK\n", task->path);
        }
        else
        {
            printf("\tremove\t: %s ... Failed - %s\n", task->path, strerror(errno));
        }
    }
    return ret;
}

static inline uint64_t auto_size(struct rwtask *task)
{
    if (task->dir)
    {
        return get_free_flash(task->dir) * 85 / 100;
    }
    else
    {
        return get_file_size(task->file);
    }
}

static int do_write(struct rwtask *task)
{
    int fd, ret = -1;
    char *buf;
    uint64_t size = 0, start_ms = 0, end_ms = 0, ms = 0;
    struct timeval start = {0}, end = {0};

    printf("\r\twrite\t: %s ... ", task->path);

    buf = malloc(DEFAULT_BUF_SIZE);
    if (!buf)
    {
        printf("malloc failed - %s\n", strerror(errno));
        return -ENOMEM;
    }
    randbuf(buf, DEFAULT_BUF_SIZE);

    fd = open(task->path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
    {
        printf("open failed - %s\n", strerror(errno));
        goto out;
    }

    ret = gettimeofday(&start, NULL);
    if (ret)
    {
        printf("gettimeofday failed - %s\n", strerror(errno));
        goto out;
    }

    ret = -1;
	int total_write = 0;
    size = task->size;
    while (size > 0)
    {
        uint64_t done;
        uint64_t sz;

        sz = min(size, (uint64_t)DEFAULT_BUF_SIZE);
        done = write(fd, buf, sz);
        if (done != sz)
        {
            printf("write failed - %s\n", strerror(errno));
            close(fd);
            goto out;
        }
        total_write += done;
		//printf("has write %d\n", total_write);
        size -= sz;
    }
    fsync(fd);
    close(fd);

    ret = gettimeofday(&end, NULL);
    if (ret)
    {
        printf("gettimeofday failed - %s\n", strerror(errno));
        goto out;
    }

    /* calculate the speed */
    start_ms = start.tv_sec * 1000 + start.tv_usec / 1000;
    end_ms = end.tv_sec * 1000 + end.tv_usec / 1000;
    ms = end_ms - start_ms;
    task->w_ms += ms;
    printf("OK (%.2f KB/s)\n", ((double)task->size / KB) / ((double)ms / 1000));

    ret = 0;
out:
    free(buf);
    return ret;
}

static int do_read(struct rwtask *task)
{
    int fd, ret = -1;
    char *buf;
    uint64_t size = 0, start_ms = 0, end_ms = 0, ms = 0;
    struct timeval start = {0}, end = {0};

    printf("\r\tread\t: %s ... ", task->path);

    buf = malloc(DEFAULT_BUF_SIZE);
    if (!buf)
    {
        printf("malloc failed - %s\n", strerror(errno));
        return -ENOMEM;
    }

    fd = open(task->path, O_RDONLY, 0666);
    if (fd < 0)
    {
        printf("open failed - %s\n", strerror(errno));
        goto out;
    }

    ret = gettimeofday(&start, NULL);
    if (ret)
    {
        printf("gettimeofday failed - %s\n", strerror(errno));
        goto out;
    }

    ret = -1;
    size = task->size ? task->size : auto_size(task);
    while (size > 0)
    {
        uint64_t done;
        uint64_t sz;

        sz = min(size, (uint64_t)DEFAULT_BUF_SIZE);
        done = read(fd, buf, sz);
        if (done != sz)
        {
            printf("read failed - %s\n", strerror(errno));
            close(fd);
            goto out;
        }
        size -= sz;
    }

    ret = gettimeofday(&end, NULL);
    if (ret)
    {
        printf("gettimeofday failed - %s\n", strerror(errno));
        goto out;
    }

    close(fd);

    /* calculate the speed */
    start_ms = start.tv_sec * 1000 + start.tv_usec / 1000;
    end_ms = end.tv_sec * 1000 + end.tv_usec / 1000;
    ms = end_ms - start_ms;
    task->r_ms += ms;
    printf("OK (%.2f KB/s)\n", ((double)task->size / KB) / ((double)ms / 1000));

    ret = 0;
out:
    free(buf);
    return ret;
}

static void *rwspeed_thread(void *task)
{
    int ret = 0;
    int times = 1;
    int avg = ((struct rwtask *)task)->avg;

    while (avg-- > 0)
    {
        printf("rwspeed timers = %d\n", times++);

        ret = do_write(task);
        if (ret)
        {
            goto out;
        }

        ret = do_read(task);
        if (ret)
        {
            goto out;
        }

        ret = do_remove(task);
        if (ret)
        {
            goto out;
        }
    }
out:
    pthread_barrier_wait(((struct rwtask *)task)->barrier);
    if (ret)
    {
        printf("task->path(%s) read/write failed!\n", ((struct rwtask *)task)->path);
    }
    return NULL;
}

static int begin(struct rwtask *task, int multi_thread)
{
    int i;

    print_head(task, multi_thread);

    for (i = 0; i < multi_thread; i++)
    {
        pthread_attr_t attr;
        struct sched_param param;
        char name[32];

        memset(&param, 0, sizeof(struct sched_param));
        memset(&attr, 0, sizeof(pthread_attr_t));

#ifdef CONFIG_SDMMC_CACHE_WRITEBACK_THREAD_PRIO
        param.sched_priority = CONFIG_SDMMC_CACHE_WRITEBACK_THREAD_PRIO;
#else
        param.sched_priority = 22;
#endif
        pthread_attr_init(&attr);
        pthread_attr_setschedparam(&attr, &param);

        pthread_create(&((&task[i])->pth), &attr, rwspeed_thread, &task[i]);
        
        snprintf((char *)&name, sizeof(name), "%s%d", "rwspeed", i);
        pthread_setname_np((&task[i])->pth, (char *)&name);
    }

    pthread_barrier_wait(task->barrier);

    msleep(10);

    for (i = 0; i < multi_thread; i++)
    {
        pthread_join((&task[i])->pth, NULL);
        print_end(&task[i], multi_thread);
    }

    return 0;
}

static int rwspeed_main(int argc, char **argv)
{
    int opts = 0;
    int ret = 0;
    int i;
    struct rwtask *task;
    pthread_barrier_t  barrier;
    struct rwtask g_task[MAX_TASKS] = {0};

    int multi_thread = 0;
    DEFAULT_BUF_SIZE = 4 * 1024;

    for (i = 0; i < sizeof(g_task) / sizeof(g_task[0]); i++)
    {
        memset(&g_task[i], 0, sizeof(g_task[i]));
    }

    task = &g_task[0];

    optind = 0;
    while ((opts = getopt(argc, argv, ":hd:t:s:f:b:m:")) != EOF)
    {
        switch (opts)
        {
            case 'h':
                show_help();
                goto out;
            case 'f':
            {
                if (task->dir)
                {
                    printf("-f and -d is conflicting\n");
                    goto out;
                }
                task->file = optarg;
                break;
            }
            case 'd':
            {
                if (task->file)
                {
                    printf("-f and -d is conflicting\n");
                    goto out;
                }
                /* Alios's spiffs do not support stat directory */
                /*
                struct stat s;

                ret = stat(optarg, &s);
                if (ret) {
                    printf("stat %s failed - %s\n", optarg, strerror(errno));
                    goto out;
                }

                if (!S_ISDIR(s.st_mode)) {
                    printf("%s is not directory\n", optarg);
                    goto out;
                }
                */
                task->dir = optarg;
                break;
            }
            case 't':
            {
                task->avg = atoi(optarg);
                if (!task->avg)
                {
                    printf("average times %s is zero or invalid\n", optarg);
                    goto out;
                }
                break;
            }
            case 'b':
            {
                DEFAULT_BUF_SIZE = atoi(optarg);
                if (DEFAULT_BUF_SIZE == 0)
                {
                    DEFAULT_BUF_SIZE = (4 * 1024);
                }
                break;
            }
            case 's':
            {
                task->size = str_to_bytes(optarg);
                if (!task->size)
                {
                    printf("size %s is zero or invalid\n", optarg);
                    goto out;
                }
                break;
            }
            case 'm':
            {
                multi_thread = atoi(optarg);
                if (!multi_thread)
                {
                    printf("multi_thread %s is zero or invalid\n", optarg);
                    goto out;
                }
                if (multi_thread > MAX_TASKS)
                {
                    printf("just support %d tasks!\n", MAX_TASKS);
                    goto out;
                }
                break;
            }
            case '?':
                printf("invalid option %c\n", optopt);
                goto out;
            case ':':
                printf("option -%c requires an argument\n", optopt);
                show_help();
                goto out;
        }
    }

    if (!task->dir && !task->file)
    {
        printf("Which directory/file to check? Please tell me by '-d/-f'\n");
        show_help();
        goto out;
    }

    if (!task->avg)
    {
        task->avg = DEFAULT_RUN_TIMES;
    }

    if (task->file)
    {
        if (multi_thread > 1)
        {
            printf("can not r/w the same file by multiple-thread!!\n");
            goto out;
        }
        task->path = malloc(strlen(task->file) + 1);
        if (!task->path)
        {
            printf("malloc for path failed\n");
            goto out;
        }
        memset(task->path, 0, strlen(task->file) + 1);
        sprintf(task->path, "%s", task->file);
    }
    else
    {
        task->path = malloc(strlen(task->dir) + sizeof(DEFAULT_ORG) + 5);
        if (!task->path)
        {
            printf("malloc for path failed\n");
            goto out;
        }
        memset(task->path, 0, strlen(task->dir) + sizeof(DEFAULT_ORG) + 5);
        sprintf(task->path, "%s/%s", task->dir, DEFAULT_ORG);
        unlink(task->path);
    }

    if (!task->size)
    {
        task->size = auto_size(task);
    }

    task->sys.free_ddr = get_free_ddr();
    task->sys.total_ddr = get_total_ddr();
    task->sys.total_flash = get_total_flash(task->dir);

    if (multi_thread < 1)
    {
        multi_thread = 1;
    }

    pthread_barrier_init(&barrier, NULL, multi_thread + 1);
    task->barrier = &barrier;

    for (i = 1; i < multi_thread; i++)
    {
        g_task[i].barrier = &barrier;
        memcpy(&g_task[i], task, sizeof(g_task[0]));
        g_task[i].path = malloc(strlen(task->dir) + sizeof(DEFAULT_ORG) + 5);
        if (!g_task[i].path)
        {
            printf("malloc for path failed\n");
            goto free_path;
        }
        memset(g_task[i].path, 0, strlen(task->dir) + sizeof(DEFAULT_ORG) + 5);
        sprintf(g_task[i].path, "%s%d", task->path, i);
    }

    ret = begin(task, multi_thread);

    pthread_barrier_destroy(&barrier);

free_path:
    for (i = 0; i < sizeof(g_task) / sizeof(g_task[0]); i++)
    {
        if (g_task[i].path)
        {
            free(g_task[i].path);
        }
    }
out:
    return ret;
}
FINSH_FUNCTION_EXPORT_CMD(rwspeed_main, __cmd_rwspeed, get seq read / write speed);
