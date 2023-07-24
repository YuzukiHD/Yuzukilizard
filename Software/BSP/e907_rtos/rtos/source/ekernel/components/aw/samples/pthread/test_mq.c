#include <mqueue.h>
#include <stdio.h>
#include <pthread.h>

mqd_t mqdes;

static void *thread2(void *arg)
{
    pthread_t tid;
    tid = pthread_self();

    printf("I am thread2, tid=%lu\n", tid);

    char buf_send[] = "hello";
    if (mq_send(mqdes, buf_send, sizeof(buf_send), 1) < 0)
    {
        printf("send error\n");
    }
    printf("thread 2 exit --------------\n");
    return NULL;
}

static void *thread1(void *arg)
{
    pthread_t tid;
    tid = pthread_self();

    sleep(1);
    printf("I am thread1, tid=%lu\n", tid);

    char buf_rec[50];
    if (mq_receive(mqdes, buf_rec, sizeof(buf_rec), NULL) < 0)
    {
        printf("rec error\n");
    }
    printf("buf_rec=%s\n", buf_rec);
    printf("thread 1 exit --------------\n");
    return NULL;
}

static int mq(int argc, char **argv)
{
    pthread_t tid1, tid2;
    int ret;
    struct mq_attr attr;
    struct mq_attr attr1;

    attr.mq_maxmsg = 5;
    attr.mq_msgsize = 10;

    if ((mqdes = mq_open("/mqueue1", O_RDWR | O_CREAT, 0666, &attr)) == NULL)
    {
        printf("mq_open create new mqueue failed because attr.mq_msgsize not specified.\n");
    }

    ret = pthread_create(&tid1, NULL, (void *)thread1, NULL);
    if (ret != 0)
    {
        printf("Create pthread 1 error!\n");
    }
    printf("Create pthread 1 success!\n");

    ret = pthread_create(&tid2, NULL, (void *)thread2, NULL);
    if (ret != 0)
    {
        printf("Create pthread 2 error!\n");
    }
    printf("Create pthread 2 success!\n");

    mq_getattr(mqdes, &attr1);
    printf("%ld %ld\n", attr1.mq_maxmsg, attr1.mq_msgsize);

    sleep(5);
    mq_close(mqdes);
    mq_unlink("/mqueue1");

    printf("join tid1\n");
    pthread_join(tid1, NULL);
    printf("join tid2\n");
    pthread_join(tid2, NULL);
    printf("main exit --------------\n");

    return 0;
}

FINSH_FUNCTION_EXPORT_ALIAS(mq, __cmd_mqtest, mq test command);
