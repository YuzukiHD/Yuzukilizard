#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>

static sem_t *sem2, sem3, sem4;

static void *thread2(void *arg)
{
    int ret;
    struct timespec ts;
    ts.tv_sec = 10;
    ts.tv_nsec = 0;
    //if (sem_timedwait(sem2, &ts) != 0)
    if (sem_trywait(sem2) != 0)
    {
        printf("time out\n");
    }
    else
    {
        printf("exit thread2 ------------\n");
    }
    return NULL;
}

static void *thread3(void *arg)
{
    sem_wait(&sem3);
    int i;
    pthread_t tid;
    tid = pthread_self();

    for (i = 0; i < 5; i++)
    {
        printf("I am thread3, tid=%lu, i=%d\n", tid, i);
    }
    sem_post(&sem4);
    printf("exit thread3 ------------\n");
    return NULL;
}


static void *thread4(void *arg)
{
    sem_wait(&sem4);
    int i;
    pthread_t tid;
    tid = pthread_self();

    for (i = 0; i < 5; i++)
    {
        printf("I am thread4, tid=%lu, i=%d\n", tid, i);
    }
    sem_post(&sem3);
    printf("exit thread4 ------------\n");
    return NULL;
}

static int sem(int argc, char **argv)
{
    pthread_t tid2, tid3, tid4;
    int ret;

    sem2 = sem_open("test", O_RDWR | O_CREAT, 0666, 0);
    //if (sem2 == SEM_FAILED)
    if (sem2 == NULL)
    {
        printf("sem open failed\n");
    }
    else
    {
        printf("sem open success\n");
    }


    ret = pthread_create(&tid2, NULL, (void *)thread2, NULL);
    if (ret != 0)
    {
        printf("Create pthread 2 error!\n");
    }
    printf("Create pthread 2 success!\n");

    sleep(1);
    sem_post(sem2);
    if (sem_getvalue(sem2, &ret) != 0)
    {
        printf("get value failed\n");
    }
    else
    {
        printf("sem2 value=%d\n", ret);
    }
    sem_close(sem2);
    sem_unlink("test");

    ret = sem_init(&sem3, 0, 0);
    if (ret != 0)
    {
        printf("init failed\n");
    }
    sem_init(&sem4, 0, 1);

    ret = pthread_create(&tid3, NULL, (void *)thread3, NULL);
    if (ret != 0)
    {
        printf("Create pthread 3 error!\n");
    }
    printf("Create pthread 3 success!\n");

    ret = pthread_create(&tid4, NULL, (void *)thread4, NULL);
    if (ret != 0)
    {
        printf("Create pthread 4 error!\n");
    }
    printf("Create pthread 4 success!\n");

    pthread_join(tid3, NULL);
    pthread_join(tid4, NULL);

    sleep(1);
    sem_destroy(&sem3);
    sem_destroy(&sem4);
    printf("sem main exit---------------\n");

    return 0;
}
FINSH_FUNCTION_EXPORT_ALIAS(sem, __cmd_semtest, sem test command);
