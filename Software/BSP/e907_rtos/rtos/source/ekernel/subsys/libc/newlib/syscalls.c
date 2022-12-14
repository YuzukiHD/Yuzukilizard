#include <reent.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <rtthread.h>
#include <fcntl.h>
#include <log.h>
#include <kapi.h>
#include <debug.h>
#include <hal_uart.h>

#ifdef RT_USING_PTHREADS
#include <pthread.h>
#endif

#ifdef RT_USING_POSIX
#include <dfs_posix.h>
#endif

#include <sunxi_drv_rtc.h>

static struct timeval sys_time = {0};
static int64_t arch_timer_count = 0;
static rt_sem_t sys_time_sem = NULL;

#ifndef USEC_PER_SEC
#define USEC_PER_SEC    (1000000LL)
#endif
#ifndef NSEC_PER_USEC
#define NSEC_PER_USEC   (1000LL)
#endif

#define NR_MAXOPEN 64

extern void syscall_trap(void);
int cli_console_read(void *console, void *buf, size_t nbytes);
void *get_clitask_console(void);
int cli_console_write(void *console, const void *buf, size_t nbytes);
void *get_default_console(void);

int epdk_flag_to_mode_trans(char **p_mode, __u32 mode);

extern int64_t ktime_get(void);
/* Reentrant versions of system calls.  */

extern int libc_stdio_get_console(void);

int _close_r(struct _reent *ptr, int fd)
{
    __hdle fp;

    if (fd < 3)
    {
        __err("not valid file descriptor.");
        ptr->_errno = EINVAL;
        return -1;
    }
#ifdef CONFIG_MELIS_VIRTUAL_FILESYSTEM
    else if (fd < 3 + NR_MAXOPEN)
    {
        fp = esFSYS_fd2file(fd - 3);
        if (fp == NULL)
        {
            __err("fp is null");
            ptr->_errno = EINVAL;
            return -1;
        }
        ptr->_errno = 0;
        esFSYS_fclose(fp);
        return 0;
    }
#endif
    else
    {
#ifdef CONFIG_RT_USING_DFS
        return close(fd);
#endif
    }
    return 0;
}

int _execve_r(struct _reent *ptr, const char *name, char *const *argv, char *const *env)
{
    /* return "not supported" */
    ptr->_errno = ENOTSUP;
    return -1;
}

int _fcntl_r(struct _reent *ptr, int fd, int cmd, int arg)
{
    /* return "not supported" */
    ptr->_errno = ENOTSUP;
    return -1;
}

int _fork_r(struct _reent *ptr)
{
    /* return "not supported" */
    ptr->_errno = ENOTSUP;
    return -1;
}

int _fstat_r(struct _reent *ptr, int fd, struct stat *pstat)
{
    /* return "not supported" */
    ptr->_errno = S_IFCHR;
    return -1;
}

int _getpid_r(struct _reent *ptr)
{
    return (long)rt_thread_self();
}

int _isatty_r(struct _reent *ptr, int fd)
{
    if (fd >= 0 && fd < 3)
    {
        return 1;
    }

    /* return "not supported" */
    ptr->_errno = ENOTSUP;
    return -1;
}

int _kill_r(struct _reent *ptr, int pid, int sig)
{
    /* return "not supported" */
    ptr->_errno = ENOTSUP;
    return -1;
}

int _link_r(struct _reent *ptr, const char *old, const char *new)
{
    /* return "not supported" */
    ptr->_errno = EMLINK;
    return -1;
}

_off_t _lseek_r(struct _reent *ptr, int fd, _off_t pos, int whence)
{
    __hdle fp;
    if (fd < 3)
    {
        __err("invaild fd.");
        ptr->_errno = EINVAL;
        return -1;
    }
#ifdef CONFIG_MELIS_VIRTUAL_FILESYSTEM
    else if (fd < 3 + NR_MAXOPEN)
    {
        fp = esFSYS_fd2file(fd - 3);
        if (fp == NULL)
        {
            __err("fp is null");
            ptr->_errno = EINVAL;
            return -1;
        }

        //TODO: fseek and ftell combined.
        ptr->_errno = 0;
        esFSYS_fseek(fp, pos, whence);
        return esFSYS_ftell(fp);
    }
#endif
    else
    {
#ifdef CONFIG_RT_USING_DFS
        return lseek(fd, pos, whence);
#endif
    }
    return 0;
}

int _mkdir_r(struct _reent *ptr, const char *name, int mode)
{
    ptr->_errno = ENOTSUP;
    return -1;
}

int _open_r(struct _reent *ptr, const char *file, int flags, int mode)
{
    __hdle fp;
    int fd = 0;
    char *vfs_mode;

    if (file == NULL)
    {
        __err("file name is error.");
        ptr->_errno = EINVAL;
        return -1;
    }

#ifdef CONFIG_MELIS_VIRTUAL_FILESYSTEM
    if (epdk_flag_to_mode_trans(&vfs_mode, flags))
    {
        goto dfs_open;
    }

    fp = esFSYS_fopen(file, vfs_mode);
    if (fp == NULL)
    {
        goto dfs_open;
    }

    ptr->_errno = 0;
    return esFSYS_file2fd(fp) + 3;
#endif

dfs_open:
    fd = open(file, flags);
    if (fd == -1)
    {
        ptr->_errno = EINVAL;
        return -1;
    }
    return fd;
}

char *_gets_r_echo(struct _reent *ptr, char *buf)
{
    register int c;
    register char *s = buf;
    FILE *fp;

    _REENT_SMALL_CHECK_INIT(ptr);
    fp = _stdin_r(ptr);
    c = __sgetc_r(ptr, fp);
    __sputc_r(ptr, c, fp);
    while (c != '\n' && c != '\r')
    {
        if (c == EOF)
        {
            if (s == buf)
            {
                return NULL;
            }
            else
            {
                break;
            }
        }
        else
        {
            *s++ = c;
        }
        c = __sgetc_r(ptr, fp);
        __sputc_r(ptr, c, fp);
    }
    *s = 0;
    __sputc_r(ptr, '\n', fp);
    return buf;
}

char *gets(char *buf)
{
    return _gets_r_echo(_REENT, buf);
}

_ssize_t _read_r(struct _reent *ptr, int fd, void *buf, size_t nbytes)
{
    if (fd < 3 || fd == libc_stdio_get_console())
    {
#ifdef CONFIG_SUBSYS_MULTI_CONSOLE
        if (get_default_console()) {
            return cli_console_read(get_clitask_console(), buf, nbytes);
        } else {
#endif
#ifdef CONFIG_DRIVERS_UART
extern int g_cli_direct_read;
        if (g_cli_direct_read)
            return hal_uart_receive_polling(CONFIG_CLI_UART_PORT, buf, nbytes);
        else
            return hal_uart_receive(CONFIG_CLI_UART_PORT, buf, nbytes);
#else
            return 0;
#endif
#ifdef CONFIG_SUBSYS_MULTI_CONSOLE
        }
#endif
    }
#ifdef CONFIG_MELIS_VIRTUAL_FILESYSTEM
    else if (fd < 3 + NR_MAXOPEN)
    {
        __hdle fp;

        fp = esFSYS_fd2file(fd - 3);
        if (fp == NULL)
        {
            __err("fp is null");
            ptr->_errno = EINVAL;
            return -1;
        }

        ptr->_errno = 0;
        return esFSYS_fread(buf, 1, nbytes, fp);
    }
#endif
    else
    {
#ifdef CONFIG_RT_USING_DFS
        return read(fd, buf, nbytes);
#endif
    }
    return 0;
}

int _rename_r(struct _reent *ptr, const char *old, const char *new)
{
#ifdef CONFIG_MELIS_VIRTUAL_FILESYSTEM
    if (esFSYS_rename(new, old) != EPDK_OK)
    {
        __err("rename %s to %s failure.", old, new);
        ptr->_errno = EINVAL;
        return -1;
    }
    ptr->_errno = 0;
    return 0;
#endif

dfs_rename:
    return rename(old, new);
}

int _stat_r(struct _reent *ptr, const char *file, struct stat *pstat)
{
    ptr->_errno = S_IFCHR;
    return 0;
}

_CLOCK_T_ _times_r(struct _reent *ptr, struct tms *ptms)
{
    /* return "not supported" */
    ptr->_errno = ENOTSUP;
    return -1;
}

int _unlink_r(struct _reent *ptr, const char *file)
{
    ptr->_errno = EMLINK;
    return -1;
}

int _wait_r(struct _reent *ptr, int *status)
{
    /* return "not supported" */
    ptr->_errno = ENOTSUP;
    return -1;
}

_ssize_t _write_r(struct _reent *reent, int fd, const void *buf, size_t nbytes)
{
    if (fd < 3 || fd == libc_stdio_get_console())
    {
        _ssize_t ret = 0;
#ifdef CONFIG_SUBSYS_MULTI_CONSOLE
        if (get_default_console()) {
            ret = cli_console_write(get_clitask_console(), (void *)buf, nbytes);
        } else {
#endif
#ifdef CONFIG_DRIVERS_UART
            /*if console do not init, use the hardware uart_output api*/
            char *b = (char *)buf;
            int i;
            ret = nbytes;
            for (i = 0; i < nbytes; i ++)
            {
                if (*(b + i) == '\n')
                {
                    hal_uart_send(CONFIG_CLI_UART_PORT, "\r", 1);
                }
                hal_uart_send(CONFIG_CLI_UART_PORT, b + i, 1);
            }
#endif
#ifdef CONFIG_SUBSYS_MULTI_CONSOLE
        }
#endif
        return ret <= 0 ? 1 : ret;
    }
#ifdef CONFIG_MELIS_VIRTUAL_FILESYSTEM
    else if (fd < 3 + NR_MAXOPEN)
    {
        __hdle fp;

        fp = esFSYS_fd2file(fd - 3);
        if (fp == NULL)
        {
            __err("fp is null");
            reent->_errno = EINVAL;
            return -1;
        }

        reent->_errno = 0;
        return esFSYS_fwrite(buf, 1, nbytes, fp);
    }
#endif
    else
    {
#ifdef CONFIG_RT_USING_DFS
        return write(fd, buf, nbytes);
#endif
    }
    return 0;
}

#ifndef MILLISECOND_PER_SECOND
#define MILLISECOND_PER_SECOND  1000UL
#endif

#ifndef MICROSECOND_PER_SECOND
#define MICROSECOND_PER_SECOND  1000000UL
#endif

#ifndef NANOSECOND_PER_SECOND
#define NANOSECOND_PER_SECOND   1000000000UL
#endif

#define MILLISECOND_PER_TICK    (MILLISECOND_PER_SECOND / RT_TICK_PER_SECOND)
#define MICROSECOND_PER_TICK    (MICROSECOND_PER_SECOND / RT_TICK_PER_SECOND)
#define NANOSECOND_PER_TICK     (NANOSECOND_PER_SECOND  / RT_TICK_PER_SECOND)

#ifdef RT_USING_DEVICE
static void libc_system_time_init(void)
{
    time_t time;
    rt_tick_t tick;
    rt_device_t device;

    time = 0;
    device = rt_device_find("rtc");
    if (device != RT_NULL)
    {
        /* get realtime seconds */
        rt_device_control(device, RT_DEVICE_CTRL_RTC_GET_TIME, &time);
    }

    /* get tick */
    tick = rt_tick_get();

    sys_time.tv_usec = MICROSECOND_PER_SECOND - (tick % RT_TICK_PER_SECOND) * MICROSECOND_PER_TICK;
    sys_time.tv_sec = time - tick / RT_TICK_PER_SECOND - 1;
}
#endif

int _gettimeofday_r(struct _reent *ptr, struct timeval *tv, void *__tzp)
{
    int fd = -1;
    int ret = -1;
    struct rtc_time rtc_time;
    static int inited = 0;

    rt_sem_take(sys_time_sem, RT_WAITING_FOREVER);

    int64_t nsecs = ktime_get();
    int64_t dnsecs = (nsecs > arch_timer_count) ? (nsecs - arch_timer_count) : 0LL;

    arch_timer_count = nsecs;

    struct timeval tmp;
    tmp.tv_sec  = (dnsecs / NSEC_PER_USEC) / USEC_PER_SEC;
    tmp.tv_usec = (dnsecs - (tmp.tv_sec * NSEC_PER_USEC * USEC_PER_SEC)) / NSEC_PER_USEC;

    if (inited == 0)
    {
#ifdef CONFIG_GET_RTC_TIME
        fd = open("/dev/rtc", O_RDWR);
        if (fd < 0)
        {
            ret = -1;
        }
        else
        {
            memset(&rtc_time, 0, sizeof(struct rtc_time));
            if (!ioctl(fd, RTC_GET_TIME, &rtc_time))
            {
                struct tm tm;
                time_t time;

                memset(&tm, 0, sizeof(tm));
                tm.tm_year = rtc_time.tm_year;
                tm.tm_mon = rtc_time.tm_mon;
                tm.tm_mday = rtc_time.tm_mday;
                tm.tm_hour = rtc_time.tm_hour;
                tm.tm_min = rtc_time.tm_min;
                tm.tm_sec = rtc_time.tm_sec;
                tm.tm_wday = rtc_time.tm_wday;
                tm.tm_yday = rtc_time.tm_yday;
                tm.tm_isdst = rtc_time.tm_isdst;

                time = mktime(&tm);
                sys_time.tv_sec = time;
                sys_time.tv_usec = tmp.tv_usec;
                ret = 0;
            }
            close(fd);
        }
#endif
        inited = 1;
    }

    if (ret)
    {
        sys_time.tv_sec += tmp.tv_sec;
        sys_time.tv_usec += tmp.tv_usec;

        if(sys_time.tv_usec >= (USEC_PER_SEC)) {
            sys_time.tv_sec ++;
            sys_time.tv_usec -= ( USEC_PER_SEC);
        }
    }

    if (tv != NULL)
    {
        tv->tv_sec = sys_time.tv_sec;
        tv->tv_usec = sys_time.tv_usec;
    }

    rt_sem_release(sys_time_sem);

    return 0;

}

int settimeofday(const struct timeval *tm_val, const struct timezone *tz)
{
    time_t time;
    struct tm tm;
    struct rtc_time rtc_time;
    int fd = -1;
    int ret = -1;

#ifdef CONFIG_GET_RTC_TIME
    fd = open("/dev/rtc", O_RDWR);
    if (fd >= 0)
    {
        memset(&tm, 0, sizeof(tm));
        memset(&rtc_time, 0, sizeof(struct rtc_time));

        localtime_r(&tm_val->tv_sec, &tm);

        rtc_time.tm_year = tm.tm_year;
        rtc_time.tm_mon = tm.tm_mon;
        rtc_time.tm_mday = tm.tm_mday;
        rtc_time.tm_hour = tm.tm_hour;
        rtc_time.tm_min = tm.tm_min;
        rtc_time.tm_sec = tm.tm_sec;
        rtc_time.tm_wday = tm.tm_wday;
        rtc_time.tm_yday = tm.tm_yday;
        rtc_time.tm_isdst = tm.tm_isdst;

        if (!ioctl(fd, RTC_SET_TIME, &rtc_time))
        {
            ret = 0;
        }
        close(fd);
    }
#endif

    rt_sem_take(sys_time_sem, RT_WAITING_FOREVER);

    sys_time.tv_sec = tm_val->tv_sec;
    sys_time.tv_usec = tm_val->tv_usec;

    arch_timer_count = ktime_get();

    rt_sem_release(sys_time_sem);
    return 0;
}

void system_time_init(void)
{
    if (sys_time_sem == NULL)
    {
        sys_time_sem = rt_sem_create("sys-time-sem", 1, RT_IPC_FLAG_FIFO);
    }
}

void __malloc_lock(struct _reent *ptr)
{

}

void __malloc_unlock (struct _reent *ptr)
{

}

void* memalign (size_t align, size_t nbytes)
{
    __err("cant be used on melis.");
    software_break();
    return NULL;
}

/* Memory routine */
void *_malloc_r(struct _reent *ptr, size_t size)
{
    void *result;

    result = (void *)rt_malloc(size);
    if (result == RT_NULL)
    {
        ptr->_errno = ENOMEM;
    }

    return result;
}

void *_realloc_r(struct _reent *ptr, void *old, size_t newlen)
{
    void *result;

    result = (void *)rt_realloc(old, newlen);
    if (result == RT_NULL)
    {
        ptr->_errno = ENOMEM;
    }

    return result;
}

void *_calloc_r(struct _reent *ptr, size_t size, size_t len)
{
    void *result;

    result = (void *)rt_calloc(size, len);
    if (result == RT_NULL)
    {
        ptr->_errno = ENOMEM;
    }

    return result;
}

void _free_r(struct _reent *ptr, void *addr)
{
    rt_free(addr);
}

void *aligned_alloc(size_t __alignment, size_t __size)
{
    return rt_malloc_align(__size, __alignment);
}

void aligned_free(void *buf)
{
    rt_free_align(buf);
}

void _exit(int status)
{
    rt_kprintf("thread:%s exit with %d\n", rt_thread_self()->name, status);
    RT_ASSERT(0);

    while (1);
}

void _system(const char *s)
{
    /* not support this call */
    return;
}

void __libc_init_array(void)
{
    /* we not use __libc init_aray to initialize C++ objects */
}

int _isatty(int fd)
{
    if (fd >= 0 && fd < 3)
    {
        return 1;
    }

    return -1;
}

int remove(const char *pathname)
{
#ifdef CONFIG_RT_USING_DFS
    return unlink(pathname);
#else
    return 0;
#endif
}

/*
 * fp is handle-point of libc, but not the esFSYS-system's handle-point,
 * fd arrange map:
 *  0 - stdin;
 *  1 - stdout;
 *  2 - stderr;
 *  3 ~ 66  - esFSYS-system's file descriptor;
 * 67 ~ 130 - dfs-system's file descriptor;
 * but these file-descriptors all are recorded by libc's std-fops throught _open_r's return;
 * so wen can use the libc's handle-point with fileno function to get the file-descriptor, and
 * then recogonized the fd is belong to dfs, or belong to esFSYS-system.
*/
int fioctrl(FILE* fp, int Cmd, void *args)
{
    int     fd = fileno(fp);
    long    *argments = (void*)args;

    if (fd < 3)
    {
        __err("invaild fd(%d).", fd);
        return -1;
    }
#ifdef CONFIG_MELIS_VIRTUAL_FILESYSTEM
    else if (fd < 3 + NR_MAXOPEN)
    {
        return esFSYS_fioctrl(esFSYS_fd2file(fd - 3), Cmd, argments[0], (void*)(argments[1]));
    }
#endif
    else
    {
#ifdef CONFIG_RT_USING_DFS
        return ioctl(fileno(fp), Cmd, args);
#endif
    }
	return 0;
}

#ifdef CONFIG_SYSCALL
/*SWIHandler_NLIBOPS_t*/
const unsigned long SWIHandler_entity_NLIBOPS[] =
{
    (const unsigned long) _close_r                      ,//_close_r         ;
    (const unsigned long) _open_r                       ,//_open_r          ;
    (const unsigned long) _read_r                       ,//_read_r          ;
    (const unsigned long) _write_r                      ,//_write_r         ;
    (const unsigned long) _lseek_r                      ,//_lseek_r         ;
    (const unsigned long) _rename_r                     ,//_rename_r        ;
    (const unsigned long) _stat_r                       ,//_stat_r          ;
    (const unsigned long) _mkdir_r                      ,//_mkdir_r         ;
    (const unsigned long) _unlink_r                     ,//_unlink          ;
    (const unsigned long) _gettimeofday_r               ,//_gettimeofday_r  ;
    (const unsigned long) settimeofday                  ,//settimeofday     ;
    (const unsigned long) _malloc_r                     ,//_malloc_r        ;
    (const unsigned long) _realloc_r                    ,//_realloc_r       ;
    (const unsigned long) _calloc_r                     ,//_calloc_r        ;
    (const unsigned long) _free_r                       ,//_free_r          ;
    (const unsigned long) _isatty_r                     ,//_isatty_r        ;
    (const unsigned long) _isatty                       ,//_isatty          ;
    (const unsigned long) __libc_init_array             ,//__libc_init_array ;
    (const unsigned long) _exit                         ,//_exit            ;
    (const unsigned long) _system                       ,//_system          ;

    (const unsigned long) opendir                       ,//opendir          ;
    (const unsigned long) readdir                       ,//readdir          ;
    (const unsigned long) rewinddir                     ,//rewinddir        ;
    (const unsigned long) rmdir                         ,//rmdir            ;
    (const unsigned long) closedir                      ,//closedir         ;
    (const unsigned long) mkdir                         ,//mkdir            ;
    (const unsigned long) ioctl                         ,//ioctl            ;
    (const unsigned long) remove                        ,//remove           ;

    (const unsigned long) _fstat_r                      ,//_fstat_r         ;

    (const unsigned long) syscall_trap                  ,//fd2file          ;
    (const unsigned long) syscall_trap                  ,//ferrclr          ;
    (const unsigned long) ferror                        ,//ferror           ;
    (const unsigned long) syscall_trap                  ,//file2fd          ;
    (const unsigned long) fioctrl                       ,//fioctrl          ;
    (const unsigned long) syscall_trap                  ,//format           ;
    (const unsigned long) syscall_trap                  ,//fsdbg            ;
    (const unsigned long) syscall_trap                  ,//fseekex          ;
    (const unsigned long) fsync                         ,//fsync            ;
    (const unsigned long) syscall_trap                  ,//ftellex          ;
    (const unsigned long) ftruncate                     ,//ftruncate        ;
    (const unsigned long) syscall_trap                  ,//getfscharset     ;
    (const unsigned long) syscall_trap                  ,//setfs            ;
    (const unsigned long) statfs                        ,//statfs           ;
};
#endif
