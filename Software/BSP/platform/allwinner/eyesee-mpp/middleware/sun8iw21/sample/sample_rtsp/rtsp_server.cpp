/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     :
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2016/11/4
  Last Modified :
  Description   :
  Function List :
  History       :
******************************************************************************/

//#include <time.h>
//#include <sys/types.h>
//#include <sys/stat.h>
//#include <fcntl.h>

#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <string.h>

#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include "MediaStream.h"
#include "TinyServer.h"

#include "rtsp_server.h"

#define RTSP_SERVER_CHANNEL_NUM  (2)

static TinyServer *gpRtspContext[RTSP_SERVER_CHANNEL_NUM];
static MediaStream *gpRtspStream[RTSP_SERVER_CHANNEL_NUM];

static int get_net_dev_ip(const char *netdev_name, char *ip)
{
    int            fd     = -1;
    unsigned int   u32ip  =  0;
    struct ifreq   ifr;
    struct in_addr sin_addr;

    if (NULL == netdev_name || NULL == ip) {
        printf("%s,%d: Input netdev_name or ip is NULL!\n", __func__, __LINE__);
        return -1;
    }

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd <= 0) {
        printf("%s,%d: Fail to create socket! errno[%d] errinfo[%s]\n", __func__, __LINE__,
                errno, strerror(errno));
        return -1;
    }

    memset(&ifr, 0, sizeof(struct ifreq));
    strncpy(ifr.ifr_name, netdev_name, sizeof(ifr.ifr_name));
    ifr.ifr_addr.sa_family = AF_INET;
    if (ioctl(fd, SIOCGIFADDR, &ifr) < 0) {
        printf("%s,%d: Fail to ioctl SIOCGIFADDR. devname[%s] errno[%d] errinfo[%s]\n", __func__, __LINE__,
                netdev_name, errno, strerror(errno));
        close(fd);
        return -1;
    }

    close(fd);

    u32ip = ntohl(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr.s_addr);

    sin_addr.s_addr = htonl(u32ip);
    sprintf(ip, "%s", inet_ntoa(sin_addr));

    return 0;
}

int rtsp_open(int id, RtspServerAttr *rtsp_attr)
{
    int ret  = 0;
    int port = 8554;
    char net_name[32] = {0};
    char ip[64] = {0};
    std::string ip_addr;

    if (id >= RTSP_SERVER_CHANNEL_NUM)
    {
        printf("%s,%d: invalid id %d >= %d\n", __func__,__LINE__, id, RTSP_SERVER_CHANNEL_NUM);
        return -1;
    }

    if (RTSP_NET_TYPE_LO == rtsp_attr->net_type)
        strncpy(net_name, "lo", 31);
    else if (RTSP_NET_TYPE_ETH0 == rtsp_attr->net_type)
        strncpy(net_name, "eth0", 31);
    else if (RTSP_NET_TYPE_BR0 == rtsp_attr->net_type)
        strncpy(net_name, "br0", 31);
    else if (RTSP_NET_TYPE_WLAN0 == rtsp_attr->net_type)
        strncpy(net_name, "wlan0", 31);
    else
        printf("%s,%d: invalid net type %d\n", __func__,__LINE__, rtsp_attr->net_type);

    // get ip addr and create server
    ret = get_net_dev_ip(net_name, ip);
    if (ret)
    {
        printf("%s get_net_dev_ip fail! ret:%d\n", net_name, ret);
        return -1;
    }
    printf("%s ip:%s \n", net_name, ip);

    ip_addr = ip;
    TinyServer *rtsp = TinyServer::createServer(ip_addr, port);
    if (NULL == rtsp)
    {
        printf("%s,%d: rtsp create server failed!\n", __func__,__LINE__);
        return -1;
    }
     gpRtspContext[id] = rtsp;

    // create Media Stream
    MediaStream::MediaStreamAttr attr;

    if (RTSP_VIDEO_TYPE_H264 == rtsp_attr->video_type)
        attr.videoType  = MediaStream::MediaStreamAttr::VIDEO_TYPE_H264;
    else if (RTSP_VIDEO_TYPE_H265 == rtsp_attr->video_type)
        attr.videoType  = MediaStream::MediaStreamAttr::VIDEO_TYPE_H265;
    else
        printf("%s,%d: invalid video type %d\n", __func__,__LINE__, rtsp_attr->video_type);

    if (RTSP_AUDIO_TYPE_AAC == rtsp_attr->audio_type)
        attr.audioType  = MediaStream::MediaStreamAttr::AUDIO_TYPE_AAC;
    else
        printf("%s,%d: invalid audio type %d\n", __func__,__LINE__, rtsp_attr->audio_type);

    if (RTSP_STREAM_TYPE_UNICAST == rtsp_attr->stream_type)
        attr.streamType = MediaStream::MediaStreamAttr::STREAM_TYPE_UNICAST;
    else if (RTSP_STREAM_TYPE_MULTICAST == rtsp_attr->stream_type)
        attr.streamType = MediaStream::MediaStreamAttr::STREAM_TYPE_MULTICAST;
    else
        printf("%s,%d: invalid stream type %d\n", __func__,__LINE__, rtsp_attr->stream_type);

    char ch_str[32] = {0};
    sprintf(ch_str, "ch%d", id);
    MediaStream *stream = rtsp->createMediaStream(ch_str, attr);
    if (NULL == stream)
    {
        printf("%s,%d: rtsp create media stream failed!\n", __func__,__LINE__);
        return -1;
    }
    gpRtspStream[id] = stream;

    printf("============================================================\n");
    printf("   rtsp://%s:%d/%s  \n", ip_addr.c_str(), port, ch_str);
    printf("============================================================\n");

    return 0;
}

void rtsp_close(int id)
{
    if (id >= RTSP_SERVER_CHANNEL_NUM)
    {
        printf("%s,%d: invalid id %d >= %d\n", __func__,__LINE__, id, RTSP_SERVER_CHANNEL_NUM);
        return;
    }
    printf("close rtsp id %d\n", id);
}

void rtsp_start(int id)
{
    if (id >= RTSP_SERVER_CHANNEL_NUM)
    {
        printf("%s,%d: invalid id %d >= %d\n", __func__,__LINE__, id, RTSP_SERVER_CHANNEL_NUM);
        return;
    }
    TinyServer *rtsp = gpRtspContext[id];
    MediaStream *stream = gpRtspStream[id];

    std::string url_ = stream->streamURL();
    rtsp->runWithNewThread();
}

void rtsp_stop(int id)
{
    if (id >= RTSP_SERVER_CHANNEL_NUM)
    {
        printf("%s,%d: invalid id %d >= %d\n", __func__,__LINE__, id, RTSP_SERVER_CHANNEL_NUM);
        return;
    }
    TinyServer *rtsp = gpRtspContext[id];
    rtsp->stop();
}

void rtsp_sendData(int id, RtspSendDataParam *param)
{
    if (id >= RTSP_SERVER_CHANNEL_NUM)
    {
        printf("%s,%d: invalid id %d >= %d\n", __func__,__LINE__, id, RTSP_SERVER_CHANNEL_NUM);
        return;
    }
    if (NULL == param)
    {
        printf("%s,%d: invalid param!\n", __func__,__LINE__);
        return;
    }

    MediaStream *stream = gpRtspStream[id];
    if (stream != NULL)
    {
        if (RTSP_FRAME_DATA_TYPE_I == param->frame_type)
        {
            stream->appendVideoData(param->buf, param->size, param->pts, MediaStream::FRAME_DATA_TYPE_I);
        }
        else if (RTSP_FRAME_DATA_TYPE_P == param->frame_type)
        {
            stream->appendVideoData(param->buf, param->size, param->pts, MediaStream::FRAME_DATA_TYPE_P);
        }
        else
        {
            printf("rtsp id %d, frame_type %d is NOT support!\n", id, param->frame_type);
        }
    }
}

