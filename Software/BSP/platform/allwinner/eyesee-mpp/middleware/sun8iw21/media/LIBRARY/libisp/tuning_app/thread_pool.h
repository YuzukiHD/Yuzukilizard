#ifndef _THREAD_POOL_H_V100_
#define _THREAD_POOL_H_V100_

/*
 * thread pool
 * author: clarkyy
 * date: 20161118
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include <errno.h>

#define msleep(t)            usleep((t) << 10)


typedef void *(*thread_work_func)(void *);

/*
 * init thread pool
 * returns 0 if OK, <0 if something went wrong
 */
int init_thread_pool();

/*
 * add a work to thread queue
 * some work thread will execute the work function with work function params
 * work_func: real work function
 * work_func_params: params for real work function, it should be malloc and free manually
 * returns 0 if OK, <0 if something went wrong
 */
int add_work(thread_work_func work_func, void *work_func_params);

/*
 * exit thread pool
 * returns 0 if OK, <0 if something went wrong
 */
int exit_thread_pool();


#endif /* _THREAD_POOL_H_V100_ */
