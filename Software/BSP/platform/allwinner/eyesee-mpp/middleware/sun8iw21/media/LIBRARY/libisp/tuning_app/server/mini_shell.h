/*
 * mini shell, pseudo terminal
 * refers to adb(android debug bridge)
 * author: clarkyy
 * date:   20160803
 * http://www.allwinnertech.com
 */

#ifndef _AW_MINI_SHELL_H_V100_
#define _AW_MINI_SHELL_H_V100_

#ifndef _GNU_SOURCE
#define _GNU_SOURCE   // for O_CLOEXEC, must define before #include <fcntl.h>
#endif

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdint.h>  /* for int64_t */


extern int adb_open(const char* pathname, int options);
extern int adb_read(int fd, void* buf, size_t len);
extern int adb_write(int fd, const void* buf, size_t len);
extern int adb_close(int fd);


/*
 * init mini shell
 * mini shell will use stdin/stdout/stderr for shell interactive,
 * so DON'T USE stdin/stdout/stderr during using mini shell and 
 * use mini_shell_stdin/mini_shell_stdout/mini_shell_stderr instead
 * log_file: log file to save shell
 * returns 0 if OK, others if something went wrong
 */
extern int init_mini_shell(const char *log_file);
/*
 * execute a shell command
 * command: shell command
 * reply: shell reply, its length should be 1024 at least
 * returns 0 if OK, others if something went wrong
 */
extern int mini_shell_exec(const char *command, char *reply);
/*
 * exit mini shell
 * mini shell will recovery stdin/stdout/stderr after it exits
 */
extern void exit_mini_shell();
/*
 * mini shell stdin
 * inputs: stdin inputs
 * len: max length of inputs
 * returns length of inputs read from stdin, negative if something went wrong
 */
extern int mini_shell_stdin(char *inputs, int len);
/*
 * mini shell stdout
 * outputs: stdout outputs
 * returns length of outputs write to stdout, negative if something went wrong
 */
extern int mini_shell_stdout(const char *outputs);
/*
 * mini shell stderr
 * error: stderr outputs
 * returns length of error write to stderr, negative if something went wrong
 */
extern int mini_shell_stderr(const char *error);


#endif  /* _AW_MINI_SHELL_H_V100_ */