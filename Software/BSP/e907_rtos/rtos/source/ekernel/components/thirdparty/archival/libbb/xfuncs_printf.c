/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 * Copyright (C) 2006 Rob Landley
 * Copyright (C) 2006 Denys Vlasenko
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */

/* We need to have separate xfuncs.c and xfuncs_printf.c because
 * with current linkers, even with section garbage collection,
 * if *.o module references any of XXXprintf functions, you pull in
 * entire printf machinery. Even if you do not use the function
 * which uses XXXprintf.
 *
 * xfuncs.c contains functions (not necessarily xfuncs)
 * which do not pull in printf, directly or indirectly.
 * xfunc_printf.c contains those which do.
 */

#include "glibbb.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <fcntl.h>
#include <unistd.h>

/* All the functions starting with "x" call bb_error_msg_and_die() if they
 * fail, so callers never need to check for errors.  If it returned, it
 * succeeded. */
#if 0
#ifndef DMALLOC
/* dmalloc provides variants of these that do abort() on failure.
 * Since dmalloc's prototypes overwrite the impls here as they are
 * included after these prototypes in libbb.h, all is well.
 */
// Warn if we can't allocate size bytes of memory.
void *malloc_or_warn(size_t size)
{
    void *ptr = malloc(size);
    if (ptr == NULL && size != 0)
    {
        printf("out of memory1\n");
        //bb_error_msg(bb_msg_memory_exhausted);
    }
    return ptr;
}

// Die if we can't allocate size bytes of memory.
void *xmalloc(size_t size)
{
    void *ptr = malloc(size);
    if (ptr == NULL && size != 0)
    {
        printf("out of memory2\n");
        //bb_error_msg_and_die(bb_msg_memory_exhausted);
    }
    return ptr;
}

// Die if we can't resize previously allocated memory.  (This returns a pointer
// to the new memory, which may or may not be the same as the old memory.
// It'll copy the contents to a new chunk and free the old one if necessary.)
void *xrealloc(void *ptr, size_t size)
{
    ptr = realloc(ptr, size);
    if (ptr == NULL && size != 0)
    {

        printf("out of memory3\n");
        //bb_error_msg_and_die(bb_msg_memory_exhausted);
    }
    return ptr;
}
#endif /* DMALLOC */

// Die if we can't allocate and zero size bytes of memory.

// Die if we can't copy a string to freshly allocated memory.


// Die if we can't allocate n+1 bytes (space for the null terminator) and copy
// the (possibly truncated to length n) string into it.
char *xstrndup(const char *s, int n)
{
    int m;
    char *t;

    if (0 && s == NULL)
    {
        printf("xstrndup bug\n");
        //bb_error_msg_and_die("xstrndup bug");
    }

    /* We can just xmalloc(n+1) and strncpy into it, */
    /* but think about xstrndup("abc", 10000) wastage! */
    m = n;
    t = (char *) s;
    while (m)
    {
        if (!*t)
        {
            break;
        }
        m--;
        t++;
    }
    n -= m;
    t = xmalloc(n + 1);
    t[n] = '\0';

    return memcpy(t, s, n);
}

void *xmemdup(const void *s, int n)
{
    return memcpy(xmalloc(n), s, n);
}

// Die if we can't open a file and return a FILE* to it.
// Notice we haven't got xfread(), This is for use with fscanf() and friends.
FILE *xfopen(const char *path, const char *mode)
{
    FILE *fp = fopen(path, mode);
    if (fp == NULL)
    {

        printf("can't open '%s'\n", path);
        //bb_perror_msg_and_die("can't open '%s'", path);
    }
    return fp;
}

// Die if we can't open a file and return a fd.
int xopen3(const char *pathname, int flags, int mode)
{
    int ret;

    ret = open(pathname, flags, mode);
    if (ret < 0)
    {
        //bb_perror_msg_and_die("can't open '%s'", pathname);
        printf("can't open '%s'", pathname);
    }
    return ret;
}

// Die if we can't open a file and return a fd.
int xopen(const char *pathname, int flags)
{
    return xopen3(pathname, flags, 0666);
}

// Warn if we can't open a file and return a fd.


// Warn if we can't open a file and return a fd.
int open_or_warn(const char *pathname, int flags)
{
    return open3_or_warn(pathname, flags, 0666);
}

/* Die if we can't open an existing file readonly with O_NONBLOCK
 * and return the fd.
 * Note that for ioctl O_RDONLY is sufficient.
 */
int xopen_nonblocking(const char *pathname)
{
    return xopen(pathname, O_RDONLY | O_NONBLOCK);
}

int xopen_as_uid_gid(const char *pathname, int flags, uid_t u, gid_t g)
{
    int fd;
    uid_t old_euid = geteuid();
    gid_t old_egid = getegid();

    xsetegid(g);
    xseteuid(u);

    fd = xopen(pathname, flags);

    xseteuid(old_euid);
    xsetegid(old_egid);

    return fd;
}



void xrename(const char *oldpath, const char *newpath)
{
    if (rename(oldpath, newpath))
    {

        //bb_perror_msg_and_die("can't move '%s' to '%s'", oldpath, newpath);
        printf("can't move '%s' to '%s'", oldpath, newpath);
    }
}

int rename_or_warn(const char *oldpath, const char *newpath)
{
    int n = rename(oldpath, newpath);
    if (n)
    {

        //bb_perror_msg("can't move '%s' to '%s'", oldpath, newpath);
        printf("can't move '%s' to '%s'", oldpath, newpath);
    }
    return n;
}

void xpipe(int filedes[2])
{
    if (pipe(filedes))
    {

        printf("can't create pipe");
        //bb_perror_msg_and_die("can't create pipe");
    }
}




void xwrite_str(int fd, const char *str)
{
    xwrite(fd, str, strlen(str));
}

// Die with an error message if we can't lseek to the right spot.
off_t xlseek(int fd, off_t offset, int whence)
{
    off_t off = lseek(fd, offset, whence);
    if (off == (off_t) -1)
    {
        if (whence == SEEK_SET)
        {

            //bb_perror_msg_and_die("lseek(%"OFF_FMT"u)", offset);
            printf("lseek(%"OFF_FMT"u)", offset);
        }
        //bb_perror_msg_and_die("lseek");
        printf("lseek......\n");
    }
    return off;
}

int xmkstemp(char *template)
{
    int fd = mkstemp(template);
    if (fd < 0)
    {

        //bb_perror_msg_and_die("can't create temp file '%s'", template);
        printf("can't create temp file '%s'", template);
    }
    return fd;
}

// Die with supplied filename if this FILE* has ferror set.
void die_if_ferror(FILE *fp, const char *fn)
{
    if (ferror(fp))
    {
        /* ferror doesn't set useful errno */
        //bb_error_msg_and_die("%s: I/O error", fn);
        printf("%s: I/O error", fn);
    }
}

// Die with an error message if stdout has ferror set.
void die_if_ferror_stdout(void)
{
    die_if_ferror(stdout, bb_msg_standard_output);
}

int fflush_all(void)
{
    return fflush(NULL);
}


int bb_putchar(int ch)
{
    return putchar(ch);
}

/* Die with an error message if we can't copy an entire FILE* to stdout,
 * then close that file. */
void xprint_and_close_file(FILE *file)
{
    fflush_all();
    // copyfd outputs error messages for us.
    if (bb_copyfd_eof(fileno(file), STDOUT_FILENO) == -1)
    {
        xfunc_die();
    }

    fclose(file);
}

// Die with an error message if we can't malloc() enough space and do an
// sprintf() into that space.

char *xasprintf(const char *format, ...)
{
    va_list p;
    int r;
    char *string_ptr;

    va_start(p, format);
    r = vasprintf(&string_ptr, format, p);
    va_end(p);

    if (r < 0)
    {

        //bb_error_msg_and_die(bb_msg_memory_exhausted);
        printf("out of memory in xasprintf\n");
    }
    return string_ptr;
}

void xsetenv(const char *key, const char *value)
{
    if (setenv(key, value, 1))
    {

        //bb_error_msg_and_die(bb_msg_memory_exhausted);
        printf("out of memory in xsetenv\n");
    }
}

/* Handles "VAR=VAL" strings, even those which are part of environ
 * _right now_
 */
void bb_unsetenv(const char *var
{
    char *tp = strchr(var, '=');

    if (!tp)
    {
        unsetenv(var);
        return;
    }

    /* In case var was putenv'ed, we can't replace '='
     * with NUL and unsetenv(var) - it won't work,
     * env is modified by the replacement, unsetenv
     * sees "VAR" instead of "VAR=VAL" and does not remove it!
     * horror :( */
    tp = xstrndup(var, tp - var);
    unsetenv(tp);
    free(tp);
}

void bb_unsetenv_and_free(char *var)
{
    bb_unsetenv(var);
    free(var);
}

// Die with an error message if we can't set gid.  (Because resource limits may
// limit this user to a given number of processes, and if that fills up the
// setgid() will fail and we'll _still_be_root_, which is bad.)
void xsetgid(gid_t gid)
{
    if (setgid(gid))
    {
        //bb_perror_msg_and_die("setgid");
        printf("setgid");
    }
}

// Die with an error message if we can't set uid.  (See xsetgid() for why.)
void xsetuid(uid_t uid)
{
    if (setuid(uid))
    {

        //bb_perror_msg_and_die("setuid");
        printf("setuid");
    }
}

void xsetegid(gid_t egid)
{
    if (setegid(egid))
    {

        //bb_perror_msg_and_die("setegid");
        printf("setegid");
    }
}

void xseteuid(uid_t euid)
{
    if (seteuid(euid))
    {

        //bb_perror_msg_and_die("seteuid");
        printf("seteuid");
    }
}

// Die if we can't chdir to a new path.
void xchdir(const char *path)
{
    if (chdir(path))
    {

        printf("can't change directory to '%s'", path);
        //bb_perror_msg_and_die("can't change directory to '%s'", path);
    }
}

void xfchdir(int fd)
{
    if (fchdir(fd))
    {

        //bb_perror_msg_and_die("fchdir");
        printf("fchdir");
    }
}

void xchroot(const char *path)
{
    if (chroot(path))
    {
        //bb_perror_msg_and_die("can't change root directory to '%s'", path);
        printf("can't change root directory to '%s'", path);
    }
    xchdir("/");
}

// Print a warning message if opendir() fails, but don't die.
DIR *warn_opendir(const char *path)
{
    DIR *dp;

    dp = opendir(path);
    if (!dp)
    {

        //bb_perror_msg("can't open '%s'", path);
        printf("can't open '%s'", path);
    }
    return dp;
}

// Die with an error message if opendir() fails.
DIR *xopendir(const char *path)
{
    DIR *dp;

    dp = opendir(path);
    if (!dp)
    {

        //bb_perror_msg_and_die("can't open '%s'", path);
        printf("can't open '%s'", path);
    }
    return dp;
}

// Die with an error message if we can't open a new socket.
int xsocket(int domain, int type, int protocol)
{
    int r = socket(domain, type, protocol);

    if (r < 0)
    {
        /* Hijack vaguely related config option */
#if ENABLE_VERBOSE_RESOLUTION_ERRORS
        const char *s = "INET";
# ifdef AF_PACKET
        if (domain == AF_PACKET)
        {
            s = "PACKET";
        }
# endif
# ifdef AF_NETLINK
        if (domain == AF_NETLINK)
        {
            s = "NETLINK";
        }
# endif
        IF_FEATURE_IPV6(if (domain == AF_INET6) s = "INET6";)
            //bb_perror_msg_and_die("socket(AF_%s,%d,%d)", s, type, protocol);
        {
            printf("socket(AF_%s,%d,%d)", s, type, protocol);
        }
#else
        //bb_perror_msg_and_die("socket");
        printf("socket");
#endif
    }

    return r;
}

// Die with an error message if we can't bind a socket to an address.
void xbind(int sockfd, struct sockaddr *my_addr, socklen_t addrlen)
{
    if (bind(sockfd, my_addr, addrlen))
    {

        //bb_perror_msg_and_die("bind");
        printf("bind");
    }
}

// Die with an error message if we can't listen for connections on a socket.
void xlisten(int s, int backlog)
{
    if (listen(s, backlog))
    {

        //bb_perror_msg_and_die("listen");
        printf("listen");
    }
}

/* Die with an error message if sendto failed.
 * Return bytes sent otherwise  */
ssize_t xsendto(int s, const void *buf, size_t len, const struct sockaddr *to,
                socklen_t tolen)
{
    ssize_t ret = sendto(s, buf, len, 0, to, tolen);
    if (ret < 0)
    {
        if (ENABLE_FEATURE_CLEAN_UP)
        {
            close(s);
        }
        //bb_perror_msg_and_die("sendto");
        printf("sendto");
    }
    return ret;
}

// xstat() - a stat() which dies on failure with meaningful error message
void xstat(const char *name, struct stat *stat_buf)
{
    if (stat(name, stat_buf))
    {

        //bb_perror_msg_and_die("can't stat '%s'", name);
        printf("can't stat '%s'", name);
    }
}

void xfstat(int fd, struct stat *stat_buf, const char *errmsg)
{
    /* errmsg is usually a file name, but not always:
     * xfstat may be called in a spot where file name is no longer
     * available, and caller may give e.g. "can't stat input file" string.
     */
    if (fstat(fd, stat_buf))
    {

        //bb_simple_perror_msg_and_die(errmsg);
        printf(errmsg);
    }
}

// selinux_or_die() - die if SELinux is disabled.
void selinux_or_die(void)
{
#if ENABLE_SELINUX
    int rc = is_selinux_enabled();
    if (rc == 0)
    {
        //bb_error_msg_and_die("SELinux is disabled");
        printf("SELinux is disabled");
    }
    else if (rc < 0)
    {
        //bb_error_msg_and_die("is_selinux_enabled() failed");
        printf("is_selinux_enabled() failed");
    }
#else
    //bb_error_msg_and_die("SELinux support is disabled");
    printf("SELinux support is disabled");
#endif
}

int ioctl_or_perror_and_die(int fd, unsigned request, void *argp, const char *fmt, ...)
{
    int ret;
    va_list p;

    ret = ioctl(fd, request, argp);
    if (ret < 0)
    {
        va_start(p, fmt);
        bb_verror_msg(fmt, p, strerror(errno));
        /* xfunc_die can actually longjmp, so be nice */
        va_end(p);
        xfunc_die();
    }
    return ret;
}

int ioctl_or_perror(int fd, unsigned request, void *argp, const char *fmt, ...)
{
    va_list p;
    int ret = ioctl(fd, request, argp);

    if (ret < 0)
    {
        va_start(p, fmt);
        //bb_verror_msg(fmt, p, strerror(errno));
        printf("in ioctl_or_perror:fmt = %s, p =%d\n"fmt, p);
        va_end(p);
    }
    return ret;
}

#if ENABLE_IOCTL_HEX2STR_ERROR
int bb_ioctl_or_warn(int fd, unsigned request, void *argp, const char *ioctl_name)
{
    int ret;

    ret = ioctl(fd, request, argp);
    if (ret < 0)
    {

        //bb_simple_perror_msg(ioctl_name);
        printf("ioctl_name = %s\n", ioctl_name);
    }
    return ret;
}
int bb_xioctl(int fd, unsigned request, void *argp, const char *ioctl_name)
{
    int ret;

    ret = ioctl(fd, request, argp);
    if (ret < 0)
    {

        printf("ioctl_name = %s\n", ioctl_name);
        //bb_simple_perror_msg_and_die(ioctl_name);
    }
    return ret;
}
#else
int bb_ioctl_or_warn(int fd, unsigned request, void *argp)
{
    int ret;

    ret = ioctl(fd, request, argp);
    if (ret < 0)
    {

        //bb_perror_msg("ioctl %#x failed", request);
        printf("ioctl %#x failed", request);
    }
    return ret;
}
int bb_xioctl(int fd, unsigned request, void *argp)
{
    int ret;

    ret = ioctl(fd, request, argp);
    if (ret < 0)
    {

        //bb_perror_msg_and_die("ioctl %#x failed", request);
        printf("ioctl %#x failed", request);
    }
    return ret;
}
#endif

char *xmalloc_ttyname(int fd)
{
    char buf[128];
    int r = ttyname_r(fd, buf, sizeof(buf) - 1);
    if (r)
    {
        return NULL;
    }
    return xstrdup(buf);
}

void generate_uuid(uint8_t *buf)
{
    /* http://www.ietf.org/rfc/rfc4122.txt
     *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
     * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     * |                          time_low                             |
     * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     * |       time_mid                |         time_hi_and_version   |
     * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     * |clk_seq_and_variant            |         node (0-1)            |
     * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     * |                         node (2-5)                            |
     * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     * IOW, uuid has this layout:
     * uint32_t time_low (big endian)
     * uint16_t time_mid (big endian)
     * uint16_t time_hi_and_version (big endian)
     *  version is a 4-bit field:
     *   1 Time-based
     *   2 DCE Security, with embedded POSIX UIDs
     *   3 Name-based (MD5)
     *   4 Randomly generated
     *   5 Name-based (SHA-1)
     * uint16_t clk_seq_and_variant (big endian)
     *  variant is a 3-bit field:
     *   0xx Reserved, NCS backward compatibility
     *   10x The variant specified in rfc4122
     *   110 Reserved, Microsoft backward compatibility
     *   111 Reserved for future definition
     * uint8_t node[6]
     *
     * For version 4, these bits are set/cleared:
     * time_hi_and_version & 0x0fff | 0x4000
     * clk_seq_and_variant & 0x3fff | 0x8000
     */
    pid_t pid;
    int i;

    i = open("/dev/urandom", O_RDONLY);
    if (i >= 0)
    {
        read(i, buf, 16);
        close(i);
    }
    /* Paranoia. /dev/urandom may be missing.
     * rand() is guaranteed to generate at least [0, 2^15) range,
     * but lowest bits in some libc are not so "random".  */
    srand(monotonic_us()); /* pulls in printf */
    pid = getpid();
    while (1)
    {
        for (i = 0; i < 16; i++)
        {
            buf[i] ^= rand() >> 5;
        }
        if (pid == 0)
        {
            break;
        }
        srand(pid);
        pid = 0;
    }

    /* version = 4 */
    buf[4 + 2    ] = (buf[4 + 2    ] & 0x0f) | 0x40;
    /* variant = 10x */
    buf[4 + 2 + 2] = (buf[4 + 2 + 2] & 0x3f) | 0x80;
}

#if BB_MMU
pid_t xfork(void)
{
    pid_t pid;
    pid = fork();
    if (pid < 0)
    {

        //bb_perror_msg_and_die("vfork"+1);
        printf("vfork\n");
    }/* wtf? */
    return pid;
}
#endif

void xvfork_parent_waits_and_exits(void)
{
    pid_t pid;

    fflush_all();
    pid = xvfork();
    if (pid > 0)
    {
        /* Parent */
        int exit_status = wait_for_exitstatus(pid);
        if (WIFSIGNALED(exit_status))
        {
            kill_myself_with_sig(WTERMSIG(exit_status));
        }
        _exit(WEXITSTATUS(exit_status));
    }
    /* Child continues */
}
#endif
#if 0
void xdup2(int from, int to)
{
    if (dup2(from, to) != to)
    {

        //bb_perror_msg_and_die("can't duplicate file descriptor");
        printf("can't duplicate file descriptor");
    }
}
#endif
void xdup2(int from, int to)
{
    close(from);
    fcntl(from, F_DUPFD, to);
}
char *xstrdup(const char *s)
{
    char *t;

    if (s == NULL)
    {
        return NULL;
    }

    t = strdup(s);

    if (t == NULL)
    {

        //bb_error_msg_and_die(bb_msg_memory_exhausted);
        printf("out of memory4\n");
    }

    return t;
}
// Die with an error message if we can't write the entire buffer.
void xwrite(int fd, const void *buf, size_t count)
{
    if (count)
    {
        ssize_t size = full_write(fd, buf, count);
        if ((size_t)size != count)
        {
            /*
             * Two cases: write error immediately;
             * or some writes succeeded, then we hit an error.
             * In either case, errno is set.
             */
            //bb_perror_msg_and_die(size >= 0 ? "short write" : "write error");
            printf("write error\n");
        }
    }
}

void xclose(int fd)
{
    if (close(fd))
    {
        printf("close failed\n");
    }
}

void xunlink(const char *pathname)
{
    if (unlink(pathname))
    {
        printf("can't remove file '%s'", pathname);
    }
}

// "Renumber" opened fd
#if 1
void xmove_fd(int from, int to)
{
    if (from == to)
    {
        return;
    }
    xdup2(from, to);
    close(from);
}
#endif
int open3_or_warn(const char *pathname, int flags, int mode)
{
    int ret;

    ret = open(pathname, flags, mode);
    if (ret < 0)
    {
        //bb_perror_msg("can't open '%s'", pathname);
        printf("in copen3_or_warn:can't open '%s'", pathname);
    }
    return ret;
}
int vasprintf(char **string_ptr, const char *format, va_list p)
{
    int r;
    va_list p2;
    char buf[128];

    va_copy(p2, p);
    r = vsnprintf(buf, 128, format, p);
    va_end(p);

    /* Note: can't use xstrdup/xmalloc, they call vasprintf (us) on failure! */

    if (r < 128)
    {
        va_end(p2);
        *string_ptr = strdup(buf);
        return (*string_ptr ? r : -1);
    }

    *string_ptr = malloc(r + 1);
    r = (*string_ptr ? vsnprintf(*string_ptr, r + 1, format, p2) : -1);
    va_end(p2);

    return r;
}

char *xasprintf(const char *format, ...)
{
    va_list p;
    int r;
    char *string_ptr;

    va_start(p, format);
    r = vasprintf(&string_ptr, format, p);
    va_end(p);

    if (r < 0)
    {

        //bb_error_msg_and_die(bb_msg_memory_exhausted);
        printf("out of memory in xasprintf\n");
    }
    return string_ptr;
}

