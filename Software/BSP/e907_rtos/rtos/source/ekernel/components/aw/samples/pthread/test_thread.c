#include <stdio.h>
#include <pthread.h>

static int count = 888;

static pthread_mutex_t mutex;
static pthread_cond_t cond;
static pthread_spinlock_t spin;
static pthread_rwlock_t rwlock;
static pthread_barrier_t barrier;

pthread_once_t once = PTHREAD_ONCE_INIT;

static void once_run(void)
{
    printf("once_run in thread:%ld\n", pthread_self() - 1);
}

static void *thread1(void *arg)
{
    pthread_t tid;
    tid = pthread_self();
    pthread_once(&once, once_run);

    printf("I am thread1, tid=%lu, arg=%d\n", tid, *(int *)arg);
    pthread_barrier_wait(&barrier);

    pthread_mutex_lock(&mutex);
    while (count == 888)
    {
        printf("cond wait\n");
        pthread_cond_wait(&cond, &mutex);
    }
    count++;
    pthread_mutex_unlock(&mutex);

    sleep(1);
    printf("exit thread1 ------------\n");
    return NULL;
}

static void *thread2(void *arg)
{
    pthread_t tid;
    tid = pthread_self();
    pthread_once(&once, once_run);

    printf("I am thread2, tid=%lu, arg=%s\n", tid, (char *)arg);
    pthread_barrier_wait(&barrier);

    pthread_mutex_lock(&mutex);
    if (count == 888)
    {
        count--;
    }
    pthread_mutex_unlock(&mutex);
    pthread_cond_signal(&cond);

    sleep(1);

    printf("exit thread2 ------------\n");
    pthread_exit(NULL);
    return NULL;
}

static void *thread3(void *arg)
{
    int i;
    pthread_t tid;
    tid = pthread_self();

    for (i = 0; i < 5; i++)
    {
        printf("I am thread3, tid=%lu, i=%d\n", tid, i);
        pthread_spin_lock(&spin);
        pthread_rwlock_wrlock(&rwlock);
        count--;
        pthread_rwlock_unlock(&rwlock);
        pthread_spin_unlock(&spin);
    }
    printf("exit thread3 ------------\n");
    return NULL;
}


static void *thread4(void *arg)
{
    int i;
    pthread_t tid;
    tid = pthread_self();

    for (i = 0; i < 5; i++)
    {
        printf("I am thread4, tid=%lu, i=%d\n", tid, i);
        pthread_spin_lock(&spin);
        pthread_rwlock_rdlock(&rwlock);
        count++;
        pthread_rwlock_unlock(&rwlock);
        pthread_spin_unlock(&spin);
    }
    sleep(2);
    printf("exit thread4 ------------\n");
    return NULL;
}

static int thread(int argc, char **argv)
{
    pthread_t tid1, tid2, tid3, tid4;
    int ret, state, policy, t1arg = 666;
    char *t2arg = "thread2 arg";
    pthread_attr_t attr;
    struct sched_param param;
    size_t stack_size;

    pthread_attr_init(&attr);
    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&cond, NULL);
    pthread_spin_init(&spin, 0);
    pthread_rwlock_init(&rwlock, NULL);
    pthread_barrier_init(&barrier, NULL, 3);

    ret = pthread_create(&tid1, &attr, (void *)thread1, &t1arg);
    if (ret != 0)
    {
        printf("Create pthread 1 error!\n");
    }
    printf("Create pthread 1 success!\n");
    pthread_detach(tid1);

    ret = pthread_create(&tid2, NULL, (void *)thread2, t2arg);
    if (ret != 0)
    {
        printf("Create pthread 2 error!\n");
    }
    printf("Create pthread 2 success!\n");

    ret = pthread_create(&tid3, NULL, (void *)thread3, NULL);
    if (ret != 0)
    {
        printf("Create pthread 3 error!\n");
    }
    printf("Create pthread 3 success!\n");


    pthread_attr_getschedparam(&attr, &param);
    printf("before setting, sched_priority is %d\n", param.sched_priority);

    param.sched_priority = 13;
    pthread_attr_setschedparam(&attr, &param);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    ret = pthread_create(&tid4, &attr, (void *)thread4, NULL);
    if (ret != 0)
    {
        printf("Create pthread 4 error!\n");
    }
    printf("Create pthread 4 success!\n");


    pthread_attr_getschedparam(&attr, &param);
    printf("after setting, sched_priority is %d\n", param.sched_priority);

    pthread_attr_getschedpolicy(&attr, &policy);
    if (policy == SCHED_FIFO)
    {
        printf("sched policy is %d\n", policy);
    }

    pthread_attr_getstacksize(&attr, &stack_size);
    printf("stack_size is = %u\n", stack_size);

    pthread_attr_getdetachstate(&attr, &state);
    if (state == PTHREAD_CREATE_DETACHED)
    {
        printf("detach state is %d\n", state);
    }


    if (!(pthread_equal(tid1, tid2)))
    {
        printf("tid1 != tid2\n");
    }

    sleep(2);
    pthread_barrier_wait(&barrier);

    ret = pthread_join(tid3, NULL);
    printf("tid3 join ret = %d\n", ret);
    ret = pthread_join(tid2, NULL);
    printf("tid2 join ret = %d\n", ret);


    sleep(3);
    pthread_attr_destroy(&attr);
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&cond);
    pthread_spin_destroy(&spin);
    pthread_rwlock_destroy(&rwlock);
    pthread_barrier_destroy(&barrier);

    printf("count = %d\n", count);
    printf("exit main------\n");

    return 0;
}

FINSH_FUNCTION_EXPORT_ALIAS(thread, __cmd_threadtest, thread test command);
