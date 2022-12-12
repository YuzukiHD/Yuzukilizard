#ifndef __RTSP_SERVER_H__
#define __RTSP_SERVER_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef enum RTSP_NET_TYPE
{
    RTSP_NET_TYPE_LO,
    RTSP_NET_TYPE_ETH0,
    RTSP_NET_TYPE_BR0,
    RTSP_NET_TYPE_WLAN0,
    RTSP_NET_TYPE_LAST
} RTSP_NET_TYPE;

typedef enum RTSP_STREAM_TYPE
{
    RTSP_STREAM_TYPE_UNICAST,
    RTSP_STREAM_TYPE_MULTICAST,
    RTSP_STREAM_TYPE_LAST
} RTSP_STREAM_TYPE;

typedef enum RTSP_VIDEO_TYPE
{
    RTSP_VIDEO_TYPE_H264,
    RTSP_VIDEO_TYPE_H265,
    RTSP_VIDEO_TYPE_LAST
} RTSP_VIDEO_TYPE;

typedef enum RTSP_AUDIO_TYPE
{
    RTSP_AUDIO_TYPE_AAC,
    RTSP_AUDIO_TYPE_LAST
} RTSP_AUDIO_TYPE;

typedef enum RTSP_FRAME_DATA_TYPE
{
    RTSP_FRAME_DATA_TYPE_I,
    RTSP_FRAME_DATA_TYPE_B,
    RTSP_FRAME_DATA_TYPE_P,
    RTSP_FRAME_DATA_TYPE_HEADER,
    RTSP_FRAME_DATA_TYPE_UNKNOW,
    RTSP_FRAME_DATA_TYPE_LAST
} RTSP_FRAME_DATA_TYPE;

typedef struct RtspServerAttr
{
    RTSP_NET_TYPE net_type;
    RTSP_STREAM_TYPE stream_type;
    RTSP_VIDEO_TYPE video_type;
    RTSP_AUDIO_TYPE audio_type;
} RtspServerAttr;

typedef struct RtspSendDataParam
{
    unsigned char *buf;
    unsigned int size;
    RTSP_FRAME_DATA_TYPE frame_type;
    uint64_t pts;
} RtspSendDataParam;

int rtsp_open(int id, RtspServerAttr *rtsp_attr);
void rtsp_close(int id);
void rtsp_start(int id);
void rtsp_stop(int id);
void rtsp_sendData(int id, RtspSendDataParam *param);

#ifdef __cplusplus
}
#endif

#endif // __RTSP_SERVER_H__

