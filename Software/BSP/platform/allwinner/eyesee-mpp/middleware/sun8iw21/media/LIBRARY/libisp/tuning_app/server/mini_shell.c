/*
 * mini shell, pseudo terminal
 * refers to adb(android debug bridge)
 * author: clarkyy
 * date:   20160803
 * http://www.allwinnertech.com
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mini_shell.h"

#ifndef TEMP_FAILURE_RETRY
#define TEMP_FAILURE_RETRY(exp) ({             \
	typeof (exp) _rc;                      \
	do {                                   \
		_rc = (exp);                   \
	} while (_rc == -1 && errno == EINTR); \
	_rc; })
#endif


#define MINI_SHELL_DEBUG_EN     0
#if MINI_SHELL_DEBUG_EN
char       g_dbg_buffer[1024];
#define D(format, ...) \
    sprintf(g_dbg_buffer, format, ##__VA_ARGS__); \
    mini_shell_stdout(g_dbg_buffer);
#else
#define D(format, ...)
#endif


typedef  pthread_t                 adb_thread_t;
typedef void*  (*adb_thread_func_t)( void*  arg );
typedef struct stinfo {
	void (*func)(int fd, void *cookie);
	int fd;
	void *cookie;
} stinfo;


static __inline__ void  close_on_exec(int  fd)
{
	fcntl( fd, F_SETFD, FD_CLOEXEC );
}

static int  unix_open(const char*  path, int options,...)
{
	if ((options & O_CREAT) == 0)
		return  TEMP_FAILURE_RETRY( open(path, options) );
	else {
		int      mode;
		va_list  args;
		va_start( args, options );
		mode = va_arg( args, int );
		va_end( args );
		return TEMP_FAILURE_RETRY( open( path, options, mode ) );
	}
}

int adb_open( const char*  pathname, int  options )
{
	int  fd = TEMP_FAILURE_RETRY( open( pathname, options ) );
	if (fd < 0)
		return -1;
	close_on_exec( fd );
	return fd;
}

int adb_read(int  fd, void*  buf, size_t  len)
{
    return TEMP_FAILURE_RETRY( read( fd, buf, len ) );
}

int adb_write(int  fd, const void*  buf, size_t  len)
{
	return TEMP_FAILURE_RETRY( write( fd, buf, len ) );
}

__inline__ int adb_close(int fd)
{
    return close(fd);
}

static void init_subproc_child()
{
	D("init_subproc_child\n");
	setsid();

	// Set OOM score adjustment to prevent killing
	int fd = adb_open("/proc/self/oom_score_adj", O_WRONLY | O_CLOEXEC);
	if (fd >= 0) {
		adb_write(fd, "0", 1);
		adb_close(fd);
	} else
		D("adb: unable to update oom_score_adj\n");
}

static void subproc_waiter_service(int fd, void *cookie)
{
	pid_t pid = (pid_t) (uintptr_t) cookie;

	D("entered. fd=%d of pid=%d\n", fd, pid);
	for (;;) {
		int status;
		pid_t p = waitpid(pid, &status, 0);
		if (p == pid) {
			D("fd=%d, post waitpid(pid=%d) status=%04x\n", fd, p, status);
			if (WIFSIGNALED(status)) {
				D("*** Killed by signal %d\n", WTERMSIG(status));
				break;
			} else if (!WIFEXITED(status)) {
				D("*** Didn't exit!!. status %d\n", status);
				break;
			} else if (WEXITSTATUS(status) >= 0) {
				D("*** Exit code %d\n", WEXITSTATUS(status));
				break;
			}
		}
	}
	D("shell exited fd=%d of pid=%d err=%d\n", fd, pid, errno);
	//if (SHELL_EXIT_NOTIFY_FD >=0) {
	//	int res;
	//	res = writex(SHELL_EXIT_NOTIFY_FD, &fd, sizeof(fd));
	//	D("notified shell exit via fd=%d for pid=%d res=%d errno=%d\n",
	//	SHELL_EXIT_NOTIFY_FD, pid, res, errno);
	//}
}

static int create_subproc_pty(const char *cmd, const char *arg0, const char *arg1, pid_t *pid)
{
	int ptm = 0, pts = 0;
		
	D("create_subproc_pty(cmd=%s, arg0=%s, arg1=%s)\n", cmd, arg0, arg1);

	ptm = unix_open("/dev/ptmx", O_RDWR | O_CLOEXEC); // | O_NOCTTY);
	if (ptm < 0) {
		D("[ cannot open /dev/ptmx - %s ]\n",strerror(errno));
		return -1;
	}
    D("%s: %d\n", __FUNCTION__, __LINE__);
	char devname[64];
	if (grantpt(ptm) || unlockpt(ptm) || ptsname_r(ptm, devname, sizeof(devname)) != 0) {
		D("[trouble with /dev/ptmx - %s ]\n", strerror(errno));
		adb_close(ptm);
		return -1;
	}
	D("devname: %s\n", devname);

	D("%s: %d\n", __FUNCTION__, __LINE__);

	*pid = fork();
	if (*pid < 0) {
		D("- fork failed: %s -\n", strerror(errno));
		adb_close(ptm);
		return -1;
	}

	D("%s: %d\n", __FUNCTION__, __LINE__);

	if (*pid == 0) {
		D("%s: %d\n", __FUNCTION__, __LINE__);
		init_subproc_child();

		D("%s: %d\n", __FUNCTION__, __LINE__);
		pts = unix_open(devname, O_RDWR | O_CLOEXEC);
		if (pts < 0) {
			adb_close(ptm);
			D("child failed to open pseudo-term slave: %s\n", devname);
			exit(-1);
		}

		D("%s: %d\n", __FUNCTION__, __LINE__);
		dup2(pts, STDIN_FILENO);
		dup2(pts, STDOUT_FILENO);
		dup2(pts, STDERR_FILENO);

		D("%s: %d\n", __FUNCTION__, __LINE__);
		adb_close(pts);
		adb_close(ptm);

		D("%s: %d\n", __FUNCTION__, __LINE__);
		execl(cmd, cmd, arg0, arg1, NULL);
		D("- exec '%s' failed: %s (%d) -\n",
			cmd, strerror(errno), errno);
		exit(-1);
	} else {
		return ptm;
	}
}

static void *service_bootstrap_func(void *x)
{
	stinfo *sti = x;
	D("%s: %d\n", __FUNCTION__, __LINE__);
	sti->func(sti->fd, sti->cookie);
	free(sti);
	return 0;
}

static int  adb_thread_create( adb_thread_t  *pthread, adb_thread_func_t  start, void*  arg )
{
	pthread_attr_t   attr;

	pthread_attr_init (&attr);
	pthread_attr_setdetachstate (&attr, PTHREAD_CREATE_DETACHED);

	return pthread_create( pthread, &attr, start, arg );
}

#if 0
/*
 * compare current pts output to last pts output,
 * the real output saved in cur_output
 * returns length of cur_output, NOT includes '\0'
 */
static int calc_pts_output(const char *cur_output, const char *last_output)
{
	int cur_len = 0, last_len = 0;
	const char *cur_ptr = NULL, *last_ptr = NULL;
	
	if (cur_output && last_output) {
		cur_len = strlen(cur_output);
		last_len = strlen(last_output);
		
		if (cur_len > 0 && cur_len == last_len) {
			cur_ptr = cur_output + cur_len - 1;      // skip '\0'
			last_ptr = last_output + last_len - 1;   // skip '\0'
			while (cur_len-- > 0 && *cur_ptr-- == *last_ptr--);
			if (cur_len == 0) {
				cur_len = last_len;
			} else {
				cur_len++;
			}
		}
	}

	return cur_len;
}
#endif


// for pc, ADB_HOST = 1; for android, ADB_HOST = 0
#define ADB_HOST 1
#if ADB_HOST
#define SHELL_COMMAND "/bin/sh"
#else
#define SHELL_COMMAND "/system/bin/sh"
#endif
static int fd_log_file = -1;
static int fd_ptmx = -1;
static int stdin_copy = -1;
static int stdout_copy = -1;
static int stderr_copy = -1;
static char cmd_reply[1024] = { 0 };

int init_mini_shell(const char *log_file)
{
	stinfo *sti;
	adb_thread_t t;
	pid_t pid;

	//exit_mini_shell();

	stdin_copy = dup(STDIN_FILENO);
	stdout_copy = dup(STDOUT_FILENO);
	stderr_copy = dup(STDERR_FILENO);

	D("%s: %d %d %d\n", __FUNCTION__, stdin_copy, stdout_copy, stderr_copy);

	fd_ptmx = create_subproc_pty(SHELL_COMMAND, "-", NULL, &pid);
	if (fd_ptmx < 0) {
		D("cannot create subproc pty\n");
		return -1;
	}
	D("create_subprocess() ret_fd=%d pid=%d\n", fd_ptmx, pid);

	sti = malloc(sizeof(stinfo));
	if (sti == 0) {
		D("cannot allocate stinfo\n");
		return -1;
	}

	sti->func = subproc_waiter_service;
	sti->cookie = (void*)(intptr_t)pid;
	sti->fd = fd_ptmx;

	if (adb_thread_create( &t, service_bootstrap_func, sti)) {
		free(sti);
		adb_close(fd_ptmx);
		D("cannot create service thread\n");
		return -1;
	}
	D("service thread started, fd=%d pid=%d\n", fd_ptmx, pid);

	if (log_file)
		fd_log_file = open(log_file, O_CREAT|O_APPEND|O_RDWR, 0777);
	adb_write(fd_log_file, "mini_shell init\n", strlen("mini_shell init\n")); 

	return 0;
}

void exit_mini_shell()
{
	D("%s: ready to exit\n", __FUNCTION__);
	if (fd_ptmx >= 0) {
		adb_write(fd_ptmx, "exit\n", strlen("exit\n"));
		usleep(500000);
		adb_close(fd_ptmx);
		fd_ptmx = -1;
	}

	if (fd_log_file >= 0) {
		adb_close(fd_log_file);
		fd_log_file = -1;
	}

	if (stdin_copy >= 0) {
		dup2(stdin_copy, STDIN_FILENO);
		stdin_copy = -1;
	}
	if (stdout_copy >= 0) {
		dup2(stdout_copy, STDOUT_FILENO);
		stdout_copy = -1;
	}
	if (stderr_copy >= 0) {
		dup2(stderr_copy, STDERR_FILENO);
		stderr_copy = -1;
	}
	D("%s: exits\n", __FUNCTION__);
}

int mini_shell_exec(const char *command, char *reply)
{
	if (fd_ptmx >= 0 && command) {
		adb_write(fd_ptmx, command, strlen(command));
		usleep(500000);  // wait for pts write

		memset(cmd_reply, 0, sizeof(cmd_reply));
		adb_read(fd_ptmx, cmd_reply, sizeof(cmd_reply));
		D("%s: %s\n", __FUNCTION__, cmd_reply);
		if (reply)
			strncpy(reply, cmd_reply, sizeof(cmd_reply));

		if (fd_log_file >= 0) {
			D("%s: %s\n", __FUNCTION__, reply);
			adb_write(fd_log_file, cmd_reply, sizeof(cmd_reply));
		}
		return 0;
	}

	return -1;
}

int mini_shell_stdin(char *inputs, int len)
{
	if (stdin_copy >= 0) {
		memset(inputs, 0, len);
		return adb_read(stdin_copy, inputs, len);
	}
	return -1;
}

int mini_shell_stdout(const char *outputs)
{
	if (stdout_copy >= 0)
		return adb_write(stdout_copy, outputs, strlen(outputs));
	printf("%s\n", outputs);
	return -1;
}

int mini_shell_stderr(const char *error)
{
	if (stderr_copy >= 0)
		return adb_write(stderr_copy, error, strlen(error));
	printf("%s\n", error);
	return -1;
}


