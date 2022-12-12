/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxSocketUtil.c
 * Description : SocketUtil
 * History :
 *
 */

#include <CdxSocketUtil.h>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <strings.h>
#include <netdb.h>
#include <netinet/tcp.h>

#include <cdx_log.h>
#include <CdxMemory.h>

#define CONFIG_KEEP_ALIVE (1)

#if defined(CONF_CMCC)
#define TCP_KEEP_IDLE_SECS      (1) // cmcc do not need 1 min
#else
#define TCP_KEEP_IDLE_SECS      (10) // 如该连接在60秒内没有任何数据往来,则进行探测
#endif

#define TCP_KEEP_INTERVAL_SECS  (5)  // 探测时发包的时间间隔为5 秒
#define TCP_KEEP_COUNT          (3)  // 探测尝试的次数.如果第1次探测包就收到响应了,则后2次的不再发.

cdx_err CdxMakeSocketBlocking(cdx_int32 s, cdx_bool blocking)
{
    // Make socket non-blocking.
    int flags = fcntl(s, F_GETFL, 0);

    if (flags == -1) {
        return errno;
    }

    if (blocking) {
        flags &= ~O_NONBLOCK;
    }
    else {
        flags |= O_NONBLOCK;
    }

    flags = fcntl(s, F_SETFL, flags);

    return flags == -1 ? errno : CDX_SUCCESS;
}

static void CdxSocketRecvBufSize(cdx_int32 s, int size)
{
    int ret;
    ret = setsockopt(s, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size));
    CDX_FORCE_CHECK(ret == 0);
    return ;
}

void CdxSocketMakePortPair(cdx_int32 *rtpSocket, cdx_int32 *rtcpSocket,
                                cdx_uint32 *rtpPort)
{
    cdx_uint32 start, port;
    const cdx_int32 recvBufSize = 256 *1024;

    *rtpSocket = socket(AF_INET, SOCK_DGRAM, 0);
    CDX_FORCE_CHECK(*rtpSocket > 0);

    CdxSocketRecvBufSize(*rtpSocket, recvBufSize);

    *rtcpSocket = socket(AF_INET, SOCK_DGRAM, 0);
    CDX_FORCE_CHECK(*rtcpSocket > 0);

    CdxSocketRecvBufSize(*rtcpSocket, recvBufSize);

    start = (rand() * 1000)/ RAND_MAX + 15550;
    start &= ~1;

    struct sockaddr_in tmpAddr;
    int ret = 0;
    for (port = start; port < 65536; port += 2)
    {
        memset(tmpAddr.sin_zero, 0x00, sizeof(tmpAddr.sin_zero));
        tmpAddr.sin_port = htons(port);
        tmpAddr.sin_family = AF_INET;
        tmpAddr.sin_addr.s_addr = htonl(INADDR_ANY);

        ret = bind(*rtpSocket, (const struct sockaddr *)&tmpAddr, sizeof(tmpAddr));
        if (ret < 0)
        {
            continue;
        }

        tmpAddr.sin_port = htons(port + 1);

        ret = bind(*rtcpSocket, (const struct sockaddr *)&tmpAddr, sizeof(tmpAddr));
        if ( ret == 0)
        {
            *rtpPort = port;
            return;
        }
    }

    CDX_TRESPASS();
    return ;
}
cdx_int32 CdxSockAddrConstruct(struct sockaddr_in *dest, char *ip, cdx_int32 port)
{
    struct hostent* hp = NULL;

    bzero(dest, sizeof(*dest));
    dest->sin_family = AF_INET;
    dest->sin_port = htons(port);

    hp = (struct hostent *)gethostbyname(ip);//-block?

    if(!hp)
    {
        CDX_LOGE("get host fail.");
        return -1;
    }

    memcpy(&dest->sin_addr.s_addr, hp->h_addr_list[0], hp->h_length);

    return 0;
}
cdx_int32 CdxSockSetBlocking(cdx_int32 sockfd, cdx_int32 blocking)
{
    // Make socket non-blocking.
    cdx_int32 flags = fcntl(sockfd, F_GETFL, 0);

    if (flags == -1)
    {
        return errno;
    }

    if (blocking)
    {
        flags &= ~O_NONBLOCK;
    }
    else
    {
        flags |= O_NONBLOCK;
    }

    flags = fcntl(sockfd, F_SETFL, flags);

    return flags == -1 ? errno : 0;
}

cdx_int32 CdxSockIsBlocking(cdx_int32 sockfd)
{
    cdx_int32 flags = fcntl(sockfd, F_GETFL, 0);
    return !(flags & O_NONBLOCK);
}
//nRecvBufLen: socket recv buf len to set. 0: use default.
cdx_int32 CdxAsynSocket(int domain, cdx_int32 *nRecvBufLen)
{
    cdx_int32 sockfd;
    cdx_int32 ret;
#if CONFIG_KEEP_ALIVE
    cdx_int32 keepalive = 1;                            // 开启keepalive属性
    cdx_int32 keepidle = TCP_KEEP_IDLE_SECS;            // 如该连接在60秒内没有任何数据往来,则进行探测
    cdx_int32 keepinterval = TCP_KEEP_INTERVAL_SECS;    // 探测时发包的时间间隔为5 秒
    // 探测尝试的次数.如果第1次探测包就收到响应了,则后2次的不再发.
    cdx_int32 keepcount = TCP_KEEP_COUNT;
#endif

    logv("address family[%d] = %s", domain, domain == AF_INET ? "AF_INET" :
                                    (domain == AF_INET6 ? "AF_INET6":"others"));

    sockfd = socket(domain, SOCK_STREAM, 0);
    if(sockfd < 0)
    {
        CDX_LOGE("errno(%d)", errno);
        return -1;
    }

    ret = CdxSockSetBlocking(sockfd, 0);
    CDX_FORCE_CHECK(ret == 0);

    if (nRecvBufLen != NULL)
    {
        if(*nRecvBufLen > 0)
        {
            ret = setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF,
                    nRecvBufLen, sizeof(*nRecvBufLen));
            CDX_FORCE_CHECK(ret == 0);
        }
        ret = getsockopt(sockfd, SOL_SOCKET, SO_RCVBUF,
                nRecvBufLen, &(socklen_t){sizeof(*nRecvBufLen)});
        //CDX_FORCE_CHECK(ret == 0);
        //CDX_LOGV("sock recv buf len set to: %d, return %d", nRecvBufLen, *nRecvBufLenRet);
    }

#if CONFIG_KEEP_ALIVE
    setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, (void *)&keepalive , sizeof(keepalive));
    setsockopt(sockfd, SOL_TCP, TCP_KEEPIDLE, (void*)&keepidle , sizeof(keepidle));
    setsockopt(sockfd, SOL_TCP, TCP_KEEPINTVL, (void *)&keepinterval , sizeof(keepinterval));
    setsockopt(sockfd, SOL_TCP, TCP_KEEPCNT, (void *)&keepcount , sizeof(keepcount));
#endif

    return sockfd;
}

void CdxSockDisableTcpKeepalive(cdx_int32 sockfd)
{
    int optval;
    socklen_t optlen = sizeof(int);

    if(getsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &optval, &optlen) < 0)
    {
      logd("getsockopt failed. errno=%d", errno);
    }
    //logd("SO_KEEPALIVE is %s, optval=%d, optlen=%d", (optval ? "ON" : "OFF"), optval, optlen);

    optval = 0;
    optlen = sizeof(optval);
    if(setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &optval, optlen) < 0)
    {
      logd("setsockopt failed.");
    }

    if(getsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &optval, &optlen) < 0)
    {
      logd("getsockopt failed.");
    }

    return;
}

cdx_int32  CdxSockAsynConnect(cdx_int32 sockfd, const struct sockaddr *addr, socklen_t addrlen,
                        cdx_long timeoutUs, cdx_int32 *pForceStop)
{
    cdx_int32 ret, ioErr;
    fd_set ws;
    struct timeval tv;
    cdx_long loopTimes = 0, i = 0;

    if (timeoutUs == 0)
    {
        loopTimes = ((cdx_ulong)(-1L))>> 1;
    }
    else
    {
        loopTimes = timeoutUs/CDX_SELECT_TIMEOUT;
    }

    ret = connect(sockfd, (struct sockaddr *)addr, addrlen);
    if (ret == 0)
    {
        //success
        return 0;
    }

    if (errno != EINPROGRESS)
    {
        CDX_LOGE("<%s,%d>select err(%d)", __FUNCTION__, __LINE__, errno);
        return -1;
    }

    for (i = 0; i < loopTimes; i++)
    {
        if (pForceStop && *pForceStop)
        {
            CDX_LOGE("<%s,%d>force stop", __FUNCTION__, __LINE__);
            return -2;
        }

        FD_ZERO(&ws);
        FD_SET(sockfd, &ws);
        tv.tv_sec = 0;
        tv.tv_usec = CDX_SELECT_TIMEOUT;
        ret = select(sockfd + 1, NULL, &ws, NULL, &tv);
        if (ret > 0)
        {
            int nError = 0;
            socklen_t nLen = sizeof(nError);
            ret = getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &nError, &nLen);
            // if error happened, Solaris's getsockopt return -1,
            // and set pending error to errno. Berkeley's getsockopt return 0,
            // and set pending error to nError.
            if(ret < 0 || nError)
            {
                if(nError)
                {
                    errno = nError;
                }
                CDX_LOGE("connect err(%d)", errno);
                return -1;
            }
            // connect success
            return 0;
        }
        else if (ret == 0)
        {
            continue;
        }
        else
        {
            ioErr = errno;
            if (EINTR == ioErr)
            {
                continue;
            }
            CDX_LOGE("<%s,%d>select err(%d)", __FUNCTION__, __LINE__, errno);
            return -1;
        }
    }
    //time out
    return -1;
}

cdx_ssize CdxSockAsynSend(cdx_int32 sockfd, const void *buf, cdx_size len,
                        cdx_long timeoutUs, cdx_int32 *pForceStop)
{
    fd_set ws;
    fd_set errs;
    struct timeval tv;
    cdx_ssize ret = 0, sendSize = 0;
    cdx_long loopTimes = 0, i = 0;
    cdx_int32 ioErr;

    if (timeoutUs == 0)
    {
        loopTimes = ((unsigned long)(-1L))>> 1;
    }
    else
    {
        loopTimes = timeoutUs/CDX_SELECT_TIMEOUT;
    }

    for (i = 0; i < loopTimes; i++)
    {
        if (pForceStop && *pForceStop)
        {
            CDX_LOGE("<%s,%d>force stop", __FUNCTION__, __LINE__);
            return sendSize>0 ? sendSize : -2;
        }

        FD_ZERO(&ws);
        FD_SET(sockfd, &ws);
        FD_ZERO(&errs);
        FD_SET(sockfd, &errs);
        tv.tv_sec = 0;
        tv.tv_usec = CDX_SELECT_TIMEOUT;
        ret = select(sockfd + 1, NULL, &ws, &errs, &tv);
        if (ret < 0)
        {
            ioErr = errno;
            if (EINTR == ioErr)
            {
                continue;
            }
            CDX_LOGE("<%s,%d>select err(%d)", __FUNCTION__, __LINE__, errno);
            return -1;
        }
        else if (ret == 0)
        {
            //("timeout\n");
            continue;
        }

        while (1)
        {
            if (pForceStop && *pForceStop)
            {
                CDX_LOGE("<%s,%d>force stop", __FUNCTION__, __LINE__);
                return sendSize>0 ? sendSize : -2;
            }

            if(FD_ISSET(sockfd, &errs))
            {
                CDX_LOGE("<%s,%d>errs ", __FUNCTION__, __LINE__);
                break;
            }
            if(!FD_ISSET(sockfd, &ws))
            {
                CDX_LOGW("select > 0, but sockfd is not ready?");
                break;
            }

            ret = send(sockfd, ((char *)buf) + sendSize, len - sendSize, 0);
            if (ret < 0)
            {
                if (EAGAIN == errno)
                {
                    //buffer not ready
                    break;
                }
                else
                {
                    CDX_LOGE("<%s,%d>send err(%d)", __FUNCTION__, __LINE__, errno);
                    return -1;
                }
            }
            else if (ret == 0)
            {
                //buffer is full?
                break;
            }
            else // ret > 0
            {
                sendSize += ret;
                if ((cdx_size)sendSize == len)
                {
                    return sendSize;
                }
            }
        }

    }

    return sendSize;
}
//forcestop: return -2
//error: return -1
cdx_ssize CdxSockAsynRecv(cdx_int32 sockfd, void *buf, cdx_size len,
                        cdx_long timeoutUs, cdx_int32 *pForceStop)
{
    fd_set rs;
    fd_set errs;
    struct timeval tv;
    cdx_ssize ret = 0, recvSize = 0;
    cdx_long loopTimes = 0, i = 0;
    cdx_int32 ioErr;
//    cdx_int32 num = 0;

    if (CdxSockIsBlocking(sockfd))
    {
        CDX_LOGE("<%s,%d>err, blocking socket", __FUNCTION__, __LINE__);
        return -1;
    }

    if (timeoutUs == 0)
    {
        loopTimes = ((unsigned long)(-1L))>> 1;
    }
    else
    {
        loopTimes = timeoutUs/CDX_SELECT_TIMEOUT;
    }

    for (i = 0; i < loopTimes; i++)
    {
        if (pForceStop && *pForceStop)
        {
            CDX_LOGE("<%s,%d>force stop", __FUNCTION__, __LINE__);
            return recvSize>0 ? recvSize : -2;
        }

        FD_ZERO(&rs);
        FD_SET(sockfd, &rs);
        FD_ZERO(&errs);
        FD_SET(sockfd, &errs);
        tv.tv_sec = 0;
        tv.tv_usec = CDX_SELECT_TIMEOUT;
        ret = select(sockfd + 1, &rs, NULL, &errs, &tv);
        if (ret < 0)
        {
            ioErr = errno;
            if (EINTR == ioErr)
            {
                continue;
            }
            CDX_LOGE("<%s,%d>select err(%d)", __FUNCTION__, __LINE__, ioErr);
            return -1;
        }
        else if (ret == 0)
        {
            //("timeout\n");
            //CDX_LOGV("xxx timeout, select again...");
            continue;
        }

        while (1)
        {
            if (pForceStop && *pForceStop)
            {
                CDX_LOGE("<%s,%d>force stop.recvSize(%ld)", __FUNCTION__, __LINE__, recvSize);
                return recvSize>0 ? recvSize : -2;
            }
            if(FD_ISSET(sockfd,&errs))
            {
                CDX_LOGE("<%s,%d>errs ", __FUNCTION__, __LINE__);
                break;
            }
            if(!FD_ISSET(sockfd, &rs))
            {
                CDX_LOGV("select > 0, but sockfd is not ready?");
                break;
            }

            ret = recv(sockfd, ((char *)buf) + recvSize, len - recvSize, 0);
            if (ret < 0)
            {
                ioErr = errno;
                if (EAGAIN == ioErr)
                {
                    break;
                }
                else
                {
                    CDX_LOGE("<%s,%d>recv err(%d)", __FUNCTION__, __LINE__, errno);
                    return -1;
                }
            }
            else if (ret == 0)//socket is close by peer?
            {
                CDX_LOGD("xxx recvSize(%ld),sockfd(%d), want to read(%lu),"
                        "errno(%d), socket is closed by peer?",
                                        recvSize, sockfd, len, errno);
                return recvSize;
            }
            else // ret > 0
            {
                recvSize += ret;
                if ((cdx_size)recvSize == len)
                {
                    return recvSize;
                }
            }
        }

    }

    return recvSize;
}
cdx_ssize CdxSockNoblockRecv(cdx_int32 sockfd, void *buf, cdx_size len)
{
    return recv(sockfd, buf, len, 0);
}

