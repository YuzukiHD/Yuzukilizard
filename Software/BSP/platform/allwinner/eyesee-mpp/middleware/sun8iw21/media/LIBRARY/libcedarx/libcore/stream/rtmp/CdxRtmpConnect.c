/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxRtmpConnect.c
 * Description : RtmpConnect
 * History :
 *
 */

//* open the macro definition of LOG_NDEBUG if you want the printed message of this module.
//#define LOG_NDEBUG 0
#define LOG_TAG "CdxRtmpConnect.c:"       //* prefix of the printed messages.
                                          //* include for printing message.
#include "CdxRtmpStream.h"
#include <errno.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <netinet/tcp.h>
#include <SmartDnsService.h>

/* define default endianness */
#ifndef __LITTLE_ENDIAN
#define __LITTLE_ENDIAN    3456
#endif

#ifndef __BIG_ENDIAN
#define __BIG_ENDIAN    6543
#endif

#ifndef __BYTE_ORDER
#define __BYTE_ORDER    __LITTLE_ENDIAN
#endif

/* most of the time closing a socket is just closing an fd */
static int WriteN(aw_rtmp_t *r, char *buffer, int n);
void aw_rtmp_close(aw_rtmp_t *r);
char *aw_amf_prop_encode(aw_amfobject_property_t  *prop, char *pBuffer, char *pBufEnd);
int aw_amf_prop_decode(aw_rtmp_t*r, aw_amfobject_property_t *prop, char *pBuffer, int nSize,
                     int bDecodeName);
int aw_amf3_decode(aw_rtmp_t*r, aw_amf_object_t *obj, char *pBuffer, int nSize, int bAMFData);
int aw_amf_decode(aw_rtmp_t*r,aw_amf_object_t *obj, const char *pBuffer, int nSize,
                 int bDecodeName);
void aw_amf_prop_dump(aw_amfobject_property_t *prop);
void aw_amf_prop_reset(aw_amfobject_property_t *prop);

extern void aw_amf_add_prop(aw_amf_object_t *obj, aw_amfobject_property_t *prop);

//***********************************************************************//
//**********************************************************************//
#define TRUE 1
#define FALSE 0
#define AVC(str)    {str,sizeof(str)-1}

void aw_set_rtmp_parameter(aw_rtmp_t* r)
{

    av_rtmp_param_t* p = NULL;
    char *startPtr = NULL;
    int len = 0;

    p = &(r->rtmpParam);
    startPtr = p->pBuffer;

    len = sizeof("app");
    p->av_app.pVal = startPtr;
    p->av_app.len = len-1;
    strncpy(p->av_app.pVal, "app", len);
    startPtr += len;

    len = sizeof("connect");
    p->av_connect.pVal = startPtr;
    p->av_connect.len = len-1;
    strncpy(p->av_connect.pVal, "connect", len);
    startPtr += len;

    len = sizeof("flashVer");
    p->av_flashVer.pVal = startPtr;
    p->av_flashVer.len = len-1;
    strncpy(p->av_flashVer.pVal, "flashVer", len);
    startPtr += len;

    len = sizeof("swfUrl");
    p->av_swfUrl.pVal = startPtr;
    p->av_swfUrl.len = len-1;
    strncpy(p->av_swfUrl.pVal, "swfUrl", len);
    startPtr += len;

    len = sizeof("pageUrl");
    p->av_pageUrl.pVal = startPtr;
    p->av_pageUrl.len = len-1;
    strncpy(p->av_pageUrl.pVal, "pageUrl", len);
    startPtr += len;

    len = sizeof("tcUrl");
    p->av_tcUrl.pVal = startPtr;
    p->av_tcUrl.len = len-1;
    strncpy(p->av_tcUrl.pVal, "tcUrl", len);
    startPtr += len;

    len = sizeof("fpad");
    p->av_fpad.pVal = startPtr;
    p->av_fpad.len = len-1;
    strncpy(p->av_fpad.pVal, "fpad", len);
    startPtr += len;

    len = sizeof("capabilities");
    p->av_capabilities.pVal = startPtr;
    p->av_capabilities.len = len-1;
    strncpy(p->av_capabilities.pVal, "capabilities", len);
    startPtr += len;

    len = sizeof("audioCodecs");
    p->av_audioCodecs.pVal = startPtr;
    p->av_audioCodecs.len = len-1;
    strncpy(p->av_audioCodecs.pVal, "audioCodecs", len);
    startPtr += len;

    len = sizeof("videoCodecs");
    p->av_videoCodecs.pVal = startPtr;
    p->av_videoCodecs.len = len-1;
    strncpy(p->av_videoCodecs.pVal, "videoCodecs", len);
    startPtr += len;

    len = sizeof("videoFunction");
    p->av_videoFunction.pVal = startPtr;
    p->av_videoFunction.len = len-1;
    strncpy(p->av_videoFunction.pVal, "videoFunction", len);
    startPtr += len;

    len = sizeof("objectEncoding");
    p->av_objectEncoding.pVal = startPtr;
    p->av_objectEncoding.len = len-1;
    strncpy(p->av_objectEncoding.pVal, "objectEncoding", len);
    startPtr += len;

    len = sizeof("secureToken");
    p->av_secureToken.pVal = startPtr;
    p->av_secureToken.len = len-1;
    strncpy(p->av_secureToken.pVal, "secureToken", len);
    startPtr += len;

    len = sizeof("secureTokenResponse");
    p->av_secureTokenResponse.pVal = startPtr;
    p->av_secureTokenResponse.len = len-1;
    strncpy(p->av_secureTokenResponse.pVal, "secureTokenResponse", len);
    startPtr += len;

    len = sizeof("type");
    p->av_type.pVal = startPtr;
    p->av_type.len = len-1;
    strncpy(p->av_type.pVal, "type", len);
    startPtr += len;

    len = sizeof("nonprivate");
    p->av_nonprivate.pVal = startPtr;
    p->av_nonprivate.len = len-1;
    strncpy(p->av_nonprivate.pVal, "nonprivate", len);
    startPtr += len;

    len = sizeof("pause");
    p->av_pause.pVal = startPtr;
    p->av_pause.len = len-1;
    strncpy(p->av_pause.pVal, "pause", len);
    startPtr += len;

    len = sizeof("_checkbw");
    p->av__checkbw.pVal = startPtr;
    p->av__checkbw.len = len-1;
    strncpy(p->av__checkbw.pVal, "_checkbw", len);
    startPtr += len;

    len = sizeof("_result");
    p->av__result.pVal = startPtr;
    p->av__result.len = len-1;
    strncpy(p->av__result.pVal, "_result", len);
    startPtr += len;

    len = sizeof("ping");
    p->av_ping.pVal = startPtr;
    p->av_ping.len = len-1;
    strncpy(p->av_ping.pVal, "ping", len);
    startPtr += len;

    len = sizeof("pong");
    p->av_pong.pVal = startPtr;
    p->av_pong.len = len-1;
    strncpy(p->av_pong.pVal, "pong", len);
    startPtr += len;

    len = sizeof("play");
    p->av_play.pVal = startPtr;
    p->av_play.len = len-1;
    strncpy(p->av_play.pVal, "play", len);
    startPtr += len;

    len = sizeof("set_playlist");
    p->av_set_playlist.pVal = startPtr;
    p->av_set_playlist.len = len-1;
    strncpy(p->av_set_playlist.pVal, "set_playlist", len);
    startPtr += len;

    len = sizeof("0");
    p->av_0.pVal = startPtr;
    p->av_0.len = len-1;
    strncpy(p->av_0.pVal, "0", len);
    startPtr += len;

    len = sizeof("onBWDone");
    p->av_onBWDone.pVal = startPtr;
    p->av_onBWDone.len = len-1;
    strncpy(p->av_onBWDone.pVal, "onBWDone", len);
    startPtr += len;

    len = sizeof("onFCSubscribe");
    p->av_onFCSubscribe.pVal = startPtr;
    p->av_onFCSubscribe.len = len-1;
    strncpy(p->av_onFCSubscribe.pVal, "onFCSubscribe", len);
    startPtr += len;

    len = sizeof("onFCUnsubscribe");
    p->av_onFCUnsubscribe.pVal = startPtr;
    p->av_onFCUnsubscribe.len = len-1;
    strncpy(p->av_onFCUnsubscribe.pVal, "onFCUnsubscribe", len);
    startPtr += len;

    len = sizeof("onFCUnsubscribe");
    p->av_onFCUnsubscribe.pVal = startPtr;
    p->av_onFCUnsubscribe.len = len-1;
    strncpy(p->av_onFCUnsubscribe.pVal, "onFCUnsubscribe", len);
    startPtr += len;

    len = sizeof("_onbwcheck");
    p->av__onbwcheck.pVal = startPtr;
    p->av__onbwcheck.len = len-1;
    strncpy(p->av__onbwcheck.pVal, "_onbwcheck", len);
    startPtr += len;

    len = sizeof("_onbwdone");
    p->av__onbwdone.pVal = startPtr;
    p->av__onbwdone.len = len-1;
    strncpy(p->av__onbwdone.pVal, "_onbwdone", len);
    startPtr += len;

    len = sizeof("_error");
    p->av__error.pVal = startPtr;
    p->av__error.len = len-1;
    strncpy(p->av__error.pVal, "_error", len);
    startPtr += len;

    len = sizeof("close");
    p->av_close.pVal = startPtr;
    p->av_close.len = len-1;
    strncpy(p->av_close.pVal, "close", len);
    startPtr += len;

    len = sizeof("code");
    p->av_code.pVal = startPtr;
    p->av_code.len = len-1;
    strncpy(p->av_code.pVal, "code", len);
    startPtr += len;

    len = sizeof("level");
    p->av_level.pVal = startPtr;
    p->av_level.len = len-1;
    strncpy(p->av_level.pVal, "level", len);
    startPtr += len;

    len = sizeof("onStatus");
    p->av_onStatus.pVal = startPtr;
    p->av_onStatus.len = len-1;
    strncpy(p->av_onStatus.pVal, "onStatus", len);
    startPtr += len;

    len = sizeof("playlist_ready");
    p->av_playlist_ready.pVal = startPtr;
    p->av_playlist_ready.len = len-1;
    strncpy(p->av_playlist_ready.pVal, "playlist_ready", len);
    startPtr += len;

    len = sizeof("onMetaData");
    p->av_onMetaData.pVal = startPtr;
    p->av_onMetaData.len = len-1;
    strncpy(p->av_onMetaData.pVal, "onMetaData", len);
    startPtr += len;

    len = sizeof("duration");
    p->av_duration.pVal = startPtr;
    p->av_duration.len = len-1;
    strncpy(p->av_duration.pVal, "duration", len);
    startPtr += len;

    len = sizeof("video");
    p->av_video.pVal = startPtr;
    p->av_video.len = len-1;
    strncpy(p->av_video.pVal, "video", len);
    startPtr += len;

    len = sizeof("audio");
    p->av_audio.pVal = startPtr;
    p->av_audio.len = len-1;
    strncpy(p->av_audio.pVal, "audio", len);
    startPtr += len;

    len = sizeof("audio");
    p->av_audio.pVal = startPtr;
    p->av_audio.len = len-1;
    strncpy(p->av_audio.pVal, "audio", len);
    startPtr += len;

    len = sizeof("FCPublish");
    p->av_FCPublish.pVal = startPtr;
    p->av_FCPublish.len = len-1;
    strncpy(p->av_FCPublish.pVal, "FCPublish", len);
    startPtr += len;

    len = sizeof("FCUnpublish");
    p->av_FCUnpublish.pVal = startPtr;
    p->av_FCUnpublish.len = len-1;
    strncpy(p->av_FCUnpublish.pVal, "FCUnpublish", len);
    startPtr += len;

    len = sizeof("releaseStream");
    p->av_releaseStream.pVal = startPtr;
    p->av_releaseStream.len = len-1;
    strncpy(p->av_releaseStream.pVal, "releaseStream", len);
    startPtr += len;

    len = sizeof("publish");
    p->av_publish.pVal = startPtr;
    p->av_publish.len = len-1;
    strncpy(p->av_publish.pVal, "publish", len);
    startPtr += len;

    len = sizeof("live");
    p->av_live.pVal = startPtr;
    p->av_live.len = len-1;
    strncpy(p->av_live.pVal, "live", len);
    startPtr += len;

    len = sizeof("record");
    p->av_record.pVal = startPtr;
    p->av_record.len = len-1;
    strncpy(p->av_record.pVal, "record", len);
    startPtr += len;

    len = sizeof("seek");
    p->av_seek.pVal = startPtr;
    p->av_seek.len = len-1;
    strncpy(p->av_seek.pVal, "seek", len);
    startPtr += len;

    len = sizeof("createStream");
    p->av_createStream.pVal = startPtr;
    p->av_createStream.len = len-1;
    strncpy(p->av_createStream.pVal, "createStream", len);
    startPtr += len;

    len = sizeof("FCSubscribe");
    p->av_FCSubscribe.pVal = startPtr;
    p->av_FCSubscribe.len = len-1;
    strncpy(p->av_FCSubscribe.pVal, "FCSubscribe", len);
    startPtr += len;

    len = sizeof("deleteStream");
    p->av_deleteStream.pVal = startPtr;
    p->av_deleteStream.len = len-1;
    strncpy(p->av_deleteStream.pVal, "deleteStream", len);
    startPtr += len;

    len = sizeof("NetStream.Authenticate.NetStream.Failed");
    p->av_NetStream_Authenticate_UsherToken.pVal = startPtr;
    p->av_NetStream_Authenticate_UsherToken.len = len-1;
    strncpy(p->av_NetStream_Authenticate_UsherToken.pVal, "NetStream.Authenticate.UsherToken", len);
    startPtr += len;

    len = sizeof("NetStream.Failed");
    p->av_NetStream_Failed.pVal = startPtr;
    p->av_NetStream_Failed.len = len-1;
    strncpy(p->av_NetStream_Failed.pVal, "NetStream.Failed", len);
    startPtr += len;

    len = sizeof("NetStream.Play.Failed");
    p->av_NetStream_Play_Failed.pVal = startPtr;
    p->av_NetStream_Play_Failed.len = len-1;
    strncpy(p->av_NetStream_Play_Failed.pVal, "NetStream.Play.Failed", len);
    startPtr += len;

    len = sizeof("NetStream.Play.StreamNotFound");
    p->av_NetStream_Play_StreamNotFound.pVal = startPtr;
    p->av_NetStream_Play_StreamNotFound.len = len-1;
    strncpy(p->av_NetStream_Play_StreamNotFound.pVal, "NetStream.Play.StreamNotFound", len);
    startPtr += len;

    len = sizeof("NetConnection.Connect.InvalidApp");
    p->av_NetConnection_Connect_InvalidApp.pVal = startPtr;
    p->av_NetConnection_Connect_InvalidApp.len = len-1;
    strncpy(p->av_NetConnection_Connect_InvalidApp.pVal, "NetConnection.Connect.InvalidApp", len);
    startPtr += len;

    len = sizeof("NetStream.Play.Start");
    p->av_NetStream_Play_Start.pVal = startPtr;
    p->av_NetStream_Play_Start.len = len-1;
    strncpy(p->av_NetStream_Play_Start.pVal, "NetStream.Play.Start", len);
    startPtr += len;

    len = sizeof("NetStream.Play.Complete");
    p->av_NetStream_Play_Complete.pVal = startPtr;
    p->av_NetStream_Play_Complete.len = len-1;
    strncpy(p->av_NetStream_Play_Complete.pVal, "NetStream.Play.Complete", len);
    startPtr += len;

    len = sizeof("NetStream.Play.Stop");
    p->av_NetStream_Play_Stop.pVal = startPtr;
    p->av_NetStream_Play_Stop.len = len-1;
    strncpy(p->av_NetStream_Play_Stop.pVal, "NetStream.Play.Stop", len);
    startPtr += len;

    len = sizeof("NetStream.Seek.Notify");
    p->av_NetStream_Seek_Notify.pVal = startPtr;
    p->av_NetStream_Seek_Notify.len = len-1;
    strncpy(p->av_NetStream_Seek_Notify.pVal, "NetStream.Seek.Notify", len);
    startPtr += len;

    len = sizeof("NetStream.Pause.Notify");
    p->av_NetStream_Pause_Notify.pVal = startPtr;
    p->av_NetStream_Pause_Notify.len = len-1;
    strncpy(p->av_NetStream_Pause_Notify.pVal, "NetStream.Pause.Notify", len);
    startPtr += len;

    len = sizeof("NetStream.Play.PublishNotify");
    p->av_NetStream_Play_PublishNotify.pVal = startPtr;
    p->av_NetStream_Play_PublishNotify.len = len-1;
    strncpy(p->av_NetStream_Play_PublishNotify.pVal, "NetStream.Play.PublishNotify", len);
    startPtr += len;

    len = sizeof("NetStream.Play.UnpublishNotify");
    p->av_NetStream_Play_UnpublishNotify.pVal = startPtr;
    p->av_NetStream_Play_UnpublishNotify.len = len-1;
    strncpy(p->av_NetStream_Play_UnpublishNotify.pVal, "NetStream.Play.UnpublishNotify", len);
    startPtr += len;

    len = sizeof("NetStream.Publish.Start");
    p->av_NetStream_Publish_Start.pVal = startPtr;
    p->av_NetStream_Publish_Start.len = len-1;
    strncpy(p->av_NetStream_Publish_Start.pVal, "NetStream.Publish.Start", len);
    startPtr += len;
}

static int aw_add_addr_info(struct sockaddr_in *service, aw_rtmp_aval_t *host, int port)
{
    char *hostname;
    int ret = 1;

    /* if host is not end with '\0', add it */
    if(host->pVal[host->len])
    {
        hostname = malloc(host->len+1);
        memcpy(hostname, host->pVal, host->len);
        hostname[host->len] = '\0';
    }
    else
    {
        hostname = host->pVal;
    }

    service->sin_addr.s_addr = inet_addr(hostname);
   /*将点分十进制字字符串转换为32位二进制网络字节序的IPV4地址*/
    if(service->sin_addr.s_addr == INADDR_NONE)
    {
        struct hostent *host = gethostbyname(hostname);
      /*返回对应于给定主机名的包含主机名字和地址信息的hostent结构指针*/
        if(host == NULL || host->h_addr == NULL)
        {
            ret = 0;
            goto finish;
        }
        service->sin_addr = *(struct in_addr *)host->h_addr;
    }
    service->sin_port = htons(port);  /*主机字节顺序转化为网络字节顺序*/

finish:
    if(hostname != host->pVal)
    {
        free(hostname);
        hostname = NULL;
    }
    return ret;
}

static void DnsResponeHook(void *userhdr, int ret, struct addrinfo *ai)
{
    aw_rtmp_t *rtmp = (aw_rtmp_t *)userhdr;

    if (rtmp == NULL)
      return;

    if (ret == SDS_OK)
    {
        rtmp->dnsAI = ai;

/*        CDX_LOGD("%x%x%x", ai->ai_addr->sa_data[0], ai->ai_addr->sa_data[1],
            ai->ai_addr->sa_data[2]);*/
    }
    rtmp->dnsRet = ret;
    if (rtmp->dnsMutex != NULL)
    {
        pthread_mutex_lock(rtmp->dnsMutex);
        pthread_cond_signal(&rtmp->dnsCond);
        pthread_mutex_unlock(rtmp->dnsMutex);
    }

    return ;
}

static struct addrinfo *aw_add_addr_info_new(aw_rtmp_aval_t *host,
    int port, aw_rtmp_t *r)
{
    char *hostname;
    int ret = 1;
    struct addrinfo *ai = NULL;

    /* if host is not end with '\0', add it */
    if(host->pVal[host->len])
    {
        hostname = malloc(host->len+1);
        memcpy(hostname, host->pVal, host->len);
        hostname[host->len] = '\0';
    }
    else
    {
        hostname = host->pVal;
    }

#if 0
    service->sin_addr.s_addr = inet_addr(hostname);
   /*将点分十进制字字符串转换为32位二进制网络字节序的IPV4地址*/
    if(service->sin_addr.s_addr == INADDR_NONE)
    {
        struct hostent *host = gethostbyname(hostname);
      /*返回对应于给定主机名的包含主机名字和地址信息的hostent结构指针*/
        if(host == NULL || host->h_addr == NULL)
        {
            ret = 0;
            goto finish;
        }
        service->sin_addr = *(struct in_addr *)host->h_addr;
    }
    service->sin_port = htons(port);  /*主机字节顺序转化为网络字节顺序*/
#endif

    ret = SDSRequest(hostname, port, &ai, r, DnsResponeHook);
    if (ret == SDS_OK)
    {
        CDX_FORCE_CHECK(ai);
    }
    else if (ret == SDS_PENDING)
    {
        while (1)
        {
            struct timespec abstime;

            abstime.tv_sec = time(0);
            abstime.tv_nsec = 100000000L;

            pthread_mutex_lock(r->dnsMutex);
            pthread_cond_timedwait(&r->dnsCond, r->dnsMutex, &abstime); /* wait 100 ms */
            pthread_mutex_unlock(r->dnsMutex);

            if (r->exitFlag)
            {
                ai = NULL;
                break;
            }

            if (r->dnsRet == SDS_OK)
            {
                ai = r->dnsAI;
                break;
            }
            else if (r->dnsRet != SDS_PENDING)
            {
                ai = NULL;
                break;
            }
        }

    }

    if(hostname != host->pVal)
    {
        free(hostname);
        hostname = NULL;
    }

    return ai;
}

//******************************************************************************//
//******************************************************************************//
typedef enum {
    RTMPT_OPEN=0, RTMPT_SEND, RTMPT_IDLE, RTMPT_CLOSE
}aw_rtmp_cmd;

static const char *RTMPT_cmds[] = {
  "open",
  "send",
  "idle",
  "close"
};

int aw_rtmp_sockBuf_send(aw_rtmp_socket_buf_t*sb, char *buf, int len, int* exitflag)
{

    int rc;
    fd_set fdset;
    int sendSize = 0;
    struct timeval tv;
    int ret;

    while(1)
    {
        if(*exitflag == 1)
            return -1;

        FD_ZERO(&fdset);
        FD_SET(sb->sbSocket, &fdset);

        tv.tv_sec = 0;
        tv.tv_usec = 100000;
        ret = select(sb->sbSocket+1, NULL, &fdset, NULL, &tv);
        if(ret <= 0)
        {
            continue;
        }

        while(1)
        {
            if(*exitflag == 1)
                return -1;

            rc = send(sb->sbSocket, ((char*)buf)+sendSize, len-sendSize, 0);
            if(rc < 0)
            {
                if(EAGAIN == errno)
                {
                    //buffer not ready
                    break;
                }
                else
                {
                    return -1;
                }
            }
            else if (rc ==0)
            {
                break;
            }
            else
            {
                sendSize += rc;
                if(sendSize == len)
                {
                    return sendSize;
                }
            }
        }
    }
    return sendSize;

}

static int aw_rtmp_http_post(aw_rtmp_t *r, aw_rtmp_cmd cmd, char *buf, int len)
{
    char tmpbuf[512];
    /*http post*/
    int tmplen = snprintf(tmpbuf, sizeof(tmpbuf), "POST /%s%s/%d HTTP/1.1\r\n"
                "Host: %.*s:%d\r\n"
                "Accept: */*\r\n"
                "User-Agent: Shockwave Flash\n"
                "Connection: Keep-Alive\n"
                "Cache-Control: no-cache\r\n"
                "Content-type: application/x-fcs\r\n"
                "Content-length: %d\r\n\r\n", RTMPT_cmds[cmd],
                r->clientID.pVal ? r->clientID.pVal : "",
                r->mMsgCounter, r->link.hostname.len, r->link.hostname.pVal,
                r->link.port, len);
    aw_rtmp_sockBuf_send(&r->sb, tmpbuf, tmplen, &r->exitFlag);
    tmplen = aw_rtmp_sockBuf_send(&r->sb, buf, len, &r->exitFlag);
    r->mMsgCounter++;
    r->unackd++;
    return tmplen;
}

//********************************************************************//
//********************************************************************//
ssize_t CdxRecv(int sockfd, void *buf, size_t len,
                        long timeoutUs/* microseconds */, int *pForceStop)
{
    fd_set rs;
    struct timeval tv;
    ssize_t ret = 0, recvSize = 0;
    long loopTimes = 0, i = 0;
    int ioErr;

    if (timeoutUs == 0)
    {
        loopTimes = ((unsigned long)(-1L))>> 1;
    }
    else
    {
        loopTimes = timeoutUs/100000L;
    }

    for (i = 0; i < loopTimes; i++)
    {
        if (pForceStop && *pForceStop)
        {
            return -1;
        }

        FD_ZERO(&rs);
        FD_SET(sockfd, &rs);
        tv.tv_sec = 0;
        tv.tv_usec = 100000L;
        ret = select(sockfd + 1, &rs, NULL, NULL, &tv);
        if (ret < 0)
        {
            ioErr = errno;
            if (EINTR == ioErr)
            {
                continue;
            }
            return -1;
        }
        else if (ret == 0)
        {
            //printf("select = 0; timeout\n");
            continue;
        }

        while (1)
        {
            if (pForceStop && *pForceStop)
            {
                return -1;
            }

            ret = recv(sockfd, ((char *)buf) + recvSize, len - recvSize, 0);
            if (ret < 0)
            {
                ioErr = errno;
                if (EAGAIN == ioErr /*|| EINTR == ioErr*/)
                {
                    //buffer not ready
                    if (recvSize > 0)
                    {
                        return recvSize;
                    }
                    break;
                }
                else
                {
                    return -1;
                }
            }
            else if (ret == 0)
            {
                //buffer not ready
                if (recvSize > 0)
                {
                    return recvSize;
                }
                break;
            }
            else // ret > 0
            {
                recvSize += ret;
                if ((size_t)recvSize == len)
                {
                    return recvSize;
                }
            }
        }

    }
    return recvSize;
}

//把socket buffer填满
int aw_rtmp_sockbuf_fill(aw_rtmp_socket_buf_t *sb, int *exitflag)
{
    int nBytes;
    int readLen = 0;

    if(!sb->sbSize)
    {
        sb->sbStart = sb->sbBuf;
    }

#if 1
    while (1)
    {
        nBytes = sizeof(sb->sbBuf) - sb->sbSize - (sb->sbStart - sb->sbBuf);
        readLen = CdxRecv(sb->sbSocket, sb->sbStart+sb->sbSize, nBytes, 0, exitflag);

        if(readLen != -1)
        {
            sb->sbSize += readLen;
        }
        else
        {
            sb->sbTimedout = 1;
            readLen = 0;
        }
        break;
    }

    return readLen;

#else
       while (1)
        {
               if(exitflag == 1)
                   return -1;
            nBytes = sizeof(sb->sbBuf) - sb->sbSize
                    - (sb->sbStart - sb->sbBuf);
            readLen = recv(sb->sbSocket, sb->sbStart + sb->sbSize, nBytes, 0);
         //* 接受数据存入socket缓存
            if (readLen != -1)
            {
                sb->sbSize += readLen;
            }
            else
            {
                sb->sbTimedout = 1;
                readLen = 0;
            }
            break;
        }
    return readLen;
#endif
}

//***************************************************************************************//
//***************************************************************************************//
static int aw_rtmp_http_read(aw_rtmp_t *r, int fill)
{
    char *ptr;
    int hlen;

    if(fill)
    {
        if(aw_rtmp_sockbuf_fill(&r->sb, &r->exitFlag)<0)
            return -1;
    }

    if(r->sb.sbSize < 144)
    {
        return -2;
    }
    if(strncmp(r->sb.sbStart, "HTTP/1.1 200 ", 13))
    {
        return -1;
    }
    ptr = r->sb.sbStart + sizeof("HTTP/1.1 200");
    while((ptr = strstr(ptr, "Content-")))
    {
        if(!strncasecmp(ptr+8, "length:", 7))
        {
            break;
        }
        ptr += 8;
    }
    if(!ptr)
    {
        return -1;
    }

    hlen = atoi(ptr+16);
    ptr = strstr(ptr+16, "\r\n\r\n");
    if(!ptr)
    {
        return -1;
    }
    ptr += 4;
    r->sb.sbSize -= ptr - r->sb.sbStart;
    r->sb.sbStart = ptr;
    r->unackd--;

    if(!r->clientID.pVal)
    {
        r->clientID.len = hlen;
        r->clientID.pVal = malloc(hlen+1);
        if(!r->clientID.pVal)
        {
            return -1;
        }
        r->clientID.pVal[0] = '/';
        memcpy(r->clientID.pVal+1, ptr, hlen-1);
        r->clientID.pVal[hlen] = 0;
        r->sb.sbSize = 0;
    }
    else
    {
//        r->m_polling = *ptr++;
        r->resplen = hlen - 1;
        r->sb.sbStart++;
        r->sb.sbSize--;
    }
    return 0;
}

//*******************************************************************************//
//******************************************************************************//
char *aw_amf_encode_int16(char *pOutput, char *pOutend, short nVal)
{
    if(pOutput+2 > pOutend)
    {
        return NULL;
    }
    pOutput[1] = nVal & 0xff;
    pOutput[0] = nVal >> 8;
    return pOutput+2;
}

char *aw_amf_encode_int24(char *pOutput, char *pOutend, int nVal)
{
    if(pOutput+3 > pOutend)
    {
        return NULL;
    }

    pOutput[2] = nVal & 0xff;
    pOutput[1] = nVal >> 8;
    pOutput[0] = nVal >> 16;
    return pOutput+3;
}

char *aw_amf_encode_int32(char *pOutput, char *pOutend, int nVal)
{
    if(pOutput+4 > pOutend)
    {
        return NULL;
    }
    pOutput[3] = nVal & 0xff;
    pOutput[2] = nVal >> 8;
    pOutput[1] = nVal >> 16;
    pOutput[0] = nVal >> 24;
    return pOutput+4;
}

int aw_encode_int32Le(char *pOutput, int nVal)
{
    pOutput[0] = nVal;
    nVal >>= 8;
    pOutput[1] = nVal;
    nVal >>= 8;
    pOutput[2] = nVal;
    nVal >>= 8;
    pOutput[3] = nVal;
    return 4;
}

char * aw_amf_encode_string(char *pOutput, char *pOutend, const aw_rtmp_aval_t *bv)
{
    if((bv->len < 65536 && pOutput + 1 + 2 + bv->len > pOutend) ||
        pOutput + 1 + 4 + bv->len > pOutend)
    {
        return NULL;
    }

    if(bv->len < 65536)
    {
        *pOutput++ = RTMP_AMF_STRING;
        pOutput = aw_amf_encode_int16(pOutput, pOutend, bv->len);
    }
    else
    {
        *pOutput++ = RTMP_AMF_LONG_STRING;
        pOutput = aw_amf_encode_int32(pOutput, pOutend, bv->len);
    }
    memcpy(pOutput, bv->pVal, bv->len);
    pOutput += bv->len;
    return pOutput;
}

char *aw_amf_encode_number(char *output, char *outend, double dVal)
{
    if(output+1+8 > outend)
    {
        return NULL;
    }
    *output++ = RTMP_AMF_NUMBER;    /* type: Number */

    #if __BYTE_ORDER == __BIG_ENDIAN
        memcpy(output, &dVal, 8);
    #elif __BYTE_ORDER == __LITTLE_ENDIAN
    {
        unsigned char *pCi, *pCo;
        pCi = (unsigned char *)&dVal;
        pCo = (unsigned char *)output;
        pCo[0] = pCi[7];
        pCo[1] = pCi[6];
        pCo[2] = pCi[5];
        pCo[3] = pCi[4];
        pCo[4] = pCi[3];
        pCo[5] = pCi[2];
        pCo[6] = pCi[1];
        pCo[7] = pCi[0];
    }
    #endif
    return output+8;
}

char *aw_amf_encode_boolean(char *pOutput, char *pOutend, int bVal)
{
    if(pOutput+2 > pOutend)
    {
        return NULL;
    }
    *pOutput++ = RTMP_AMF_BOOLEAN;
    *pOutput++ = bVal ? 0x01 : 0x00;
    return pOutput;
}

/*  add string(0x00) and the string value to output
*/
char *aw_amf_encode_named_string(char *pOutput, char *pOutend, aw_rtmp_aval_t *strName,
                      aw_rtmp_aval_t *strValue)
{
    if((pOutput+2+strName->len) > pOutend)
    {
        return NULL;
    }
    pOutput = aw_amf_encode_int16(pOutput, pOutend, strName->len);
    memcpy(pOutput, strName->pVal, strName->len);
    pOutput += strName->len;
    return aw_amf_encode_string(pOutput, pOutend, strValue);
}

char *aw_amf_encode_named_number(char *pOutput, char *pOutend, aw_rtmp_aval_t *strName, double dVal)
{
    if((pOutput+2+strName->len) > pOutend)
    {
        return NULL;
    }
    pOutput = aw_amf_encode_int16(pOutput, pOutend, strName->len);
    memcpy(pOutput, strName->pVal, strName->len);
    pOutput += strName->len;
    return aw_amf_encode_number(pOutput, pOutend, dVal);
}

char *aw_amf_encode_named_boolean(char *pOutput, char *pOutend,aw_rtmp_aval_t *strName, int bVal)
{
    if(pOutput+2+strName->len > pOutend)
    {
        return NULL;
    }
    pOutput = aw_amf_encode_int16(pOutput, pOutend, strName->len);
    memcpy(pOutput, strName->pVal, strName->len);
    pOutput += strName->len;
    return aw_amf_encode_boolean(pOutput, pOutend, bVal);
}

void aw_amf_prop_get_name(aw_amfobject_property_t *prop, aw_rtmp_aval_t *name)
{
    *name = prop->name;
}

int aw_amf_decode_boolean(char *data)
{
    return *data != 0;
}

/* Data is Big-Endian */
unsigned short aw_amf_decode_int16(const char *data)
{
    unsigned char *c = NULL;
    unsigned short val;

    c = (unsigned char *) data;
    val = (c[0] << 8) | c[1];
    return val;
}

unsigned int aw_amf_decode_int24(const char *data)
{
    unsigned char *c = NULL;
    unsigned int val;

    c = (unsigned char *) data;
    val = (c[0] << 16) | (c[1] << 8) | c[2];
    return val;
}

unsigned int aw_amf_decode_int32(const char *data)
{
    unsigned char *c = NULL;
    unsigned int val;

    c = (unsigned char *)data;
    val = (c[0] << 24) | (c[1] << 16) | (c[2] << 8) | c[3];
    return val;
}

void aw_amf_decode_string(char *data, aw_rtmp_aval_t *bv)
{
    bv->len = aw_amf_decode_int16(data);
    bv->pVal = (bv->len > 0) ? (char *)data + 2 : NULL;
}

double aw_amf_decode_number(const char *data)
{
    double dVal;

    #if __BYTE_ORDER == __BIG_ENDIAN
        memcpy(&dVal, data, 8);
    #elif __BYTE_ORDER == __LITTLE_ENDIAN
        unsigned char *pCi, *pCo;
        pCi = (unsigned char *)data;
        pCo = (unsigned char *)&dVal;
        pCo[0] = pCi[7];
        pCo[1] = pCi[6];
        pCo[2] = pCi[5];
        pCo[3] = pCi[4];
        pCo[4] = pCi[3];
        pCo[5] = pCi[2];
        pCo[6] = pCi[1];
        pCo[7] = pCi[0];
    #endif

    return dVal;
}

//*******************************************************************************//
//*******************************************************************************//
static void aw_av_queue(aw_rtmp_method_t**vals, int *num, aw_rtmp_aval_t *av, int txn)
{
    char *tmp;
    if(!(*num & 0x0f))
    {
        *vals = realloc(*vals, (*num + 16) * sizeof(aw_rtmp_method_t));
    }
    tmp = malloc(av->len + 1);
    memcpy(tmp, av->pVal, av->len);
     tmp[av->len] = '\0';
    (*vals)[*num].num = txn;
    (*vals)[*num].name.len = av->len;
    (*vals)[(*num)++].name.pVal = tmp;
}

//*******************************************************************************//
//*******************************************************************************//

#define AW_RTMP_PACKET_SIZE_LARGE    0
#define AW_RTMP_PACKET_SIZE_MEDIUM   1
#define AW_RTMP_PACKET_SIZE_SMALL    2
#define AW_RTMP_PACKET_SIZE_MINIMUM  3

/*message type id*/
#define AW_RTMP_PACKET_TYPE_CHUNK_SIZE         0x01      /* set chunk size*/
#define AW_RTMP_PACKET_TYPE_BYTES_READ_REPORT  0x03      /* Acknowledge message*/
#define AW_RTMP_PACKET_TYPE_CONTROL            0x04      /* user control message */
#define AW_RTMP_PACKET_TYPE_SERVER_BW          0x05
/* the spec present is Windows Acknowledge message(?)  */
#define AW_RTMP_PACKET_TYPE_CLIENT_BW          0x06      /* set peer bandwidth */
#define AW_RTMP_PACKET_TYPE_AUDIO              0x08      /*   audio   */
#define AW_RTMP_PACKET_TYPE_VIDEO              0x09      /*    video  */
#define AW_RTMP_PACKET_TYPE_FLEX_STREAM_SEND   0x0F      /*   (AMF3)data message   */
#define AW_RTMP_PACKET_TYPE_FLEX_SHARED_OBJECT 0x10      /*   (AMF3)share object message  */
#define AW_RTMP_PACKET_TYPE_FLEX_MESSAGE       0x11      /*   (AMF3)command message  */
#define AW_RTMP_PACKET_TYPE_INFO               0x12      /*   (AMF0)data message(metadata)   */
#define AW_RTMP_PACKET_TYPE_SHARED_OBJECT      0x13      /*   (AMF0)share object message   */
#define AW_RTMP_PACKET_TYPE_INVOKE             0x14      /*   (AMF0)command message  */
#define AW_RTMP_PACKET_TYPE_FLASH_VIDEO        0x16      /*    Aggregate message */

#define RTMP_MAX_HEADER_SIZE 18
/*chunk basic header(max bytes:3)+ message header(11)+ extend time stamp*/

int aw_rtmp_send_packet(aw_rtmp_t *r, aw_rtmp_packet_t *packet, int queue)
{
    aw_rtmp_packet_t *prevPacket;
    unsigned int last = 0;
    unsigned int t = 0;
    int nSize = 0;
    int hSize = 0;
    int cSize = 0;
    int nChunkSize = 0;
    int tlen = 0;
    char *header = NULL;
    char *hptr = NULL;
    char *hend = NULL;
    char *buffer = NULL;
    char *tbuf = NULL;
    char *toff = NULL;
    char c = 0;
    char hbuf[RTMP_MAX_HEADER_SIZE];
    int packetSize[] = { 12, 8, 4, 1 };

    prevPacket = r->vecChannelsOut[packet->nChannel];   // prev packet
    if(prevPacket && packet->nHeaderType != AW_RTMP_PACKET_SIZE_LARGE)
    {
        /* compress a bit by using the prev packet's attributes */
        if(prevPacket->nBodySize == packet->nBodySize
          && prevPacket->mPacketType == packet->mPacketType
          && packet->nHeaderType == AW_RTMP_PACKET_SIZE_MEDIUM)
        {
            packet->nHeaderType = AW_RTMP_PACKET_SIZE_SMALL;
        }

        if(prevPacket->nTimeStamp == packet->nTimeStamp
        && packet->nHeaderType == AW_RTMP_PACKET_SIZE_SMALL)
        {
            packet->nHeaderType = AW_RTMP_PACKET_SIZE_MINIMUM;
        }
        last = prevPacket->nTimeStamp;
   }

    if(packet->nHeaderType > 3)    /* sanity */
    {
        return 0;
    }

    nSize = packetSize[packet->nHeaderType];
    hSize = nSize; cSize = 0;
    t = packet->nTimeStamp - last;

    if(packet->pBody)
    {
        header = packet->pBody - nSize;
        hend = packet->pBody;
    }
    else
    {
        header = hbuf + 6;
        hend = hbuf + sizeof(hbuf);
    }

    if(packet->nChannel > 319)
    {
        cSize = 2;
    }
    else if(packet->nChannel > 63)
    {
        cSize = 1;
    }

    if(cSize)
    {
        header -= cSize;
        hSize += cSize;
    }

    if(nSize > 1 && t >= 0xffffff)
    {
        header -= 4;
        hSize += 4;
    }

    hptr = header;
    c = packet->nHeaderType << 6;

    switch(cSize)
    {
        case 0:
            c |= packet->nChannel;
            break;
        case 1:
            break;
        case 2:
            c |= 1;
            break;
    }

    *hptr++ = c;
    if(cSize)
    {
        int tmp = packet->nChannel - 64;
        *hptr++ = tmp & 0xff;
        if(cSize == 2)
        {
            *hptr++ = tmp >> 8;
        }
    }

    if(nSize > 1)
    {
        hptr = aw_amf_encode_int24(hptr, hend, t > 0xffffff ? 0xffffff : t);
    }

    if(nSize > 4)
    {
        hptr = aw_amf_encode_int24(hptr, hend, packet->nBodySize);
        *hptr++ = packet->mPacketType;
    }

    if(nSize > 8)
    {
        hptr += aw_encode_int32Le(hptr, packet->nInfoField2);
    }

    if(nSize > 1 && t >= 0xffffff)
    {
        hptr = aw_amf_encode_int32(hptr, hend, t);
    }

    nSize = packet->nBodySize;
    buffer = packet->pBody;
    nChunkSize = r->outChunkSize;

    /* send all chunks in one HTTP request */
    if(r->link.protocol & CDX_RTMP_FEATURE_HTTP)
    {
        int nChunks = (nSize+nChunkSize-1) / nChunkSize;
        if(nChunks > 1)
        {
            tlen = nChunks * (cSize + 1) + nSize + hSize;
            tbuf = malloc(tlen);
            if(!tbuf)
            {
                return 0;
            }
            toff = tbuf;
        }
    }

    while(nSize + hSize)
    {
        int wrote;
        if(nSize < nChunkSize)
        {
            nChunkSize = nSize;
        }
        //RTMP_LogHexString(RTMP_LOGDEBUG2, (uint8_t *)header, hSize);
        //RTMP_LogHexString(RTMP_LOGDEBUG2, (uint8_t *)buffer, nChunkSize);
        if(tbuf)
        {
            memcpy(toff, header, nChunkSize + hSize);
            toff += nChunkSize + hSize;
        }
        else
        {
            wrote = WriteN(r, header, nChunkSize + hSize);
            if(!wrote)
            {
                return 0;
            }
        }

        hSize = 0;
        nSize -= nChunkSize;
        buffer += nChunkSize;

        if(nSize > 0)
        {
            header = buffer - 1;
            hSize = 1;
            if(cSize)
            {
                hSize += cSize;
                header -= cSize;
            }
            *header = (0xc0 | c);
            if(cSize)
            {
                int tmp = packet->nChannel - 64;
                header[1] = tmp & 0xff;
                if(cSize == 2)
                {
                    header[2] = tmp >> 8;
                }
            }
        }
    }

    if(tbuf)
    {
        int wrote = WriteN(r, tbuf, toff-tbuf);
        free(tbuf);
        tbuf = NULL;
        if (!wrote)
        {
            return 0;
        }
    }

    /* we invoked a remote method */
    if(packet->mPacketType == AW_RTMP_PACKET_TYPE_INVOKE)
    {
        aw_rtmp_aval_t method;
        char *ptr;

        ptr = packet->pBody + 1;
        aw_amf_decode_string(ptr, &method);
        /* keep it in call queue till result arrives */
        if(queue)
        {
            int txn;
            ptr += 3 + method.len;
            txn = (int)aw_amf_decode_number(ptr);
            aw_av_queue(&r->pMethodCalls, &r->numCalls, &method, txn);
        }
    }

    if(!r->vecChannelsOut[packet->nChannel])
    {
        r->vecChannelsOut[packet->nChannel] = malloc(sizeof(aw_rtmp_packet_t));
    }
    memcpy(r->vecChannelsOut[packet->nChannel], packet, sizeof(aw_rtmp_packet_t));
    return 1;
}

//*********************************************************************//
//********************************************************************//
static int aw_send_bytes_received(aw_rtmp_t *r)
{
  aw_rtmp_packet_t packet;
  char pbuf[256];
  char *pend = NULL;

  pend = pbuf + sizeof(pbuf);
  packet.nChannel = 0x02;    /* control channel (invoke) */
  packet.nHeaderType = AW_RTMP_PACKET_SIZE_MEDIUM;
  packet.mPacketType = AW_RTMP_PACKET_TYPE_BYTES_READ_REPORT;
  packet.nTimeStamp = 0;
  packet.nInfoField2 = 0;
  packet.nHasAbsTimestamp = 0;
  packet.pBody = pbuf + RTMP_MAX_HEADER_SIZE;

  packet.nBodySize = 4;

  aw_amf_encode_int32(packet.pBody, pend, r->nBytesIn);    /* hard coded for now */
  r->nBytesInSent = r->nBytesIn;
  return aw_rtmp_send_packet(r, &packet, 0);
}
//*******************************************************************************//
//*******************************************************************************//
/*从socket buffer读n个字节到client buffer*/
static int ReadN(aw_rtmp_t *r, char *buffer, int n)
{
    int nOriginalSize = n;
    int avail;
    char *ptr;

    r->sb.sbTimedout = 0;
    ptr = buffer;

    while(n > 0)
    {
        int nBytes = 0, nRead;
        if(r->link.protocol & CDX_RTMP_FEATURE_HTTP)
        {
            while(!r->resplen)
            {
                if(r->sb.sbSize < 144)
                {
                    if(!r->unackd)
                    {
                        aw_rtmp_http_post(r, RTMPT_IDLE, "", 1);
                    }
                    if(aw_rtmp_sockbuf_fill(&r->sb, &r->exitFlag) < 1)
                    {
                        if(!r->sb.sbTimedout)
                        {
                            aw_rtmp_close(r);
                        }
                        return 0;
                    }
                }
                if(aw_rtmp_http_read(r, 0) == -1)
                {
                    aw_rtmp_close(r);
                    return 0;
                }
            }
            if(r->resplen && !r->sb.sbSize)
            {
                aw_rtmp_sockbuf_fill(&r->sb, &r->exitFlag);
            }
            avail = r->sb.sbSize;
            if(avail > r->resplen)
            {
                avail = r->resplen;
            }
        }
        else
        {
            avail = r->sb.sbSize;
            if(avail == 0)
            {
                if(aw_rtmp_sockbuf_fill(&r->sb, &r->exitFlag) < 1)
                {
                    if(!r->sb.sbTimedout)
                    {
                        aw_rtmp_close(r);
                    }
                    return 0;
                }
                avail = r->sb.sbSize;
            }
        }

        nRead = ((n < avail) ? n : avail);
        if(nRead > 0)
        {
            memcpy(ptr, r->sb.sbStart, nRead);  /*copy data from socket buffer to client buffer*/
            r->sb.sbStart += nRead;
            r->sb.sbSize -= nRead;
            nBytes = nRead;
            r->nBytesIn += nRead;
            if(r->bSendCounter && r->nBytesIn > ( r->nBytesInSent + r->nClientBW / 10)) //if
            {
                if(!aw_send_bytes_received(r))
                {
                    return 0;
                }
            }
        }

        if(nBytes == 0)
        {
            /*goto again; */
            aw_rtmp_close(r);
            break;
        }

        if(r->link.protocol & CDX_RTMP_FEATURE_HTTP)
        {
            r->resplen -= nBytes;
        }
        n -= nBytes;
        ptr += nBytes;
    }
    return nOriginalSize - n;
}

static int WriteN(aw_rtmp_t *r, char *buffer, int n)
{
    char *ptr = buffer;

    while (n > 0)
    {
        int nBytes;
        /*如果是http形式，则需要在传输之前添加一些请求信息*/
        if(r->link.protocol & CDX_RTMP_FEATURE_HTTP)
        {
            nBytes = aw_rtmp_http_post(r, RTMPT_SEND, ptr, n);
        }
        else
        {
            nBytes = aw_rtmp_sockBuf_send(&r->sb, ptr, n, &r->exitFlag);
        }

        if(nBytes < 0)
        {
            //aw_rtmp_close(r);
            n = 1;
            break;
        }

        if(nBytes == 0)
        {
            break;
        }
        n -= nBytes;
        ptr += nBytes;
    }
    return n == 0;
}

//**********************************************************************//
//**********************************************************************//
#define SET_RCVTIMEO(tv,s)    tv = s*1000
#define GetSockError()    WSAGetLastError()

static int SocksNegotiate(aw_rtmp_t *r)
{
    unsigned long addr;
    struct sockaddr_in service;

    memset(&service, 0, sizeof(struct sockaddr_in));

    aw_add_addr_info(&service, &r->link.hostname, r->link.port);
    addr = htonl(service.sin_addr.s_addr);      /*将一个32位数从主机字节顺序转换成网络字节顺序。*/

    {
        char packet[] = {4, 1,           /* SOCKS 4, connect */
                        (r->link.port >> 8) & 0xFF,
                        (r->link.port) & 0xFF,
                        (char)(addr >> 24) & 0xFF, (char)(addr >> 16) & 0xFF,
                        (char)(addr >> 8) & 0xFF, (char)addr & 0xFF,
                         0};                /* NULL terminate */

        WriteN(r, packet, sizeof packet);

        if(ReadN(r, packet, 8) != 8)
        {
            return 0;
        }

        if(packet[0] == 0 && packet[1] == 90)
        {
            return 1;
        }
        else
        {
            return 0;
        }
     }
}

int aw_rtmp_connect3(aw_rtmp_t *r, struct sockaddr * service)
{
    int on = 1;
    int tv =  0;

    r->sb.sbTimedout = 0;
    r->pausing = 0;
    r->fDuration = 0.0;

    r->sb.sbSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(r->sb.sbSocket != -1)
    {
        if(connect(r->sb.sbSocket, service, sizeof(struct sockaddr)) < 0)
        {
            //int err = GetSockError();
            aw_rtmp_close(r);
            return 0;
        }

        if(r->link.socksport)
        {
            if(!SocksNegotiate(r))
            {
                aw_rtmp_close(r);
                return 0;
            }
        }
    }
    else
    {
        return 0;
    }

    /* set timeout */
    SET_RCVTIMEO(tv, r->link.timeout);
    setsockopt(r->sb.sbSocket, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(tv));
    setsockopt(r->sb.sbSocket, IPPROTO_TCP, TCP_NODELAY, (char *) &on, sizeof(on));
    return 1;
}

int aw_rtmp_connect0(aw_rtmp_t *r, struct sockaddr * service)
{
    int on = 1;
    int tv =  0;
    fd_set fdset;

    r->sb.sbTimedout = 0;
    r->pausing = 0;
    r->fDuration = 0.0;

    r->sb.sbSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);  //create socket

    // Turn the socket as non blocking so we can timeout on the connection
    int ret = fcntl( r->sb.sbSocket, F_SETFL, fcntl(r->sb.sbSocket, F_GETFL, 0) | O_NONBLOCK );
    if(ret < 0)
    {
        return 0;
    }

    if(r->sb.sbSocket != -1) // SOCKET_ERROR : -1
    {
        if(connect(r->sb.sbSocket, service, sizeof(struct sockaddr)) != 0)
      //*connect将socket连接到服务端
        {

            if (errno != EINPROGRESS)  //*阻塞
            {
                r->iostate = CDX_IO_STATE_ERROR;
                aw_rtmp_close(r);
                return 0;
            }

            while (1)
            {
                if(r->exitFlag == 1)
                    return -1;

                struct timeval timeout;
                timeout.tv_sec = 0;
                timeout.tv_usec = 50000;

                FD_ZERO(&fdset);
                FD_SET(0, &fdset);
                FD_SET(r->sb.sbSocket, &fdset);
                ret = select(r->sb.sbSocket + 1, NULL, &fdset, NULL, &timeout);
                if (ret > 0)
                {
                    if (r->link.socksport)
                    {
                        if (!SocksNegotiate(r))  //*判断是否连接成功？
                        {
                            aw_rtmp_close(r);
                            return 0;
                        }
                    }
                    break;
                }
                else
                {
                    r->iostate = CDX_IO_STATE_ERROR;
                }
            }
//            fcntl( r->sb.sbSocket, F_SETFL, fcntl(r->sb.sbSocket, F_GETFL, 0) & (~O_NONBLOCK) );
        }
    }
    else
    {
        return 0;
    }
        //* set timeout
    SET_RCVTIMEO(tv, r->link.timeout);
    setsockopt(r->sb.sbSocket, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(tv));
   //*设置套接字属性
    setsockopt(r->sb.sbSocket, IPPROTO_TCP, TCP_NODELAY, (char *) &on, sizeof(on));

    return 1;
}

//******************************************************************************************//
//******************************************************************************************//
#define RTMP_SIG_SIZE 1536
unsigned int  aw_rtmp_get_time()
{
    #if 0
    struct tms t;
    if(!clk_tck)
    {
        clk_tck = sysconf(_SC_CLK_TCK);
    }
    return times(&t) * 1000 / clk_tck;
    #endif
    return 0;
}

static int aw_rtmp_hand_shake(aw_rtmp_t *r, int FP9HandShake)
{
    CDX_UNUSE(FP9HandShake);
    int i = 0;

    char type = 0;
    unsigned int uptime = 0;
    unsigned int suptime = 0;
    char *clientsig = NULL;
    char clientbuf[RTMP_SIG_SIZE + 1];
    char serversig[RTMP_SIG_SIZE];

    clientsig = clientbuf + 1;
    clientbuf[0] = 0x03;        /* not encrypted */
    uptime = htonl(aw_rtmp_get_time());
    memcpy(clientsig, &uptime, 4);    /*time stamp*/
    memset(&clientsig[4], 0, 4);     /* zero */
    /*client随机生成1528个字节的数据*/
    for(i = 8; i < RTMP_SIG_SIZE; i++)
    {
        clientsig[i] = (char)(rand() % 256);
    }

    if(!WriteN(r, clientbuf, RTMP_SIG_SIZE + 1))  /*C0+C1*/
    {
        CDX_LOGW("write C0,C1 error");
        return 0;
    }

    if(ReadN(r, &type, 1) != 1)    /* 0x03 or 0x06  (S0)*/
    {
        return 0;
    }

    if(ReadN(r, serversig, RTMP_SIG_SIZE) != RTMP_SIG_SIZE)   /*S1*/
    {
        CDX_LOGW(" the size of server send S1 is 1536");
        return 0;
    }

    /* decode server response */

    memcpy(&suptime, serversig, 4);
    suptime = ntohl(suptime);

    /* 2nd part of handshake */
    if(!WriteN(r, serversig, RTMP_SIG_SIZE))   /*C2*/
    {
        return 0;
    }

    if(ReadN(r, serversig, RTMP_SIG_SIZE) != RTMP_SIG_SIZE)  /*S2*/
    {
        return 0;
    }

    //if(memcmp(serversig, clientsig, RTMP_SIG_SIZE) != 0)
    //{
    //}
    return 1;
}

//**************************************************************************************//
//**************************************************************************************//
char *aw_amf_encode(aw_amf_object_t *obj, char *pBuffer, char *pBufEnd)
{
    int i;

    if(pBuffer+4 >= pBufEnd)
    {
        return NULL;
    }

    *pBuffer++ = RTMP_AMF_OBJECT;
    for(i = 0; i < obj->o_num; i++)
    {
        char *res = aw_amf_prop_encode(&obj->o_props[i], pBuffer, pBufEnd);
        if(res == NULL)
        {
            break;
        }
        else
        {
            pBuffer = res;
        }
    }

    if(pBuffer + 3 >= pBufEnd)
    {
        return NULL;            /* no room for the end marker */
    }
    pBuffer = aw_amf_encode_int24(pBuffer, pBufEnd, RTMP_AMF_OBJECT_END);
    return pBuffer;
}

//**************************************************************************//
//**************************************************************************//
char *aw_amf_prop_encode(aw_amfobject_property_t  *prop, char *pBuffer, char *pBufEnd)
{
    if(prop->eType == RTMP_AMF_INVALID)
    {
        return NULL;
    }

    if(prop->eType != RTMP_AMF_NULL && pBuffer + prop->name.len + 2 + 1 >= pBufEnd)
    {
        return NULL;
    }

    if(prop->eType != RTMP_AMF_NULL && prop->name.len)
    {
        *pBuffer++ = prop->name.len >> 8;
        *pBuffer++ = prop->name.len & 0xff;
        memcpy(pBuffer, prop->name.pVal, prop->name.len);
        pBuffer += prop->name.len;
    }

    switch(prop->eType)
    {
        case RTMP_AMF_NUMBER:
            pBuffer = aw_amf_encode_number(pBuffer, pBufEnd, prop->uVu.nNum);
            break;
        case RTMP_AMF_BOOLEAN:
            pBuffer = aw_amf_encode_boolean(pBuffer, pBufEnd, prop->uVu.nNum != 0);
            break;
        case RTMP_AMF_STRING:
            pBuffer = aw_amf_encode_string(pBuffer, pBufEnd, &prop->uVu.aval);
            break;
        case RTMP_AMF_NULL:
            if(pBuffer+1 >= pBufEnd)
            {
                return NULL;
            }
            *pBuffer++ = RTMP_AMF_NULL;
            break;
        case RTMP_AMF_OBJECT:
            pBuffer = aw_amf_encode(&prop->uVu.pObject, pBuffer, pBufEnd);
            break;
        default:
            pBuffer = NULL;
    }
    return pBuffer;
}

//**************************************************************************************//
//**************************************************************************************//
#define AW_RTMP_LF_AUTH    0x0001    /* using auth param */
#define AW_RTMP_LF_LIVE    0x0002    /* stream is live */
#define AW_RTMP_LF_SWFV    0x0004    /* do SWF verification */
#define AW_RTMP_LF_PLST    0x0008    /* send playlist before play */
#define AW_RTMP_LF_BUFX    0x0010    /* toggle stream on BufferEmpty msg */
#define AW_RTMP_LF_FTCU    0x0020    /* free tcUrl on close */

/*
*    send connect command
*/
static int aw_send_connect_packet(aw_rtmp_t *r, aw_rtmp_packet_t *cp)
{
    aw_rtmp_packet_t packet;
    char pbuf[4096];
    char *pend = NULL;
    char *enc;

    pend = pbuf + sizeof(pbuf);

    if(cp)
    {
        return aw_rtmp_send_packet(r, cp, 1);
    }

    packet.nChannel = 0x03;    /* control channel (invoke) */
    packet.nHeaderType = AW_RTMP_PACKET_SIZE_LARGE;    /*header format*/
    packet.mPacketType = AW_RTMP_PACKET_TYPE_INVOKE;   /*message type id*/
    packet.nTimeStamp = 0;
    packet.nInfoField2 = 0;
    packet.nHasAbsTimestamp = 0;
    packet.pBody = pbuf + RTMP_MAX_HEADER_SIZE;

    enc = packet.pBody;
    enc = aw_amf_encode_string(enc, pend, &r->rtmpParam.av_connect);
    /*connect command: 0x02 00 07 "connect" */
    enc = aw_amf_encode_number(enc, pend, ++r->numInvokes);
   /* connect transcation id (always set to 1)*/
    *enc++ = RTMP_AMF_OBJECT;  /* command object(AMF0 type is 0x03) */

    enc = aw_amf_encode_named_string(enc, pend,
                              (aw_rtmp_aval_t*)(&r->rtmpParam.av_app), &r->link.app);
    if (!enc)
    {
        return 0;
    }
    if(r->link.protocol & CDX_RTMP_FEATURE_WRITE)
    {
        enc = aw_amf_encode_named_string(enc, pend, (aw_rtmp_aval_t*)(&r->rtmpParam.av_type),
                              (aw_rtmp_aval_t*)(&r->rtmpParam.av_nonprivate));
        if(!enc)
        {
            return 0;
        }
    }

    if(r->link.flashVer.len)
    {
        enc = aw_amf_encode_named_string(enc, pend, (aw_rtmp_aval_t*)(&r->rtmpParam.av_flashVer),
                               &r->link.flashVer);
        if (!enc)
        {
            return 0;
        }
    }
    if(r->link.swfUrl.len)
    {
        enc = aw_amf_encode_named_string(enc, pend, (aw_rtmp_aval_t*)(&r->rtmpParam.av_swfUrl),
                                 &r->link.swfUrl);
        if(!enc)
        {
            return 0;
        }
    }
    if(r->link.tcUrl.len)
    {
        enc = aw_amf_encode_named_string(enc, pend, (aw_rtmp_aval_t*)(&r->rtmpParam.av_tcUrl),
                                &r->link.tcUrl);
        if (!enc)
        {
            return 0;
        }
    }

    if(!(r->link.protocol & CDX_RTMP_FEATURE_WRITE))
    {
        enc = aw_amf_encode_named_boolean(enc, pend, (aw_rtmp_aval_t*)(&r->rtmpParam.av_fpad), 0);
        if(!enc)
        {
            return 0;
        }
        enc = aw_amf_encode_named_number(enc, pend,
                     (aw_rtmp_aval_t*)(&r->rtmpParam.av_capabilities), 15.0);
        if(!enc)
        {
            return 0;
        }
        enc = aw_amf_encode_named_number(enc, pend,
                     (aw_rtmp_aval_t*)(&r->rtmpParam.av_audioCodecs), r->fAudioCodecs);
        if(!enc)
        {
            return 0;
        }
        enc = aw_amf_encode_named_number(enc, pend,
                     (aw_rtmp_aval_t*)(&r->rtmpParam.av_videoCodecs), r->fVideoCodecs);
        if(!enc)
        {
            return 0;
        }
        enc = aw_amf_encode_named_number(enc, pend,
                     (aw_rtmp_aval_t*)(&r->rtmpParam.av_videoFunction), 1.0);
        if(!enc)
        {
            return 0;
        }
        if(r->link.pageUrl.len)
        {
            enc = aw_amf_encode_named_string(enc, pend,
                     (aw_rtmp_aval_t*)(&r->rtmpParam.av_pageUrl), &r->link.pageUrl);
            if (!enc)
            {
                return 0;
            }
        }
    }

    if(  0.001<= r->fEncoding || r->fEncoding <= -0.001)
    {
        /* AMF0, AMF3 not fully supported yet */
        enc = aw_amf_encode_named_number(enc, pend,
                       (aw_rtmp_aval_t*)(&r->rtmpParam.av_objectEncoding), r->fEncoding);
        if(!enc)
        {
            return 0;
        }
    }

    if(enc + 3 >= pend)
    {
        return 0;
    }
    *enc++ = 0;
    *enc++ = 0;            /* end of object - 0x00 0x00 0x09 */
    *enc++ = RTMP_AMF_OBJECT_END;

    /* add auth string */
    if(r->link.auth.len)
    {
        enc = aw_amf_encode_boolean(enc, pend, r->link.lFlags & AW_RTMP_LF_AUTH);
        if(!enc)
        {
            return 0;
        }
        enc = aw_amf_encode_string(enc, pend, &r->link.auth);
        if (!enc)
        {
            return 0;
        }
    }
    if(r->link.extras.o_num)
    {
        int i;
        for(i = 0; i < r->link.extras.o_num; i++)
        {
            enc = aw_amf_prop_encode(&r->link.extras.o_props[i], enc, pend);
            if(!enc)
            {
                return 0;
            }
        }
    }
    packet.nBodySize = enc - packet.pBody;
    return aw_rtmp_send_packet(r, &packet, 1);
}

//**************************************************************************************//
//**************************************************************************************//
int aw_rtmp_connect1(aw_rtmp_t *r, aw_rtmp_packet_t *cp)
{
    if(r->link.protocol & CDX_RTMP_FEATURE_SSL)
    {
        CDX_LOGE("it is SSL rtmp. not support");
        //aw_rtmp_close(r);
        return 0;
    }

    if(r->link.protocol & CDX_RTMP_FEATURE_HTTP)
    {
        r->mMsgCounter = 1;
        r->clientID.pVal = NULL;
        r->clientID.len = 0;
        aw_rtmp_http_post(r, RTMPT_OPEN, "", 1);
        if(aw_rtmp_http_read(r, 1) != 0)
        {
            r->mMsgCounter = 0;
            //aw_rtmp_close(r);
            return 0;
        }
        r->mMsgCounter = 0;
    }

    if(!aw_rtmp_hand_shake(r, 1))
    {
        CDX_LOGW("hand shake error, rtmp close");
        //aw_rtmp_close(r);
        return 0;
    }
    if(!aw_send_connect_packet(r, cp))
    {
        CDX_LOGW("connect packet error, rtmp close");
        //aw_rtmp_close(r);
        return 0;
    }
    return 1;
}

//******************************************************************************************//
//******************************************************************************************//
int aw_rtmp_connect(aw_rtmp_t *r, aw_rtmp_packet_t *cp)
{
    struct sockaddr_in service;  /* server地址*/

    if(!r->link.hostname.len)
    {
        return 0;
    }
    memset(&service, 0, sizeof(struct sockaddr_in));
    service.sin_family = AF_INET;

    if(r->link.socksport)
    {
        /* Connect via SOCKS */
        if(!aw_add_addr_info(&service, &r->link.sockshost, (int)r->link.socksport))
        {
            CDX_LOGW("Connect via SOCKS  error");
            return 0;
        }
    }
    else
    {
    #if 0
        /* Connect directly */
        if(!aw_add_addr_info(&service, &r->link.hostname, (int)r->link.port))
        {
            CDX_LOGW("-- Connect directly error, hostname = %s", r->link.hostname.pVal);
            return 0;
        }
    #else
        struct addrinfo *ai;
        ai = aw_add_addr_info_new(&r->link.hostname, (int)r->link.port, r);
        if(NULL == ai)
        {
            CDX_LOGW("-- Connect directly error, hostname = %s", r->link.hostname.pVal);
            return 0;
        }
        service = *(struct sockaddr_in *)ai->ai_addr;

        #if 0 //for debug
        {
            struct addrinfo *curInfo;
            struct sockaddr_in *addrInfo;
            do
            {
                curInfo = ai;
                cdx_char ipbufInfo[100];
                addrInfo = (struct sockaddr_in *)curInfo->ai_addr;
                inet_ntop(AF_INET, &addrInfo->sin_addr, ipbufInfo, 100);
                logd("xxxxxxxxxxxxx ip: %s, host: %s, ai->ai_addr:%p",
                    ipbufInfo, r->link.hostname.pVal, ai->ai_addr);
            }while ((ai = ai->ai_next) != NULL);
        }
        #endif
    #endif
    }

    if(!aw_rtmp_connect0(r, (struct sockaddr *)(&service)))  /* tcp传输层连接 */
    {
        CDX_LOGD("aw_rtmp_connect0  error");
        return 0;
    }
    r->bSendCounter = 1;

    return aw_rtmp_connect1(r, cp);  /* rtmp应用层三次握手 C0+C1, S0+S1+S2, C2 */
}
//************************************************************************************//
//************************************************************************************//

int aw_rtmp_is_connected(aw_rtmp_t *r)
{
    return r->sb.sbSocket != -1;
}

void aw_rtmp_packet_free(aw_rtmp_packet_t *p)
{
    if(p->pBody)
    {
        free(p->pBody - RTMP_MAX_HEADER_SIZE);
        p->pBody = NULL;
    }
}

static int aw_decode_int32le(char *data)
{
    unsigned char *c = (unsigned char *)data;
    unsigned int val;

    val = (c[3] << 24) | (c[2] << 16) | (c[1] << 8) | c[0];
    return val;
}

int aw_rtmp_packet_alloc(aw_rtmp_packet_t *p, int nSize)
{
    char *ptr = calloc(1, nSize + RTMP_MAX_HEADER_SIZE);
    if(!ptr)
    {
        return 0;
    }
    p->pBody = ptr + RTMP_MAX_HEADER_SIZE;
    p->nBytesRead = 0;
    return 1;
}

#define RTMP_LARGE_HEADER_SIZE 12

/*parse a chunk, (chunk basic header & chunk message header)*/
int aw_rtmp_read_packet(aw_rtmp_t*r, aw_rtmp_packet_t *packet)
{
    int nSize = 0;
    int hSize = 0;
    int nToRead = 0;
    int nChunk = 0;
    unsigned char hbuf[RTMP_MAX_HEADER_SIZE]= {0};
    int packetSize[4] = { 12, 8, 4, 1 };
    char *header = (char *)hbuf;

    if(ReadN(r, (char *)hbuf, 1) == 0)  /* chunk basic header */
    {
        return 0;
    }

    packet->nHeaderType = (hbuf[0] & 0xc0) >> 6;
    packet->nChannel = (hbuf[0] & 0x3f);
    header++;
    /**if chunk stream id is 0,
            the chunk stream id is represented by 2 bytes (the range of stream id is 64-319)*/
    if(packet->nChannel == 0)
    {
        if(ReadN(r, (char *)&hbuf[1], 1) != 1)
        {
            return 0;
        }
        packet->nChannel = hbuf[1]; /*chunk stream id = the second byte + 64*/
        packet->nChannel += 64;
        header++;
    }
    /**if chunk stream id is 1, the chunk stream id is represented by 3 bytes
            (the range of stream id is 64-65599)*/
    else if (packet->nChannel == 1)
    {
        int tmp;
        if(ReadN(r, (char *)&hbuf[1], 2) != 2)
        {
            return 0;
        }
        tmp = (hbuf[2] << 8) + hbuf[1];
      /* chunk stream id = the third byte*255 + the second byte+64 */
        packet->nChannel = tmp + 64;
        header += 2;
    }

    nSize = packetSize[packet->nHeaderType];

    /* Chunk basic header format 0 */
    if(nSize == RTMP_LARGE_HEADER_SIZE)    /* if we get a full header the timestamp is absolute */
    {
        packet->nHasAbsTimestamp = 1;
    }
    else if(nSize < RTMP_LARGE_HEADER_SIZE)
    {                /* using values from the last message of this channel */
        if(r->vecChannelsIn[packet->nChannel])
        {
            memcpy(packet, r->vecChannelsIn[packet->nChannel],sizeof(aw_rtmp_packet_t));
        }
    }

    nSize--;
    if(nSize > 0 && ReadN(r, header, nSize) != nSize)
    {
        return 0;
    }

    hSize = nSize + (header - (char *)hbuf);
    if(nSize >= 3)
    {
        packet->nTimeStamp = aw_amf_decode_int24(header);  /*big endium*/
        if(nSize >= 6)
        {
            packet->nBodySize = aw_amf_decode_int24(header + 3);  /*message length*/
            packet->nBytesRead = 0;
            aw_rtmp_packet_free(packet);
            if(nSize > 6)
            {
                packet->mPacketType = header[6];  /*message type id*/
                if(nSize == 11)
                {
                    packet->nInfoField2 = aw_decode_int32le(header + 7);  /* message stream id */
                }
            }
        }

        if(packet->nTimeStamp == 0xffffff)
      /*if the time stamp is the max number, need the extend time stamp*/
        {
            if(ReadN(r, header + nSize, 4) != 4) /*Extend Time stamp*/
            {
                return 0;
            }
            packet->nTimeStamp = aw_amf_decode_int32(header + nSize);
            hSize += 4;
        }
    }

    //RTMP_LogHexString(RTMP_LOGDEBUG2, (uint8_t *)hbuf, hSize);

    if(packet->nBodySize > 0 && packet->pBody == NULL)
    {
        if(!aw_rtmp_packet_alloc(packet, packet->nBodySize))
        {
            return 0;
        }
//        didAlloc = 1;
        packet->nHeaderType = (hbuf[0] & 0xc0) >> 6;
    }

    nToRead = packet->nBodySize - packet->nBytesRead;
   /*the data size of same message in different chunk */
    nChunk = r->inChunkSize;
    if(nToRead < nChunk)
    {
        nChunk = nToRead;
    }

    /* Does the caller want the raw chunk? */
    if(packet->pChunk)
    {
        packet->pChunk->nHeaderSize = hSize;
        memcpy(packet->pChunk->header, hbuf, hSize);
        packet->pChunk->chunk = packet->pBody + packet->nBytesRead;
        packet->pChunk->nChunkSize = nChunk;
    }

    if(ReadN(r, packet->pBody + packet->nBytesRead, nChunk) != nChunk)  /*read chunk data */
    {
        return 0;
    }

    packet->nBytesRead += nChunk;

    /* keep the packet as ref for other packets on this channel */
    if(!r->vecChannelsIn[packet->nChannel])
    {
        r->vecChannelsIn[packet->nChannel] = malloc(sizeof(aw_rtmp_packet_t));
    }
    memcpy(r->vecChannelsIn[packet->nChannel], packet, sizeof(aw_rtmp_packet_t));
    if(packet->nBytesRead == packet->nBodySize)
    {
        /* make packet's timestamp absolute */
        if(!packet->nHasAbsTimestamp)
        {
            packet->nTimeStamp += r->channelTimestamp[packet->nChannel];
         /* timestamps seem to be always relative!! */
        }

        r->channelTimestamp[packet->nChannel] = packet->nTimeStamp;

        /* reset the data from the stored packet.
                    we keep the header since we may use it later if a new packet for this channel */
        /* arrives and requests to re-use some info (small packet header) */
        r->vecChannelsIn[packet->nChannel]->pBody = NULL;
        r->vecChannelsIn[packet->nChannel]->nBytesRead = 0;
        r->vecChannelsIn[packet->nChannel]->nHasAbsTimestamp = 0;
      /* can only be false if we reuse header */
    }
    else
    {
        packet->pBody = NULL;    /* so it won't be erased on free */
    }
    return 1;
}

//********************************************************************************//
//*******************************************************************************//
static void aw_handle_change_chunkSize(aw_rtmp_t *r, aw_rtmp_packet_t *packet)
{
    if(packet->nBodySize >= 4)
    {
        r->inChunkSize = aw_amf_decode_int32(packet->pBody);
    }
}

/*
from http://jira.red5.org/confluence/display/docs/Ping:

Ping is the most mysterious message in RTMP and till now we haven't fully interpreted it yet.
In summary, Ping message is used as a special command that are exchanged between client and server.
This page aims to document all known Ping messages. Expect the list to grow.

The type of Ping packet is 0x4 and contains two mandatory parameters and two optional parameters.
The first parameter is the type of Ping and in short integer.
The second parameter is the target of the ping.
As Ping is always sent in Channel 2 (control channel)
and the target object in RTMP header is always 0 which means the Connection object,
it's necessary to put an extra parameter to indicate the exact target object the Ping is sent to.
The second parameter takes this responsibility.
The value has the same meaning as the target object field in RTMP header.
(The second value could also be used as other purposes,
like RTT Ping/Pong. It is used as the timestamp.)
The third and fourth parameters are optional
and could be looked upon as the parameter of the Ping packet.
Below is an unexhausted list of Ping messages.

    * this function handle the user message Event
    * type 0(stream begin): Clear the stream. No third and fourth parameters.
       The second parameter could be 0.
    *      After the connection is established, a Ping 0,0 will be sent from server to client.
    *      The message will also be sent to client on the start of Play
                  and in response of a Seek or Pause/Resume request.
    *      This Ping tells client to re-calibrate the clock with the timestamp
                  of the next packet server sends.
    * type 1(stream EOF): Tell the stream to clear the playing buffer.
    * type 3(set buffer length): Buffer time of the client.
       The third parameter is the buffer time in millisecond.
    * type 4(stream id recorded): Reset a stream.
       Used together with type 0 in the case of VOD. Often sent before type 0.
    * type 6(ping request): Ping the client from server. The second parameter is the current time.
    * type 7(ping response): Pong reply from client.
       The second parameter is the time the server sent with his ping request.
    * type 26: SWFVerification request
    * type 27: SWFVerification response
*/
int aw_rtmp_send_ctrl(aw_rtmp_t *r, short nType, unsigned int nObject, unsigned int nTime)
{
    aw_rtmp_packet_t packet;
    char pbuf[256];
    char *pend = NULL;
    char *buf = NULL;
    int nSize = 0;

    pend = pbuf + sizeof(pbuf);
    packet.nChannel = 0x02;    /* control channel (ping) */
    packet.nHeaderType = AW_RTMP_PACKET_SIZE_MEDIUM;
    packet.mPacketType = AW_RTMP_PACKET_TYPE_CONTROL;
    packet.nTimeStamp = 0;    /* RTMP_GetTime(); */
    packet.nInfoField2 = 0;
    packet.nHasAbsTimestamp = 0;
    packet.pBody = pbuf + RTMP_MAX_HEADER_SIZE;

    switch(nType)
    {
        case 0x03:
            nSize = 10;
            break;  /* buffer time */
        case 0x1A:
            nSize = 3;
            break;   /* SWF verify request */
        case 0x1B:
            nSize = 44;
            break;  /* SWF verify response */
        default:
            nSize = 6;
            break;
    }
    packet.nBodySize = nSize;
    buf = packet.pBody;
    buf = aw_amf_encode_int16(buf, pend, nType);

    if(nType == 0x1B)
    {

    }
    else if (nType == 0x1A)
    {
        *buf = nObject & 0xff;
    }
    else
    {
        if(nSize > 2)
        {
            buf = aw_amf_encode_int32(buf, pend, nObject);
        }
        if(nSize > 6)
        {
            buf = aw_amf_encode_int32(buf, pend, nTime);
        }
    }
    return aw_rtmp_send_packet(r, &packet, 0);
}

//********************************************************************//
//********************************************************************//
int aw_rtmp_send_pause(aw_rtmp_t *r, int DoPause, int iTime)
{
    aw_rtmp_packet_t packet;
    char pbuf[256];
    char *pend = NULL;
    char *enc = NULL;

    pend = pbuf + sizeof(pbuf);
    packet.nChannel = 0x08;    /* video channel */
    packet.nHeaderType = AW_RTMP_PACKET_SIZE_MEDIUM;
    packet.mPacketType = AW_RTMP_PACKET_TYPE_INVOKE;
    packet.nTimeStamp = 0;
    packet.nInfoField2 = 0;
    packet.nHasAbsTimestamp = 0;
    packet.pBody = pbuf + RTMP_MAX_HEADER_SIZE;

    enc = packet.pBody;
    enc = aw_amf_encode_string(enc, pend, &r->rtmpParam.av_pause);
    enc = aw_amf_encode_number(enc, pend, ++r->numInvokes);
    *enc++ = RTMP_AMF_NULL;
    enc = aw_amf_encode_boolean(enc, pend, DoPause);
    enc = aw_amf_encode_number(enc, pend, (double)iTime);
    packet.nBodySize = enc - packet.pBody;
    return aw_rtmp_send_packet(r, &packet, 1);
}

//********************************************************************//
//********************************************************************//

static void aw_handle_ctrl(aw_rtmp_t *r, aw_rtmp_packet_t *packet)
{
    short nType = -1;
    unsigned int tmp;

    if(packet->pBody && packet->nBodySize >= 2)
    {
        nType = aw_amf_decode_int16(packet->pBody);/* Event Type*/
    }

    if(packet->nBodySize >= 6)
    {
        switch (nType)   /*event Type*/
        {
            case 0:   /*stream begin*/
                tmp = aw_amf_decode_int32(packet->pBody + 2);
                break;
            case 1:   /*stream FOF*/
                tmp = aw_amf_decode_int32(packet->pBody + 2);
                if(r->pausing == 1)
                {
                    r->pausing = 2;
                }
                break;
            case 2:   /*stream Dry(notify the client that there is no more data on the stream)*/
                tmp = aw_amf_decode_int32(packet->pBody + 2);
                break;
            case 4:  /*Stream is recorded*/
                tmp = aw_amf_decode_int32(packet->pBody + 2);
                break;
            case 6:        /* server ping. reply with pong. */
                tmp = aw_amf_decode_int32(packet->pBody + 2);
                aw_rtmp_send_ctrl(r, 0x07, tmp, 0);
                break;
            case 31:
                tmp = aw_amf_decode_int32(packet->pBody + 2);
                if(!(r->link.lFlags & AW_RTMP_LF_BUFX))
                {
                    break;
                }
                if(!r->pausing)
                {
                    r->pauseStamp = r->channelTimestamp[r->nMediaChannel];
                    aw_rtmp_send_pause(r, 1, r->pauseStamp);
                    r->pausing = 1;
                }
                else if (r->pausing == 2)
                {
                    aw_rtmp_send_pause(r, 0, r->pauseStamp);
                    r->pausing = 3;
                }
                break;
            case 32:
                tmp = aw_amf_decode_int32(packet->pBody + 2);
                break;

            default:
                tmp = aw_amf_decode_int32(packet->pBody + 2);
                break;
        }
    }

    if(nType == 0x1A)
    {
        #if 0
        if(r->link.SWFSize)
        {
            aw_rtmp_send_ctrl(r, 0x1B, 0, 0);
        }
        #endif
    }
}

//***********************************************************************//
//***********************************************************************//
static void aw_handle_server_bW(aw_rtmp_t *r, aw_rtmp_packet_t *packet)
{
    r->nServerBW = aw_amf_decode_int32(packet->pBody);
}

//***********************************************************************//
//***********************************************************************//
static void aw_handle_client_bW(aw_rtmp_t *r, aw_rtmp_packet_t *packet)
{
    r->nClientBW = aw_amf_decode_int32(packet->pBody);
    if(packet->nBodySize > 4)
    {
        r->nClientBW2 = packet->pBody[4];
    }
    else
    {
        r->nClientBW2 = -1;
    }
}

static void aw_handle_audio(aw_rtmp_t *r, aw_rtmp_packet_t *packet)
{
    CDX_UNUSE(r);
    CDX_UNUSE(packet);

}
static void aw_handle_video(aw_rtmp_t *r, aw_rtmp_packet_t *packet)
{
    CDX_UNUSE(r);
    CDX_UNUSE(packet);

}

int aw_amf_decode_array(aw_rtmp_t*r, aw_amf_object_t*obj, const char *pBuffer, int nSize,
                 int nArrayLen, int bDecodeName)
{
    int nOriginalSize = nSize;
    int bError = 0;

    obj->o_num = 0;
    obj->o_props = NULL;

    while(nArrayLen > 0)
    {
        aw_amfobject_property_t prop;
        int nRes;
        nArrayLen--;
        nRes = aw_amf_prop_decode(r, &prop, (char*)pBuffer, nSize, bDecodeName);
        if(nRes == -1)
        {
            bError = 1;
        }
        else
        {
            nSize -= nRes;
            pBuffer += nRes;
            aw_amf_add_prop(obj, &prop);
        }
    }
    if(bError)
    {
        return -1;
    }
    return nOriginalSize - nSize;
}

void aw_amf_decode_long_string(char *data, aw_rtmp_aval_t *bv)
{
    bv->len = aw_amf_decode_int32(data);
    bv->pVal = (bv->len > 0) ? (char *)data + 4 : NULL;
}

#define AMF3_INTEGER_MAX    268435455
#define AMF3_INTEGER_MIN    -268435456

int aw_amf3_read_integer(char *data, int *valp)
{
    int i = 0;
    int val = 0;

    while(i <= 2)
    {                /* handle first 3 bytes */
        if(data[i] & 0x80)
        {            /* byte used */
            val <<= 7;        /* shift up */
            val |= (data[i] & 0x7f);    /* add bits */
            i++;
        }
        else
        {
            break;
        }
    }

    if(i > 2)
    {                /* use 4th byte, all 8bits */
        val <<= 8;
        val |= data[3];

        /* range check */
        if(val > AMF3_INTEGER_MAX)
        {
            val -= (1 << 29);
        }
    }
    else
    {                /* use 7bits of last unparsed byte (0xxxxxxx) */
        val <<= 7;
        val |= data[i];
    }
    *valp = val;
    return i > 2 ? 4 : i + 1;
}

typedef struct AMF3_CLASS_DEF
{
    char cdExternalizable;
    char cdDynamic;
    int cdNum;
    aw_rtmp_aval_t cdName;
    aw_rtmp_aval_t *cdProps;
}aw_amf3_class_def_t;

int aw_amf3_read_string(char *data, aw_rtmp_aval_t *str)
{
    int ref = 0;
    int len;
   // assert(str != 0);

    len = aw_amf3_read_integer(data, &ref);
    data += len;

    if((ref & 0x1) == 0)
    {                /* reference: 0xxx */
 //       unsigned int refIndex = (ref >> 1);
        return len;
    }
    else
    {
        unsigned int nSize = (ref >> 1);
        str->pVal = (char *)data;
        str->len = nSize;
        return len + nSize;
    }
    return len;
}

/* AMF3ClassDefinition */

void aw_amf3_class_definition_add_prop(aw_amf3_class_def_t *cd, aw_rtmp_aval_t *prop)
{
    if(!(cd->cdNum & 0x0f))
    {
        cd->cdProps = realloc(cd->cdProps, (cd->cdNum + 16) * sizeof(aw_rtmp_aval_t));
    }
    cd->cdProps[cd->cdNum++] = *prop;
}

aw_rtmp_aval_t * aw_amf3_class_definition_get_prop(aw_rtmp_t*r, aw_amf3_class_def_t *cd, int nIndex)
{
    r->rtmpParam.AV_empty.len = 0;
    r->rtmpParam.AV_empty.pVal = 0;

    if(nIndex >= cd->cdNum)
    {
        return &r->rtmpParam.AV_empty;
    }
    return &cd->cdProps[nIndex];
}

typedef enum {
    AMF3_UNDEFINED = 0, AMF3_NULL, AMF3_FALSE, AMF3_TRUE,
    AMF3_INTEGER, AMF3_DOUBLE, AMF3_STRING, AMF3_XML_DOC, AMF3_DATE,
    AMF3_ARRAY, AMF3_OBJECT, AMF3_XML, AMF3_BYTE_ARRAY
}aw_amf3_data_type;

int aw_amf3_prop_decode(aw_rtmp_t*r, aw_amfobject_property_t *prop, const char *pBuffer,
                  int nSize,int bDecodeName)
{

    int nOriginalSize = nSize;
    aw_amf3_data_type type;

    prop->name.len = 0;
    prop->name.pVal = NULL;

    if(nSize == 0 || !pBuffer)
    {
        return -1;
    }

    /* decode name */
    if(bDecodeName)
    {
        aw_rtmp_aval_t name;
        int nRes = aw_amf3_read_string((char*)pBuffer, &name);
        if(name.len <= 0)
        {
            return nRes;
        }
        prop->name = name;
        pBuffer += nRes;
        nSize -= nRes;
    }

    /* decode */
    type = *pBuffer++;
    nSize--;

    switch (type)
    {
        case AMF3_UNDEFINED:
        case AMF3_NULL:
            prop->eType = RTMP_AMF_NULL;
            break;
        case AMF3_FALSE:
            prop->eType = RTMP_AMF_BOOLEAN;
            prop->uVu.nNum = 0.0;
            break;
        case AMF3_TRUE:
            prop->eType = RTMP_AMF_BOOLEAN;
            prop->uVu.nNum = 1.0;
            break;
        case AMF3_INTEGER:
        {
            int res = 0;
            int len = aw_amf3_read_integer((char*)pBuffer, &res);
            prop->uVu.nNum = (double)res;
            prop->eType = RTMP_AMF_NUMBER;
            nSize -= len;
            break;
        }
        case AMF3_DOUBLE:
            if(nSize < 8)
            {
                return -1;
            }
            prop->uVu.nNum = aw_amf_decode_number(pBuffer);
            prop->eType = RTMP_AMF_NUMBER;
            nSize -= 8;
            break;
        case AMF3_STRING:
        case AMF3_XML_DOC:
        case AMF3_XML:
        {
            int len = aw_amf3_read_string((char*)pBuffer, &prop->uVu.aval);
            prop->eType = RTMP_AMF_STRING;
            nSize -= len;
            break;
        }
        case AMF3_DATE:
        {
            int res = 0;
            int len = aw_amf3_read_integer((char*)pBuffer, &res);
            nSize -= len;
            pBuffer += len;

            if((res & 0x1) == 0)
            {
                /* reference */
//                unsigned int nIndex = (res >> 1);
            }
            else
            {
                if(nSize < 8)
                {
                    return -1;
                }
                prop->uVu.nNum = aw_amf_decode_number(pBuffer);
                nSize -= 8;
                prop->eType = RTMP_AMF_NUMBER;
            }
            break;
        }
        case AMF3_OBJECT:
        {
            int nRes = aw_amf3_decode(r, &prop->uVu.pObject, (char*)pBuffer, nSize, 1);
            if(nRes == -1)
            {
                return -1;
            }
            nSize -= nRes;
            prop->eType = RTMP_AMF_OBJECT;
            break;
        }
        case AMF3_ARRAY:
        case AMF3_BYTE_ARRAY:
        default:
            return -1;
    }
    return nOriginalSize - nSize;
}

void aw_amf_prop_set_name(aw_amfobject_property_t *prop, aw_rtmp_aval_t *name)
{
    prop->name = *name;
}

int aw_amf3_decode(aw_rtmp_t*r, aw_amf_object_t *obj, char *pBuffer, int nSize, int bAMFData)
{
    int ref;
    int len;
    int nOriginalSize;

    nOriginalSize = nSize;
    obj->o_num = 0;
    obj->o_props = NULL;

    if(bAMFData)
    {
        pBuffer++;
        nSize--;
    }

    ref = 0;
    len = aw_amf3_read_integer(pBuffer, &ref);
    pBuffer += len;
    nSize -= len;

    if((ref & 1) == 0)
    {                /* object reference, 0xxx */
//        unsigned int  objectIndex = (ref >> 1);
    }
    else                /* object instance */
    {
        int classRef = (ref >> 1);
        aw_amf3_class_def_t cd;
        aw_amfobject_property_t prop;

        memset(&cd, 0, sizeof(aw_amf3_class_def_t));

        if((classRef & 0x1) == 0)
        {
            /* class reference */
//            unsigned int classIndex = (classRef >> 1);
        }
        else
        {
            int classExtRef = (classRef >> 1);
            int i;

            cd.cdExternalizable = (classExtRef & 0x1) == 1;
            cd.cdDynamic = ((classExtRef >> 1) & 0x1) == 1;
            cd.cdNum = classExtRef >> 2;

            /* class name */

            len = aw_amf3_read_string(pBuffer, &cd.cdName);
            nSize -= len;
            pBuffer += len;

            /*std::string str = className; */

            for(i = 0; i < cd.cdNum; i++)
            {
                aw_rtmp_aval_t  memberName;
                len = aw_amf3_read_string(pBuffer, &memberName);
                aw_amf3_class_definition_add_prop(&cd, &memberName);
                nSize -= len;
                pBuffer += len;
            }
        }
        /* add as referencable object */

        if(cd.cdExternalizable)
        {
            int nRes;
            aw_rtmp_aval_t name = AVC("DEFAULT_ATTRIBUTE");

            nRes = aw_amf3_prop_decode(r, &prop, pBuffer, nSize, 0);
            if(nRes == -1)
            {

            }
            else
            {
                nSize -= nRes;
                pBuffer += nRes;
            }
            aw_amf_prop_set_name(&prop, &name);
            aw_amf_add_prop(obj, &prop);
        }
        else
        {
            int nRes, i;
            for(i = 0; i < cd.cdNum; i++)    /* non-dynamic */
            {
                nRes = aw_amf3_prop_decode(r,&prop, pBuffer, nSize, FALSE);
                aw_amf_prop_set_name(&prop, aw_amf3_class_definition_get_prop(r, &cd, i));
                aw_amf_add_prop(obj, &prop);
                pBuffer += nRes;
                nSize -= nRes;
            }
            if(cd.cdDynamic)
            {
                int len = 0;
                do
                {
                    nRes = aw_amf3_prop_decode(r,&prop, pBuffer, nSize, TRUE);
                    aw_amf_add_prop(obj, &prop);
                    pBuffer += nRes;
                    nSize -= nRes;
                    len = prop.name.len;
                }while (len > 0);
            }
        }
    }
    return nOriginalSize - nSize;
}

int aw_amf_prop_decode(aw_rtmp_t*r, aw_amfobject_property_t *prop, char *pBuffer, int nSize,
                 int bDecodeName)
{
    int nOriginalSize = nSize;
    int nRes;

    prop->name.len = 0;
    prop->name.pVal = NULL;

    if(nSize == 0 || !pBuffer)
    {
        return -1;
    }

    if(bDecodeName && nSize < 4)
    {                /* at least name (length + at least 1 byte) and 1 byte of data */
        return -1;
    }

    if(bDecodeName)
    {
        unsigned short nNameSize = aw_amf_decode_int16(pBuffer);
        if(nNameSize > nSize - 2)
        {
            return -1;
        }
        aw_amf_decode_string(pBuffer, &prop->name);
        nSize -= 2 + nNameSize;
        pBuffer += 2 + nNameSize;
    }

    if(nSize == 0)
    {
        return -1;
    }
    nSize--;
    prop->eType = *pBuffer++;

    switch(prop->eType)
    {
        case RTMP_AMF_NUMBER:
            if(nSize < 8)
            {
                return -1;
            }
            prop->uVu.nNum = aw_amf_decode_number(pBuffer);
            nSize -= 8;
            break;
        case RTMP_AMF_BOOLEAN:
            if(nSize < 1)
            {
                return -1;
            }
            prop->uVu.nNum = (double)aw_amf_decode_boolean(pBuffer);
            nSize--;
            break;
        case RTMP_AMF_STRING:
        {
            unsigned short nStringSize = aw_amf_decode_int16(pBuffer);
            if(nSize < (long)nStringSize + 2)
            {
                return -1;
            }
            aw_amf_decode_string(pBuffer, &prop->uVu.aval);
            nSize -= (2 + nStringSize);
            break;
        }
        case RTMP_AMF_OBJECT:
        {
            int nRes = aw_amf_decode(r,&prop->uVu.pObject, pBuffer, nSize, 1);
            if(nRes == -1)
            {
                return -1;
            }
            nSize -= nRes;
            break;
        }
        case RTMP_AMF_MOVIECLIP:
        {
            return -1;
        }
        case RTMP_AMF_NULL:
        case RTMP_AMF_UNDEFINED:
        case RTMP_AMF_UNSUPPORTED:
            prop->eType = RTMP_AMF_NULL;
            break;
        case RTMP_AMF_REFERENCE:
        {
            return -1;
        }
        case RTMP_AMF_ECMA_ARRAY:
        {
            nSize -= 4;
            /* next comes the rest,
                            mixed array has a final 0x000009 mark and names, so its an object */
            nRes = aw_amf_decode(r,&prop->uVu.pObject, pBuffer + 4, nSize, 1);
            if(nRes == -1)
            {
                return -1;
            }
            nSize -= nRes;
            prop->eType = RTMP_AMF_OBJECT;
            break;
        }
        case RTMP_AMF_OBJECT_END:
        {
            return -1;
        }
        case RTMP_AMF_STRICT_ARRAY:
        {
            unsigned int nArrayLen = aw_amf_decode_int32(pBuffer);
            nSize -= 4;
            nRes = aw_amf_decode_array(r, &prop->uVu.pObject, pBuffer + 4, nSize,nArrayLen, 0);
            if(nRes == -1)
            {
                return -1;
            }
            nSize -= nRes;
            prop->eType = RTMP_AMF_OBJECT;
            break;
        }
        case RTMP_AMF_DATE:
        {
            if(nSize < 10)
            {
                return -1;
            }
            prop->uVu.nNum = aw_amf_decode_number(pBuffer);
            prop->pUTCoffset = aw_amf_decode_int16(pBuffer + 8);
            nSize -= 10;
            break;
        }
        case RTMP_AMF_LONG_STRING:
        case RTMP_AMF_XML_DOC:
        {
            unsigned int nStringSize = aw_amf_decode_int32(pBuffer);
            if(nSize < (long)nStringSize + 4)
            {
                return -1;
            }
            aw_amf_decode_long_string(pBuffer, &prop->uVu.aval);
            nSize -= (4 + nStringSize);
            if(prop->eType == RTMP_AMF_LONG_STRING)
            {
                prop->eType = RTMP_AMF_STRING;
            }
            break;
        }
        case RTMP_AMF_RECORDSET:
        {
            return -1;
        }
        case RTMP_AMF_TYPED_OBJECT:
        {
            return -1;
        }
        case RTMP_AMF_AVMPLUS:
        {
            int nRes = aw_amf3_decode(r, &prop->uVu.pObject, pBuffer, nSize, TRUE);
            if(nRes == -1)
            {
                return -1;
            }
            nSize -= nRes;
            prop->eType = RTMP_AMF_OBJECT;
            break;
        }
        default:
        {
            return -1;
        }
    }
    return nOriginalSize - nSize;
}

int aw_amf_decode(aw_rtmp_t *r, aw_amf_object_t *obj, const char *pBuffer, int nSize,
              int bDecodeName)
{
    int nOriginalSize = nSize;
    int bError = 0;
   /* if there is an error while decoding - try to at least find the end mark RTMP_AMF_OBJECT_END */

    obj->o_num = 0;
    obj->o_props = NULL;

    while(nSize > 0)
    {
        aw_amfobject_property_t prop;
        int nRes;

        if(nSize >=3 && aw_amf_decode_int24(pBuffer) == RTMP_AMF_OBJECT_END)
        {
            nSize -= 3;
            bError = 0;
            break;
        }

        if(bError)
        {
            nSize--;
            pBuffer++;
            continue;
        }

        nRes = aw_amf_prop_decode(r, &prop, (char*)pBuffer, nSize, bDecodeName);
        if(nRes == -1)
        {
            bError = 1;
        }
        else
        {
            nSize -= nRes;
            pBuffer += nRes;
            aw_amf_add_prop(obj, &prop);
        }
    }

    if(bError)
    {
        return -1;
    }
    return nOriginalSize - nSize;
}

//******************************************************************************//
//******************************************************************************//
void aw_amf_dump(aw_amf_object_t *obj)
{
    int n;
    for(n = 0; n < obj->o_num; n++)
    {
        aw_amf_prop_dump(&obj->o_props[n]);
    }
}

void aw_amf_prop_dump(aw_amfobject_property_t *prop)
{
    char strRes[256];
    char str[256];
    aw_rtmp_aval_t name;

    if(prop->eType == RTMP_AMF_INVALID)
    {
        return;
    }
    if(prop->eType == RTMP_AMF_NULL)
    {
        return;
    }

    if(prop->name.len)
    {
        name = prop->name;
    }
    else
    {
        name.pVal = "no-name.";
        name.len = sizeof("no-name.") - 1;
    }

    if(name.len > 18)
    {
        name.len = 18;
    }
    snprintf(strRes, 255, "Name: %18.*s, ", name.len, name.pVal);

    if(prop->eType == RTMP_AMF_OBJECT)
    {
        aw_amf_dump(&prop->uVu.pObject);
        return;
    }

    switch (prop->eType)
    {
        case RTMP_AMF_NUMBER:
            snprintf(str, 255, "NUMBER:\t%.2f", (double)prop->uVu.nNum);
            break;
        case RTMP_AMF_BOOLEAN:
            snprintf(str, 255, "BOOLEAN:\t%s",
            prop->uVu.nNum != 0.0 ? "TRUE" : "FALSE");
            break;
        case RTMP_AMF_STRING:
            snprintf(str, 255, "STRING:\t%.*s", prop->uVu.aval.len,
            prop->uVu.aval.pVal);
            break;
        case RTMP_AMF_DATE:
            snprintf(str, 255, "DATE:\ttimestamp: %.2f, UTC offset: %d",
            (double)prop->uVu.nNum, prop->pUTCoffset);
            break;
        default:
            snprintf(str, 255, "INVALID TYPE 0x%02x", (unsigned char)prop->eType);
     }
}

//******************************************************************************//
//******************************************************************************//
void aw_amf_prop_get_string(aw_amfobject_property_t *prop, aw_rtmp_aval_t *str)
{
    *str = prop->uVu.aval;
}
#define AVMATCH(a1,a2)    ((a1)->len == (a2)->len && !memcmp((a1)->pVal,(a2)->pVal,(a1)->len))
//static const aw_amfobject_property_t AMFProp_Invalid = { {0, 0}, RTMP_AMF_INVALID };

aw_amfobject_property_t *aw_amf_get_prop(aw_rtmp_t*r, aw_amf_object_t *obj, aw_rtmp_aval_t*name,
                                        int nIndex)
{
    r->rtmpParam.AMFProp_Invalid.name.pVal = 0;
    r->rtmpParam.AMFProp_Invalid.name.len = 0;
    r->rtmpParam.AMFProp_Invalid.eType = RTMP_AMF_INVALID;

    if(nIndex >= 0)
    {
        if(nIndex < obj->o_num)
        {
            return &obj->o_props[nIndex];
        }
    }
    else
    {
        int n;
        for (n = 0; n < obj->o_num; n++)
        {
            if(AVMATCH(&obj->o_props[n].name, name))
            {
                return &obj->o_props[n];
            }
        }
    }
    return (aw_amfobject_property_t *)&r->rtmpParam.AMFProp_Invalid;
}

int aw_amf_prop_get_number(aw_amfobject_property_t *prop)
{
    return prop->uVu.nNum;
}

//*****************************************************************************//
//****************************************************************************//
static void aw_av_erase(aw_rtmp_method_t *vals, int *num, int i, int freeit)
{
    if(freeit)
    {
        free(vals[i].name.pVal);
    }
    (*num)--;
    for(; i < *num; i++)
    {
        vals[i] = vals[i + 1];
    }
    vals[i].name.pVal = NULL;
    vals[i].name.len = 0;
    vals[i].num = 0;
}

//******************************************************************************//
//******************************************************************************//
int aw_rtmp_find_first_matching_property(aw_rtmp_t*r, aw_amf_object_t *obj, aw_rtmp_aval_t *name,
                   aw_amfobject_property_t* p)
{
    int n;
    /* this is a small object search to locate the "duration" property */
    for(n = 0; n < obj->o_num; n++)
    {
        aw_amfobject_property_t *prop = aw_amf_get_prop(r,obj, NULL, n);

        if(AVMATCH(&prop->name, name))
        {
            memcpy(p, prop, sizeof(*prop));
            return 1;
        }
        if(prop->eType == RTMP_AMF_OBJECT)
        {
            if(aw_rtmp_find_first_matching_property(r, &prop->uVu.pObject, name, p))
            {
                return 1;
            }
        }
    }
    return 0;
}

//******************************************************************************//
//******************************************************************************//
#define HEX_TO_BIN(a)    (((a)&0x40)?((a)&0xf)+9:((a)&0xf))

static void aw_decode_tea(aw_rtmp_aval_t *key, aw_rtmp_aval_t *text)
{
    int p = 0;
    int q = 0;
    int i = 0;
    int n = 0;
    unsigned char *ptr;
    unsigned char *out;
    unsigned int *v;
    unsigned int k[4] = {0};
    unsigned int u = 0;
    unsigned int z = 0;
    unsigned int y = 0;
    unsigned int sum = 0;
    unsigned int e = 0;
    unsigned int DELTA = 0x9e3779b9;

    /* prep key: pack 1st 16 chars into 4 LittleEndian ints */
    ptr = (unsigned char *)key->pVal;
    u = 0;
     n = 0;
    v = k;
    p = key->len > 16 ? 16 : key->len;

    for(i = 0; i < p; i++)
    {
        u |= ptr[i] << (n * 8);
        if(n == 3)
        {
            *v++ = u;
            u = 0;
            n = 0;
        }
        else
        {
            n++;
        }
    }
    /* any trailing chars */
    if(u)
    {
        *v = u;
    }

    /* prep text: hex2bin, multiples of 4 */
    n = (text->len + 7) / 8;
    out = malloc(n * 8);
    ptr = (unsigned char *)text->pVal;

    v = (unsigned int *) out;
    for(i = 0; i < n; i++)
    {
        u = (HEX_TO_BIN(ptr[0]) << 4) + HEX_TO_BIN(ptr[1]);
        u |= ((HEX_TO_BIN(ptr[2]) << 4) + HEX_TO_BIN(ptr[3])) << 8;
        u |= ((HEX_TO_BIN(ptr[4]) << 4) + HEX_TO_BIN(ptr[5])) << 16;
        u |= ((HEX_TO_BIN(ptr[6]) << 4) + HEX_TO_BIN(ptr[7])) << 24;
        *v++ = u;
        ptr += 8;
    }
    v = (unsigned int *) out;

    /* http://www.movable-type.co.uk/scripts/tea-block.html */
    #define MX (((z>>5)^(y<<2)) + ((y>>3)^(z<<4))) ^ ((sum^y) + (k[(p&3)^e]^z));
    z = v[n - 1];
    y = v[0];
    q = 6 + 52 / n;
    sum = q * DELTA;
    while(sum != 0)
    {
        e = sum >> 2 & 3;
        for(p=n-1; p > 0; p--)
        {
            z = v[p - 1], y = v[p] -= MX;
        }
        z = v[n - 1];
        y = v[0] -= MX;
        sum -= DELTA;
    }
    text->len /= 2;
    memcpy(text->pVal, out, text->len);
    free(out);
}

//******************************************************************************//
//******************************************************************************//
static int aw_send_secure_token_response(aw_rtmp_t *r, aw_rtmp_aval_t *resp)
{
    aw_rtmp_packet_t packet;
    char pbuf[1024];
    char *pend = NULL;
    char *enc;

    pend = pbuf + 1024;
    packet.nChannel = 0x03;    /* control channel (invoke) */
    packet.nHeaderType = AW_RTMP_PACKET_SIZE_MEDIUM;
    packet.mPacketType = AW_RTMP_PACKET_TYPE_INVOKE;
    packet.nTimeStamp = 0;
    packet.nInfoField2 = 0;
    packet.nHasAbsTimestamp = 0;
    packet.pBody = pbuf + RTMP_MAX_HEADER_SIZE;

    enc = packet.pBody;
    enc = aw_amf_encode_string(enc, pend, &r->rtmpParam.av_secureTokenResponse);
    enc = aw_amf_encode_number(enc, pend, 0.0);
    *enc++ = RTMP_AMF_NULL;
    enc = aw_amf_encode_string(enc, pend, resp);
    if(!enc)
    {
        return 0;
    }
    packet.nBodySize = enc - packet.pBody;
    return aw_rtmp_send_packet(r, &packet, 0);
}

//******************************************************************************//
//******************************************************************************//
static int aw_send_release_stream(aw_rtmp_t *r)
{
    aw_rtmp_packet_t packet;
    char pbuf[1024];
    char *pend = NULL;
    char *enc;

    pend = pbuf + sizeof(pbuf);
    packet.nChannel = 0x03;    /* control channel (invoke) */
    packet.nHeaderType = AW_RTMP_PACKET_SIZE_MEDIUM;
    packet.mPacketType = AW_RTMP_PACKET_TYPE_INVOKE;
    packet.nTimeStamp = 0;
    packet.nInfoField2 = 0;
    packet.nHasAbsTimestamp = 0;
    packet.pBody = pbuf + RTMP_MAX_HEADER_SIZE;

    enc = packet.pBody;
    enc = aw_amf_encode_string(enc, pend, &r->rtmpParam.av_releaseStream);
    enc = aw_amf_encode_number(enc, pend, ++r->numInvokes);
    *enc++ = RTMP_AMF_NULL;
    enc = aw_amf_encode_string(enc, pend, &r->link.playpath);
    if (!enc)
    {
        return 0;
    }
    packet.nBodySize = enc - packet.pBody;
    return aw_rtmp_send_packet(r, &packet, 0);
}

//******************************************************************************//
//*******************************************************************************//
static int aw_send_fc_publish(aw_rtmp_t *r)
{
    aw_rtmp_packet_t packet;
    char pbuf[1024];
    char *pend = NULL;
    char *enc;

    pend = pbuf + sizeof(pbuf);
    packet.nChannel = 0x03;    /* control channel (invoke) */
    packet.nHeaderType = AW_RTMP_PACKET_SIZE_MEDIUM;
    packet.mPacketType = AW_RTMP_PACKET_TYPE_INVOKE;
    packet.nTimeStamp = 0;
    packet.nInfoField2 = 0;
    packet.nHasAbsTimestamp = 0;
    packet.pBody = pbuf + RTMP_MAX_HEADER_SIZE;

    enc = packet.pBody;
    enc = aw_amf_encode_string(enc, pend, &r->rtmpParam.av_FCPublish);
    enc = aw_amf_encode_number(enc, pend, ++r->numInvokes);
    *enc++ = RTMP_AMF_NULL;
    enc = aw_amf_encode_string(enc, pend, &r->link.playpath);
    if(!enc)
    {
        return 0;
    }
    packet.nBodySize = enc - packet.pBody;
    return aw_rtmp_send_packet(r, &packet, 0);
}

//*****************************************************************************//
//*****************************************************************************//
int aw_rtmp_send_serverBw(aw_rtmp_t *r)
{
    aw_rtmp_packet_t packet;
    char pbuf[256];
    char *pend = pbuf + sizeof(pbuf);

    packet.nChannel = 0x02;    /* control channel (invoke) */
    packet.nHeaderType = AW_RTMP_PACKET_SIZE_LARGE;
    packet.mPacketType = AW_RTMP_PACKET_TYPE_SERVER_BW;
    packet.nTimeStamp = 0;
    packet.nInfoField2 = 0;
    packet.nHasAbsTimestamp = 0;
    packet.pBody = pbuf + RTMP_MAX_HEADER_SIZE;

    packet.nBodySize = 4;

    aw_amf_encode_int32(packet.pBody, pend, r->nServerBW);
    return aw_rtmp_send_packet(r, &packet, 0);
}

//******************************************************************************//
//******************************************************************************//

int aw_rtmp_send_create_stream(aw_rtmp_t *r)
{
    aw_rtmp_packet_t packet;
    char pbuf[256];
    char *pend = pbuf + sizeof(pbuf);
    char *enc;

    packet.nChannel = 0x03;    /* control channel (invoke) */
    packet.nHeaderType = AW_RTMP_PACKET_SIZE_MEDIUM;
    packet.mPacketType = AW_RTMP_PACKET_TYPE_INVOKE;
    packet.nTimeStamp = 0;
    packet.nInfoField2 = 0;
    packet.nHasAbsTimestamp = 0;
    packet.pBody = pbuf + RTMP_MAX_HEADER_SIZE;

    enc = packet.pBody;
    enc = aw_amf_encode_string(enc, pend, &r->rtmpParam.av_createStream);
    enc = aw_amf_encode_number(enc, pend, ++r->numInvokes);
    *enc++ = RTMP_AMF_NULL;        /* NULL */
    packet.nBodySize = enc - packet.pBody;
    return aw_rtmp_send_packet(r, &packet, 1);
}

//******************************************************************************//
//******************************************************************************//
static int aw_send_usher_token(aw_rtmp_t *r, aw_rtmp_aval_t *usherToken)
{
    aw_rtmp_packet_t packet;
    char pbuf[1024];
    char *pend = pbuf + sizeof(pbuf);
    char *enc;

    packet.nChannel = 0x03;    /* control channel (invoke) */
    packet.nHeaderType = AW_RTMP_PACKET_SIZE_MEDIUM;
    packet.mPacketType = AW_RTMP_PACKET_TYPE_INVOKE;
    packet.nTimeStamp = 0;
    packet.nInfoField2 = 0;
    packet.nHasAbsTimestamp = 0;
    packet.pBody = pbuf + RTMP_MAX_HEADER_SIZE;

    enc = packet.pBody;
    enc = aw_amf_encode_string(enc, pend, &r->rtmpParam.av_NetStream_Authenticate_UsherToken);
    enc = aw_amf_encode_number(enc, pend, ++r->numInvokes);
    *enc++ = RTMP_AMF_NULL;
    enc = aw_amf_encode_string(enc, pend, usherToken);

    if(!enc)
    {
        return 0;
    }
    packet.nBodySize = enc - packet.pBody;
    return aw_rtmp_send_packet(r, &packet, 0);
}

//******************************************************************************//
//******************************************************************************//
static int aw_send_fc_subscribe(aw_rtmp_t *r, aw_rtmp_aval_t *subscribepath)
{
    aw_rtmp_packet_t packet;
    char pbuf[512];
    char *pend = pbuf + sizeof(pbuf);
    char *enc;

    packet.nChannel = 0x03;    /* control channel (invoke) */
    packet.nHeaderType = AW_RTMP_PACKET_SIZE_MEDIUM;
    packet.mPacketType = AW_RTMP_PACKET_TYPE_INVOKE;
    packet.nTimeStamp = 0;
    packet.nInfoField2 = 0;
    packet.nHasAbsTimestamp = 0;
    packet.pBody = pbuf + RTMP_MAX_HEADER_SIZE;

    enc = packet.pBody;
    enc = aw_amf_encode_string(enc, pend, &r->rtmpParam.av_FCSubscribe);
    enc = aw_amf_encode_number(enc, pend, ++r->numInvokes);
    *enc++ = RTMP_AMF_NULL;
    enc = aw_amf_encode_string(enc, pend, subscribepath);

    if(!enc)
    {
        return 0;
    }
    packet.nBodySize = enc - packet.pBody;
    return aw_rtmp_send_packet(r, &packet, 1);
}

//******************************************************************************//
//******************************************************************************//
static int aw_send_publish(aw_rtmp_t *r)
{
    aw_rtmp_packet_t packet;
    char pbuf[1024];
    char *pend = pbuf + sizeof(pbuf);
    char *enc;

    packet.nChannel = 0x04;    /* source channel (invoke) */
    packet.nHeaderType = AW_RTMP_PACKET_SIZE_LARGE;
    packet.mPacketType = AW_RTMP_PACKET_TYPE_INVOKE;
    packet.nTimeStamp = 0;
    packet.nInfoField2 = r->nStreamId;
    packet.nHasAbsTimestamp = 0;
    packet.pBody = pbuf + RTMP_MAX_HEADER_SIZE;

    enc = packet.pBody;
    enc = aw_amf_encode_string(enc, pend, &r->rtmpParam.av_publish);
    enc = aw_amf_encode_number(enc, pend, ++r->numInvokes);
    *enc++ = RTMP_AMF_NULL;
    enc = aw_amf_encode_string(enc, pend, &r->link.playpath);
    if(!enc)
    {
        return 0;
    }

    /* FIXME: should we choose live based on link.lFlags & AW_RTMP_LF_LIVE? */
    enc = aw_amf_encode_string(enc, pend, &r->rtmpParam.av_live);
    if(!enc)
    {
        return 0;
    }
    packet.nBodySize = enc - packet.pBody;
    return aw_rtmp_send_packet(r, &packet, 1);
}

//******************************************************************************//
//******************************************************************************//
static int aw_send_playlist(aw_rtmp_t *r)
{
    aw_rtmp_packet_t packet;
    char pbuf[1024];
    char *pend = pbuf + sizeof(pbuf);
    char *enc;

    packet.nChannel = 0x08;    /* we make 8 our stream channel */
    packet.nHeaderType = AW_RTMP_PACKET_SIZE_LARGE;
    packet.mPacketType = AW_RTMP_PACKET_TYPE_INVOKE;
    packet.nTimeStamp = 0;
    packet.nInfoField2 = r->nStreamId;    /*0x01000000; */
    packet.nHasAbsTimestamp = 0;
    packet.pBody = pbuf + RTMP_MAX_HEADER_SIZE;

    enc = packet.pBody;
    enc = aw_amf_encode_string(enc, pend, &r->rtmpParam.av_set_playlist);
    enc = aw_amf_encode_number(enc, pend, 0);
    *enc++ = RTMP_AMF_NULL;
    *enc++ = RTMP_AMF_ECMA_ARRAY;
    *enc++ = 0;
    *enc++ = 0;
    *enc++ = 0;
    *enc++ = RTMP_AMF_OBJECT;
    enc = aw_amf_encode_named_string(enc, pend, (aw_rtmp_aval_t*)&r->rtmpParam.av_0,
                                   &r->link.playpath);
    if(!enc)
    {
        return 0;
    }
    if(enc + 3 >= pend)
    {
        return 0;
    }
    *enc++ = 0;
    *enc++ = 0;
    *enc++ = RTMP_AMF_OBJECT_END;
    packet.nBodySize = enc - packet.pBody;
    return aw_rtmp_send_packet(r, &packet, 1);
}

//******************************************************************************//
//******************************************************************************//
static int aw_send_play(aw_rtmp_t *r)
{
    aw_rtmp_packet_t packet;
    char pbuf[1024];
    char *pend = pbuf + sizeof(pbuf);
    char *enc;

    packet.nChannel = 0x08;    /* we make 8 our stream channel */
    packet.nHeaderType = AW_RTMP_PACKET_SIZE_LARGE;
    packet.mPacketType = AW_RTMP_PACKET_TYPE_INVOKE;
    packet.nTimeStamp = 0;
    packet.nInfoField2 = r->nStreamId;    /*0x01000000; */
    packet.nHasAbsTimestamp = 0;
    packet.pBody = pbuf + RTMP_MAX_HEADER_SIZE;

    enc = packet.pBody;
    enc = aw_amf_encode_string(enc, pend, &r->rtmpParam.av_play);
    enc = aw_amf_encode_number(enc, pend, ++r->numInvokes);
    *enc++ = RTMP_AMF_NULL;
    enc = aw_amf_encode_string(enc, pend, &r->link.playpath);
    if(!enc)
    {
        return FALSE;
    }

  /* Optional parameters start and len.
   *
   * start: -2, -1, 0, positive number
   *  -2: looks for a live stream, then a recorded stream,
   *      if not found any open a live stream
   *  -1: plays a live stream
   * >=0: plays a recorded streams from 'start' milliseconds
   */
    if(r->link.lFlags & AW_RTMP_LF_LIVE)
    {
        enc = aw_amf_encode_number(enc, pend, -1000.0);
    }
    else
    {
        if(r->link.seekTime > 0.0)
        {
            enc = aw_amf_encode_number(enc, pend, r->link.seekTime);  /* resume from here */
        }
        else
        {
            enc = aw_amf_encode_number(enc, pend, -2000.0); /*-2000.0);*/
            /* recorded as default,-2000.0 is not reliable
                            since that freezes the player if the stream is not found */
        }
    }
    if(!enc)
    {
        return 0;
    }

  /* len: -1, 0, positive number
   *  -1: plays live or recorded stream to the end (default)
   *   0: plays a frame 'start' ms away from the beginning
   *  >0: plays a live or recoded stream for 'len' milliseconds
   */
  /*enc += EncodeNumber(enc, -1.0); */ /* len */
    if(r->link.stopTime)
    {
        enc = aw_amf_encode_number(enc, pend, r->link.stopTime - r->link.seekTime);
        if(!enc)
        {
            return 0;
        }
    }
    packet.nBodySize = enc - packet.pBody;
    return aw_rtmp_send_packet(r, &packet, 1);
}

//******************************************************************************//
//******************************************************************************//
static int aw_send_checkBw(aw_rtmp_t *r)
{
    aw_rtmp_packet_t packet;
    char pbuf[256];
    char *pend = pbuf + sizeof(pbuf);
    char *enc;

    packet.nChannel = 0x03;    /* control channel (invoke) */
    packet.nHeaderType = AW_RTMP_PACKET_SIZE_LARGE;
    packet.mPacketType = AW_RTMP_PACKET_TYPE_INVOKE;
    packet.nTimeStamp = 0;    /* RTMP_GetTime(); */
    packet.nInfoField2 = 0;
    packet.nHasAbsTimestamp = 0;
    packet.pBody = pbuf + RTMP_MAX_HEADER_SIZE;

    enc = packet.pBody;
    enc = aw_amf_encode_string(enc, pend, &r->rtmpParam.av__checkbw);
    enc = aw_amf_encode_number(enc, pend, ++r->numInvokes);
    *enc++ = RTMP_AMF_NULL;
    packet.nBodySize = enc - packet.pBody;
    /* triggers _onbwcheck and eventually results in _onbwdone */
    return aw_rtmp_send_packet(r, &packet, 0);
}

//*******************************************************************************//
//*******************************************************************************//
static int aw_send_pong(aw_rtmp_t *r, double txn)
{
    aw_rtmp_packet_t packet;
    char pbuf[256];
    char *pend = pbuf + sizeof(pbuf);
    char *enc;

    packet.nChannel = 0x03;    /* control channel (invoke) */
    packet.nHeaderType = AW_RTMP_PACKET_SIZE_MEDIUM;
    packet.mPacketType = AW_RTMP_PACKET_TYPE_INVOKE;
    packet.nTimeStamp = 0x16 * r->nBWCheckCounter;    /* temp inc value. till we figure it out. */
    packet.nInfoField2 = 0;
    packet.nHasAbsTimestamp = 0;
    packet.pBody = pbuf + RTMP_MAX_HEADER_SIZE;

    enc = packet.pBody;
    enc = aw_amf_encode_string(enc, pend, &r->rtmpParam.av_pong);
    enc = aw_amf_encode_number(enc, pend, txn);
    *enc++ = RTMP_AMF_NULL;
    packet.nBodySize = enc - packet.pBody;
    return aw_rtmp_send_packet(r, &packet, 0);
}

//******************************************************************************//
//******************************************************************************//
static int aw_send_checkBw_result(aw_rtmp_t *r, double txn)
{
    aw_rtmp_packet_t packet;
    char pbuf[256];
    char *pend = pbuf + sizeof(pbuf);
    char *enc;

    packet.nChannel = 0x03;    /* control channel (invoke) */
    packet.nHeaderType = AW_RTMP_PACKET_SIZE_MEDIUM;
    packet.mPacketType = AW_RTMP_PACKET_TYPE_INVOKE;
    packet.nTimeStamp = 0x16 * r->nBWCheckCounter;    /* temp inc value. till we figure it out. */
    packet.nInfoField2 = 0;
    packet.nHasAbsTimestamp = 0;
    packet.pBody = pbuf + RTMP_MAX_HEADER_SIZE;

    enc = packet.pBody;
    enc = aw_amf_encode_string(enc, pend, &r->rtmpParam.av__result);
    enc = aw_amf_encode_number(enc, pend, txn);
    *enc++ = RTMP_AMF_NULL;
    enc = aw_amf_encode_number(enc, pend, (double)r->nBWCheckCounter++);
    packet.nBodySize = enc - packet.pBody;
    return aw_rtmp_send_packet(r, &packet, 0);
}

//*****************************************************************************//
//*****************************************************************************//
void aw_amf_prop_get_object(aw_amfobject_property_t *prop, aw_amf_object_t *obj)
{
    *obj = prop->uVu.pObject;
}

//******************************************************************************//
//******************************************************************************//
void aw_amf_reset(aw_amf_object_t *obj)
{
    int n;
    for(n = 0; n < obj->o_num; n++)
    {
        aw_amf_prop_reset(&obj->o_props[n]);
    }
    free(obj->o_props);
    obj->o_props = NULL;
    obj->o_num = 0;
}

void aw_amf_prop_reset(aw_amfobject_property_t *prop)
{
    if(prop->eType == RTMP_AMF_OBJECT)
    {
        aw_amf_reset(&prop->uVu.pObject);
    }
    else
    {
        prop->uVu.aval.len = 0;
        prop->uVu.aval.pVal = NULL;
    }
    prop->eType = RTMP_AMF_INVALID;
}

//******************************************************************************//
//******************************************************************************//
/* Returns 0 for OK/Failed/error, 1 for 'Stop or Complete' */
static int aw_handle_invoke(aw_rtmp_t *r, char *body, unsigned int nBodySize)
{
    aw_amf_object_t obj;
    aw_rtmp_aval_t method;
    int txn;
    int ret = 0;
    int nRes = 0;

    if(body[0] != 0x02)        /* make sure it is a string method name we start with */
    {
        return 0;
    }

    nRes = aw_amf_decode(r, &obj, body, nBodySize, 0);
    if(nRes < 0)
    {
        return 0;
    }
    aw_amf_dump(&obj);
    aw_amf_prop_get_string(aw_amf_get_prop(r,&obj, NULL, 0), &method);
    txn = aw_amf_prop_get_number(aw_amf_get_prop(r,&obj, NULL, 1));

    if(AVMATCH(&method, &r->rtmpParam.av__result)) /* "_result" */
    {
        aw_rtmp_aval_t methodInvoked;
        int i;
        memset(&methodInvoked, 0, sizeof(aw_rtmp_aval_t));

        for(i=0; i<r->numCalls; i++)
        {
            if(r->pMethodCalls[i].num == (int)txn)
            {
                methodInvoked = r->pMethodCalls[i].name;
                aw_av_erase(r->pMethodCalls, &r->numCalls, i, 0);
                break;
            }
        }

        if(!methodInvoked.pVal)
        {
            goto leave;
        }
        if(AVMATCH(&methodInvoked, &r->rtmpParam.av_connect))    /* Command message(connect)*/
        {
            if(r->link.token.len)
            {
                aw_amfobject_property_t p;
                if(aw_rtmp_find_first_matching_property(r,&obj,
                           (aw_rtmp_aval_t*)(&r->rtmpParam.av_secureToken), &p))
                {
                    aw_decode_tea(&r->link.token, &p.uVu.aval);
                    aw_send_secure_token_response(r, &p.uVu.aval);
                }
            }
            if(r->link.protocol & CDX_RTMP_FEATURE_WRITE)
            {
                aw_send_release_stream(r);
                aw_send_fc_publish(r);
            }
            else
            {
                aw_rtmp_send_serverBw(r);
                aw_rtmp_send_ctrl(r, 3, 0, 300);
            }
            aw_rtmp_send_create_stream(r);
            if(!(r->link.protocol & CDX_RTMP_FEATURE_WRITE))
            {
                /* Authenticate on Justin.tv legacy servers before sending FCSubscribe */
                if(r->link.usherToken.len)
                {
                    aw_send_usher_token(r, &r->link.usherToken);
                }
                /* Send the FCSubscribe if live stream or if subscribepath is set */
                if(r->link.subscribepath.len)
                {
                    aw_send_fc_subscribe(r, &r->link.subscribepath);
                }
                else if (r->link.lFlags & AW_RTMP_LF_LIVE)
                {
                    aw_send_fc_subscribe(r, &r->link.playpath);
                }
            }
        }
        else if (AVMATCH(&methodInvoked, &r->rtmpParam.av_createStream))
      /* command message (createStream)*/
        {
            r->nStreamId = (int)aw_amf_prop_get_number(aw_amf_get_prop(r,&obj, NULL, 3));
            if(r->link.protocol & CDX_RTMP_FEATURE_WRITE)
            {
                aw_send_publish(r);
            }
            else
            {
                if(r->link.lFlags & AW_RTMP_LF_PLST)
                {
                    aw_send_playlist(r);
                }
                aw_send_play(r);
                aw_rtmp_send_ctrl(r, 3, r->nStreamId, r->nBufferMS);
            }
        }
        else if(AVMATCH(&methodInvoked, &r->rtmpParam.av_play)
             ||AVMATCH(&methodInvoked, &r->rtmpParam.av_publish))
        {
            r->bPlaying = TRUE;
        }
        free(methodInvoked.pVal);
    }
    else if(AVMATCH(&method, &r->rtmpParam.av_onBWDone))  /* onBWDone */
    {
        if (!r->nBWCheckCounter)
        {
            aw_send_checkBw(r);
        }
    }
    else if(AVMATCH(&method, &r->rtmpParam.av_onFCSubscribe))
    {
      /* SendOnFCSubscribe(); */
    }
    else if(AVMATCH(&method, &r->rtmpParam.av_onFCUnsubscribe))
    {
        aw_rtmp_close(r);
        ret = 1;
    }
    else if (AVMATCH(&method, &r->rtmpParam.av_ping))
    {
        aw_send_pong(r, txn);
    }
    else if(AVMATCH(&method, &r->rtmpParam.av__onbwcheck))
    {
        aw_send_checkBw_result(r, txn);
    }
    else if(AVMATCH(&method, &r->rtmpParam.av__onbwdone))
    {
        int i;
        for (i = 0; i < r->numCalls; i++)
        if(AVMATCH(&r->pMethodCalls[i].name, &r->rtmpParam.av__checkbw))
        {
            aw_av_erase(r->pMethodCalls, &r->numCalls, i, TRUE);
            break;
        }
    }
    else if(AVMATCH(&method, &r->rtmpParam.av__error))
    {

    }
    else if(AVMATCH(&method, &r->rtmpParam.av_close))
    {
        aw_rtmp_close(r);
    }
    else if(AVMATCH(&method, &r->rtmpParam.av_onStatus))  /* "onStatus" */
    {
        aw_amf_object_t obj2;
        aw_rtmp_aval_t code, level;
        aw_amf_prop_get_object(aw_amf_get_prop(r,&obj, NULL, 3), &obj2);
        aw_amf_prop_get_string(aw_amf_get_prop(r,&obj2,
                        (aw_rtmp_aval_t*)&r->rtmpParam.av_code, -1), &code);
        aw_amf_prop_get_string(aw_amf_get_prop(r,&obj2,
                        (aw_rtmp_aval_t*)&r->rtmpParam.av_level, -1), &level);
        if(AVMATCH(&code, &r->rtmpParam.av_NetStream_Failed)
            || AVMATCH(&code, &r->rtmpParam.av_NetStream_Play_Failed)
            || AVMATCH(&code, &r->rtmpParam.av_NetStream_Play_StreamNotFound)
            || AVMATCH(&code, &r->rtmpParam.av_NetConnection_Connect_InvalidApp))
        {
            r->nStreamId = -1;
            aw_rtmp_close(r);
        }
        else if(AVMATCH(&code, &r->rtmpParam.av_NetStream_Play_Start)
           || AVMATCH(&code, &r->rtmpParam.av_NetStream_Play_PublishNotify))
        {
            int i;
            r->bPlaying = TRUE;
            for(i = 0; i < r->numCalls; i++)
            {
                if(AVMATCH(&r->pMethodCalls[i].name, &r->rtmpParam.av_play))
                {
                    aw_av_erase(r->pMethodCalls, &r->numCalls, i, TRUE);
                    break;
                }
            }
        }
        else if(AVMATCH(&code, &r->rtmpParam.av_NetStream_Publish_Start))
        {
            int i;
            r->bPlaying = TRUE;
            for(i = 0; i < r->numCalls; i++)
            {
                if(AVMATCH(&r->pMethodCalls[i].name, &r->rtmpParam.av_publish))
                {
                    aw_av_erase(r->pMethodCalls, &r->numCalls, i, TRUE);
                    break;
                }
            }
        }
        /* Return 1 if this is a Play.Complete or Play.Stop */
        else if (AVMATCH(&code, &r->rtmpParam.av_NetStream_Play_Complete)
            || AVMATCH(&code, &r->rtmpParam.av_NetStream_Play_Stop)
            || AVMATCH(&code, &r->rtmpParam.av_NetStream_Play_UnpublishNotify))
        {
            aw_rtmp_close(r);
            ret = 1;
        }
        else if (AVMATCH(&code, &r->rtmpParam.av_NetStream_Seek_Notify))
        {
            //r->read.flags &= ~CDX_RTMP_READ_SEEKING;
        }
        else if(AVMATCH(&code, &r->rtmpParam.av_NetStream_Pause_Notify))
        {
            if(r->pausing == 1 || r->pausing == 2)
            {
                aw_rtmp_send_pause(r, FALSE, r->pauseStamp);
                r->pausing = 3;
            }
        }
    }
    else if (AVMATCH(&method, &r->rtmpParam.av_playlist_ready))
    {
        int i;
        for(i = 0; i < r->numCalls; i++)
        {
            if(AVMATCH(&r->pMethodCalls[i].name, &r->rtmpParam.av_set_playlist))
            {
                aw_av_erase(r->pMethodCalls, &r->numCalls, i, TRUE);
                break;
            }
        }
    }
    else
    {

    }
leave:
    aw_amf_reset(&obj);
    return ret;
}

//***********************************************************************//
//***********************************************************************//
static int aw_dump_meta_data(aw_rtmp_t*r, aw_amf_object_t *obj)
{
    aw_amfobject_property_t *prop;
    int n;

    for(n = 0; n < obj->o_num; n++)
    {
        prop = aw_amf_get_prop(r,obj, NULL, n);
        if(prop->eType != RTMP_AMF_OBJECT)
        {
            char str[256] = "";
            switch (prop->eType)
            {
                case RTMP_AMF_NUMBER:
                    snprintf(str, 255, "%.2f", (double)prop->uVu.nNum);
                    break;
                case RTMP_AMF_BOOLEAN:
                    snprintf(str, 255, "%s",
                    prop->uVu.nNum != 0. ? "TRUE" : "FALSE");
                    break;
                case RTMP_AMF_STRING:
                    snprintf(str, 255, "%.*s", prop->uVu.aval.len,
                    prop->uVu.aval.pVal);
                    break;
                case RTMP_AMF_DATE:
                    snprintf(str, 255, "timestamp:%.2f", (double)prop->uVu.nNum);
                    break;
                default:
                    snprintf(str, 255, "INVALID TYPE 0x%02x",(unsigned char)prop->eType);
            }

            if(prop->name.len)
            {
                /* chomp */
                if(strlen(str) >= 1 && str[strlen(str) - 1] == '\n')
                {
                    str[strlen(str) - 1] = '\0';
                }
            }
        }
        else
        {
            aw_dump_meta_data(r, &prop->uVu.pObject);
        }
    }
    return 0;
}

//********************************************************************************************//
//*******************************************************************************************//
/* Like above, but only check if name is a prefix of property */
int aw_rtmp_find_prefix_property(aw_rtmp_t*r, aw_amf_object_t *obj, aw_rtmp_aval_t *name,
                   aw_amfobject_property_t* p)
{
    int n;
    for(n = 0; n < obj->o_num; n++)
    {
        aw_amfobject_property_t *prop = aw_amf_get_prop(r, obj, NULL, n);
        if(prop->name.len > name->len &&
            !memcmp(prop->name.pVal, name->pVal, name->len))
        {
            memcpy(p, prop, sizeof(aw_amfobject_property_t));
            return 1;
        }
        if(prop->eType == RTMP_AMF_OBJECT)
        {
            if(aw_rtmp_find_prefix_property(r, &prop->uVu.pObject, name, p))
            {
                return 1;
            }
        }
    }
    return 0;
}

//***********************************************************************//
//***********************************************************************//
static int aw_handle_metadata(aw_rtmp_t *r, char *body, unsigned int len)
{
    /* allright we get some info here, so parse it and print it */
     /* also keep duration or filesize to make a nice progress bar */

    aw_amf_object_t obj;
    aw_rtmp_aval_t  metastring;
    int ret = 0;

    int nRes = aw_amf_decode(r,&obj, body, len, 0);
    if(nRes < 0)
    {
        return FALSE;
    }

    aw_amf_dump(&obj);
    aw_amf_prop_get_string(aw_amf_get_prop(r,&obj, NULL, 0), &metastring);

    if(AVMATCH(&metastring, &r->rtmpParam.av_onMetaData))
    {
        aw_amfobject_property_t prop;
        /* Show metadata */
        aw_dump_meta_data(r,&obj);
        if(aw_rtmp_find_first_matching_property(r,&obj,
                              (aw_rtmp_aval_t*)&r->rtmpParam.av_duration, &prop))
        {
            r->fDuration = prop.uVu.nNum;
        }
        /* Search for audio or video tags */
        if(aw_rtmp_find_prefix_property(r, &obj,
                                 (aw_rtmp_aval_t*)&r->rtmpParam.av_video, &prop))
        {
            r->read.dataType |= 1;
        }
        if(aw_rtmp_find_prefix_property(r, &obj,
                                 (aw_rtmp_aval_t*)&r->rtmpParam.av_audio, &prop))
        {
            r->read.dataType |= 4;
        }
        ret = TRUE;
    }
    aw_amf_reset(&obj);
    return ret;
}

//***********************************************************************//
//***********************************************************************//
int aw_rtmp_client_packet(aw_rtmp_t*r, aw_rtmp_packet_t *packet)
{
    int bHasMediaPacket = 0;

    switch(packet->mPacketType)  /*message(AMF) type id*/
    {
        case AW_RTMP_PACKET_TYPE_CHUNK_SIZE:       /* set chunk size */
            /* chunk size */
            aw_handle_change_chunkSize(r, packet);
            break;
        case AW_RTMP_PACKET_TYPE_BYTES_READ_REPORT:   /*acknowledge message*/
             /* bytes read report */
            break;
        case AW_RTMP_PACKET_TYPE_CONTROL:     /*use control message*/
            /* ctrl */
            aw_handle_ctrl(r, packet);
            break;
        case AW_RTMP_PACKET_TYPE_SERVER_BW:
         /* spec descrip it window acknownledge size, but code is not*/
            /* server bw */
            aw_handle_server_bW(r, packet);
            break;
        case AW_RTMP_PACKET_TYPE_CLIENT_BW:     /* "set peer banwith" */
            /* client bw */
            aw_handle_client_bW(r, packet);
            break;
        case AW_RTMP_PACKET_TYPE_AUDIO:
            /* audio data */
            aw_handle_audio(r, packet);
            bHasMediaPacket = 1;
            if(!r->nMediaChannel)
            {
                r->nMediaChannel = packet->nChannel;
            }
            if(!r->pausing)
            {
                r->mediaStamp = packet->nTimeStamp;
            }
            break;
        case AW_RTMP_PACKET_TYPE_VIDEO:
            /* video data */
            aw_handle_video(r, packet);
            bHasMediaPacket = 1;
            if(!r->nMediaChannel)
            {
                r->nMediaChannel = packet->nChannel;
            }
            if(!r->pausing)
            {
                r->mediaStamp = packet->nTimeStamp;
            }
            break;
        case AW_RTMP_PACKET_TYPE_FLEX_STREAM_SEND:
            /* flex stream send */
            break;

        case AW_RTMP_PACKET_TYPE_FLEX_SHARED_OBJECT:
            /* flex shared object */
            break;
        case AW_RTMP_PACKET_TYPE_FLEX_MESSAGE:
            /* flex message */

            if(aw_handle_invoke(r, packet->pBody + 1, packet->nBodySize - 1) == 1)
            {
                bHasMediaPacket = 2;
            }
            break;
        case AW_RTMP_PACKET_TYPE_INFO:
            /* metadata (notify) */
            if(aw_handle_metadata(r, packet->pBody, packet->nBodySize))
            {
                bHasMediaPacket = 1;
            }
            break;
        case AW_RTMP_PACKET_TYPE_SHARED_OBJECT:
            break;
        case AW_RTMP_PACKET_TYPE_INVOKE:              /* user control message */

            /* invoke */
            if(aw_handle_invoke(r, packet->pBody, packet->nBodySize) == 1)
            {
                bHasMediaPacket = 2;
            }
            break;
        case AW_RTMP_PACKET_TYPE_FLASH_VIDEO:  /* Aggregate Message */
        {
            /* go through FLV packets and handle metadata packets */
            unsigned int pos = 0;
            unsigned int nTimeStamp;

            nTimeStamp = packet->nTimeStamp;
            while (pos + 11 < packet->nBodySize)
            {
                unsigned int dataSize = aw_amf_decode_int24(packet->pBody + pos + 1);
            /* size without header (11) and prevTagSize (4) */

                if(pos + 11 + dataSize + 4 > packet->nBodySize)
                {
                    break;
                }
                if(packet->pBody[pos] == 0x12)
                {
                    aw_handle_metadata(r, packet->pBody + pos + 11, dataSize);
                }
                else if (packet->pBody[pos] == 8 || packet->pBody[pos] == 9)
                {
                    nTimeStamp = aw_amf_decode_int24(packet->pBody + pos + 4);
                    nTimeStamp |= (packet->pBody[pos + 7] << 24);
                }
                pos += (11 + dataSize + 4);
            }
            if(!r->pausing)
            {
                r->mediaStamp = nTimeStamp;
            }
            bHasMediaPacket = 1;
            break;
        }
        default:
        {
            break;
        }
    }
    return bHasMediaPacket;
}

//********************************************************************//
//********************************************************************//
int aw_rtmp_connect_stream(aw_rtmp_t *r, int seekTime)
{
    aw_rtmp_packet_t packet;

    /* seekTime was already set by SetupStream / SetupURL.
     * This is only needed by ReconnectStream.
    */
    memset(&packet, 0, sizeof(aw_rtmp_packet_t));
    if(seekTime > 0)
    {
        r->link.seekTime = seekTime;
    }
    r->nMediaChannel = 0;
    int ret;

    // parse the packet header and chunk msg header,
    //then deal with the packet according to the packet type
    while(!r->bPlaying && aw_rtmp_is_connected(r) && (ret = aw_rtmp_read_packet(r, &packet)))
    {
        if(packet.nBytesRead == packet.nBodySize)
        {
            if(!packet.nBodySize)
            {
                continue;
            }
            if ((packet.mPacketType == AW_RTMP_PACKET_TYPE_AUDIO) ||
                (packet.mPacketType == AW_RTMP_PACKET_TYPE_VIDEO) ||
                (packet.mPacketType == AW_RTMP_PACKET_TYPE_INFO))
            {
                aw_rtmp_packet_free(&packet);
                continue;
            }
            aw_rtmp_client_packet(r, &packet);
            aw_rtmp_packet_free(&packet);
        }
    }
//    printf("aw_rtmp_connect_stream    end\n");
    return r->bPlaying;
}

//*****************************************************************************//
//*****************************************************************************//
static int aw_send_fc_unpublish(aw_rtmp_t *r)
{
    aw_rtmp_packet_t packet;
    char pbuf[1024];
    char *pend = pbuf + sizeof(pbuf);
    char *enc;

    packet.nChannel = 0x03;    /* control channel (invoke) */
    packet.nHeaderType = AW_RTMP_PACKET_SIZE_MEDIUM;
    packet.mPacketType = AW_RTMP_PACKET_TYPE_INVOKE;
    packet.nTimeStamp = 0;
    packet.nInfoField2 = 0;
    packet.nHasAbsTimestamp = 0;
    packet.pBody = pbuf + RTMP_MAX_HEADER_SIZE;

    enc = packet.pBody;
    enc = aw_amf_encode_string(enc, pend, &r->rtmpParam.av_FCUnpublish);
    enc = aw_amf_encode_number(enc, pend, ++r->numInvokes);
    *enc++ = RTMP_AMF_NULL;
    enc = aw_amf_encode_string(enc, pend, &r->link.playpath);
    if(!enc)
    {
        return 0;
    }
    packet.nBodySize = enc - packet.pBody;
    return aw_rtmp_send_packet(r, &packet, 0);
}

static int aw_send_delete_stream(aw_rtmp_t *r, double dStreamId)
{
    aw_rtmp_packet_t packet;
    char pbuf[256];
    char *pend = pbuf + sizeof(pbuf);
    char *enc;

    packet.nChannel = 0x03;    /* control channel (invoke) */
    packet.nHeaderType = AW_RTMP_PACKET_SIZE_MEDIUM;
    packet.mPacketType = AW_RTMP_PACKET_TYPE_INVOKE;
    packet.nTimeStamp = 0;
    packet.nInfoField2 = 0;
    packet.nHasAbsTimestamp = 0;
    packet.pBody = pbuf + RTMP_MAX_HEADER_SIZE;

    enc = packet.pBody;
    enc = aw_amf_encode_string(enc, pend, &r->rtmpParam.av_deleteStream);
    enc = aw_amf_encode_number(enc, pend, ++r->numInvokes);
    *enc++ = RTMP_AMF_NULL;
    enc = aw_amf_encode_number(enc, pend, dStreamId);
    packet.nBodySize = enc - packet.pBody;
    /* no response expected */
    return aw_rtmp_send_packet(r, &packet, 0);
}

int aw_send_seek(aw_rtmp_t *r, int64_t iTime)
{
    aw_rtmp_packet_t packet;
    char pbuf[256];
    char *pend = pbuf + sizeof(pbuf);
    char *enc;

    packet.nChannel = 0x08;    /* video channel */
    packet.nHeaderType = AW_RTMP_PACKET_SIZE_MEDIUM;
    packet.mPacketType = AW_RTMP_PACKET_TYPE_INVOKE;
    packet.nTimeStamp = 0;
    packet.nInfoField2 = 0;
    packet.nHasAbsTimestamp = 0;
    packet.pBody = pbuf + RTMP_MAX_HEADER_SIZE;

    enc = packet.pBody;
    enc = aw_amf_encode_string(enc, pend, &r->rtmpParam.av_seek);
    enc = aw_amf_encode_number(enc, pend, ++r->numInvokes);
    *enc++ = RTMP_AMF_NULL;
    enc = aw_amf_encode_number(enc, pend, (double)iTime);

    packet.nBodySize = enc - packet.pBody;

    r->read.flags |= CDX_RTMP_READ_SEEKING;
    r->read.nResumeTS = 0;

    return aw_rtmp_send_packet(r, &packet, TRUE);
}

int aw_rtmp_sock_buf_close(aw_rtmp_socket_buf_t* sb)
{
    if(sb->sbSocket != -1)
    {
        return closesocket(sb->sbSocket);
    }
    return 0;
}

static void aw_av_clear(aw_rtmp_method_t *vals, int num)
{
    int i;
    for(i = 0; i < num; i++)
    {
        free(vals[i].name.pVal);
    }
    free(vals);
}

void aw_rtmp_close(aw_rtmp_t *r)
{
    int i;
    #define CDX_RTMP_READ_HEADER    0x01

    if(!r)
    {
        return;
    }

    if(aw_rtmp_is_connected(r))
    {
        if(r->nStreamId > 0)
        {
            i = r->nStreamId;
            r->nStreamId = 0;
            if((r->link.protocol & CDX_RTMP_FEATURE_WRITE))
            {
                aw_send_fc_unpublish(r);
            }
            aw_send_delete_stream(r, i);
        }

        if(r->clientID.pVal)
        {
            aw_rtmp_http_post(r, RTMPT_CLOSE, "", 1);
            free(r->clientID.pVal);
            r->clientID.pVal = NULL;
            r->clientID.len = 0;
        }
        aw_rtmp_sock_buf_close(&r->sb);
    }

    r->nStreamId = -1;
    r->sb.sbSocket = -1;
    r->nBWCheckCounter = 0;
    r->nBytesIn = 0;
    r->nBytesInSent = 0;

    r->read.dataType = 0;
    r->read.flags = 0;
    r->read.nResumeTS = 0;
    r->write.nBytesRead = 0;
    aw_rtmp_packet_free(&r->write);

    for(i = 0; i < RTMP_CHANNELS; i++)
    {
        if(r->vecChannelsIn[i])
        {
            aw_rtmp_packet_free(r->vecChannelsIn[i]);
            free(r->vecChannelsIn[i]);
            r->vecChannelsIn[i] = NULL;
        }
        if(r->vecChannelsOut[i])
        {
            free(r->vecChannelsOut[i]);
            r->vecChannelsOut[i] = NULL;
        }
    }
    aw_av_clear(r->pMethodCalls, r->numCalls);
    r->pMethodCalls = NULL;
    r->numCalls = 0;
    r->numInvokes = 0;

    r->bPlaying = 0;
    r->sb.sbSize = 0;

    r->mMsgCounter = 0;
    r->resplen = 0;
    r->unackd = 0;

    free(r->link.playpath0.pVal);
    r->link.playpath0.pVal = NULL;

    if(r->link.lFlags & AW_RTMP_LF_FTCU)
    {
        free(r->link.tcUrl.pVal);
        r->link.tcUrl.pVal = NULL;
        r->link.lFlags ^= AW_RTMP_LF_FTCU;
    }
}

