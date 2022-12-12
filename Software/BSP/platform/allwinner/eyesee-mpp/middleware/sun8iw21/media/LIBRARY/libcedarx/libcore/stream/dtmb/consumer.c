/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : consumer.c
 * Description : library to read pipe
 * History :
 *
 */

#include"consumer.h"

int fifoInit()
{
    int fifo_id;

    fifo_id =open( DTMB ,O_RDWR);

    return fifo_id;
}

int fifoRead(int fd,char *buf,int size)
{
    return read(fd,buf,size);
}

int fifoReadSync(int fd,char *buf,int size)
{
    int totalSize = size;
    int p = 0;
    int readSize = 0;

    while(totalSize != 0)
    {
        readSize = read(fd,buf+p,totalSize);
        totalSize-=readSize;
        p+=readSize;
    }
    return size;
}

int fifoWrite(int fd,const void *buf,int size)
{
    if(size != write(fd,buf,size))
        return -1;
    else
        return 0;
}

int fifoClose(int fd)
{
    return close(fd);
}
