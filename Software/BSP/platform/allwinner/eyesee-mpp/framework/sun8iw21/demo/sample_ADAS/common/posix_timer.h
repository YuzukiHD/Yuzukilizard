#ifndef __POSIX_TIMER_H__
#define __POSIX_TIMER_H__

#include <time.h>
#include <signal.h>

#ifdef __cplusplus
extern "C" {
#endif

int create_timer(void *data, timer_t *timerid, void (*pFunc)(union sigval));
int set_one_shot_timer(time_t sec, long nsec, timer_t timerid);
int set_period_timer(time_t sec, long nsec, timer_t timerid);
int get_expiration_time(time_t *sec, long *nsec, timer_t timerid);
int stop_timer(timer_t timerid);
int delete_timer(timer_t timerid);

#ifdef __cplusplus
}  /* end of extern "C" */
#endif

#endif
