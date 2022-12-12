/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : consumer.h
 * Description : library to read pipe
 * History :
 *
 */
#ifndef _CONSUMER_H_
#define _CONSUMER_H_
#include<sys/types.h>
#include<sys/stat.h>
#include<unistd.h>
#include<fcntl.h>
#include<cdx_log.h>

#define DTMB "/data/camera/dtmb"


//#define USE_SOCKET
#define DTMB_SERVER_SOCKET "/data/camera/server_socket"
#define DTMB_CLIENT_SOCKET "/data/camera/client_socket"


#ifdef __cplusplus
extern "C"
{
#endif

int fifoInit();

int fifoRead(int fd,char *buf,int size);

int fifoReadSync(int fd,char *buf,int size);


int fifoWrite(int fd,const void *buf,int size);

int fifoClose(int fd);

#ifdef __cplusplus
}
#endif

#endif


