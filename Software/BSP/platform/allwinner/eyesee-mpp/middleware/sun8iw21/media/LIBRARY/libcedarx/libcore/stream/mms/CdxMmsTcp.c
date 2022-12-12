/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxMmsTcp.c
 * Description : MmsTcp
 * History :
 *
 */

#include "CdxMmsStream.h"
#include <stdlib.h>
#include <string.h>

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>

//***************************************************************************//
//***************************************************************************//
// Connect to a server using a TCP connection, with specified address family
// return -2 for fatal error, like unable to resolve name, connection timeout...
// return -1 is unable to connect to a particular port
// verb present whether the host need gethostbyname
static int connect2Server_with_af(aw_mms_inf_t* mmsStreamInf, char *host, int port,
                       int af, int verb)
{
    int             sockServerFd;
    int             ret;
    int             err;
    int             sockRecvLen;
    socklen_t       err_len;
    int             count = 0;
    fd_set          set;
    struct timeval  tmpTv;
    union {
        struct sockaddr_in addrfour;
    }serverAddr;

    size_t          server_address_size;
    void*           our_s_addr; //     Pointer to sin_addr or sin6_addr
    struct hostent* hp = NULL;

    struct timeval to;

    sockRecvLen = 0;
    sockServerFd = CdxAsynSocket(AF_INET, &sockRecvLen);
    if(sockServerFd == 1)
    {
        CDX_LOGW("+++++++ fd(%d), maybe error", sockServerFd);
    }
    mmsStreamInf->sockFd = sockServerFd;
    if(sockServerFd==-1)
    {
        CDX_LOGE("io err errno(%d)", errno);
        return -1;
    }

    to.tv_sec = 10;
    to.tv_usec = 0;

    setsockopt(sockServerFd, SOL_SOCKET, SO_RCVTIMEO, &to, sizeof(to));
    setsockopt(sockServerFd, SOL_SOCKET, SO_SNDTIMEO, &to, sizeof(to));

    switch (af)
    {
        case AF_INET:
            our_s_addr = (void *) &serverAddr.addrfour.sin_addr;
            break;

        default:
            CDX_LOGE("unexpect af...");
            return -1;
    }
    memset(&serverAddr, 0, sizeof(serverAddr));

    if (inet_aton(host, our_s_addr)!=1)
    {

        hp=(struct hostent*)gethostbyname( host );

        if( hp==NULL )
        {
            if(verb)
            {
                CDX_LOGE("io err errno(%d)", errno);
                return -1;
            }
        }
        memcpy( our_s_addr, (void*)hp->h_addr_list[0], hp->h_length );
    }

    switch (af)
    {
        case AF_INET:
            serverAddr.addrfour.sin_family=af;
            serverAddr.addrfour.sin_port=htons(port);
            server_address_size = sizeof(serverAddr.addrfour);
            break;

        default:
            CDX_LOGE("unexpect af...");
            return -1;
    }

    // Turn the socket as non blocking so we can timeout on the connection
    fcntl( sockServerFd, F_SETFL, fcntl(sockServerFd, F_GETFL) | O_NONBLOCK);

    if(connect(sockServerFd, (struct sockaddr*)&serverAddr, server_address_size)==-1 )
    {

        CDX_LOGV("connect, errno(%d)", errno);
        if(errno != EINPROGRESS )
        {
            closesocket(sockServerFd);
            CDX_LOGE("io err errno(%d)", errno);
            return -1;
        }
    }
    tmpTv.tv_sec = 0;
    tmpTv.tv_usec = 10000;
    FD_ZERO(&set);
    FD_SET(sockServerFd, &set);

    // When the connection will be made, we will have a writeable fd
    while((ret = select(sockServerFd+1, NULL, &set, NULL, &tmpTv)) == 0)
    {
        if(mmsStreamInf->exitFlag)
        {
            return -1;
        }
        CDX_LOGV(" select 0");
        if(count > 30 )
        {
            if(count > 30)
            {
                CDX_LOGE("*****************ConnTimeout");
            }
            else
            {
                CDX_LOGE("***************Connection interrupted by user");
            }
            return -1;
        }

        count++;
        FD_ZERO( &set );
        FD_SET( sockServerFd, &set );
        tmpTv.tv_sec = 0;
        tmpTv.tv_usec = 100000;
    }

    if(ret < 0)
        CDX_LOGE(" select error\n");

    // Check if there were any errors
    err_len = sizeof(int);
    ret = getsockopt(sockServerFd,SOL_SOCKET,SO_ERROR,&err,&err_len);
    if(ret < 0)
    {
        CDX_LOGE("io err errno(%d)", errno);
        return -1;
    }

    if(err > 0)
    {
        CDX_LOGE("io err errno(%d)", errno);
        return -1;
    }

    return sockServerFd;
}

// Connect to a server using a TCP connection
// return -2 for fatal error, like unable to resolve name, connection timeout...
// return -1 is unable to connect to a particular port
// if success , return sockServerFd
int Connect2Server(aw_mms_inf_t* mmsStreamInf,char *host, int  port, int verb)
{

    return connect2Server_with_af(mmsStreamInf,host, port, AF_INET,verb);
}
