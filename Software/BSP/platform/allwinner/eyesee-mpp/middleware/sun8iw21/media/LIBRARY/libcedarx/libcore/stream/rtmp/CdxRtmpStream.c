/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxRtmpStream.c
 * Description : RtmpStream
 * History :
 *
 */

#include <cdx_log.h>
#include "CdxRtmpStream.h"
#include <SmartDnsService.h>

#define URL_RDONLY 0
#define URL_WRONLY 1
#define URL_RDWR   2

enum RtmpStreamStateE
{
    RTMP_STREAM_IDLE          = 0x00L,
    RTMP_STREAM_CONNECTING    = 0x01L,
    RTMP_STREAM_READING       = 0x02L,
    RTMP_STREAM_SEEKING       = 0x03L,
    RTMP_STREAM_FORCESTOPPED  = 0x04L,
};

enum {STRING=0, INT, BOOL, CONN};

#if 0 //not use
static const char *optinfo[]=
{
    "string", "integer", "boolean", "AMF"
};
#endif

#define offsetof_t(T, F) ((size_t)((char *)&((T *)0)->F))
#define OFFSET(t)    offsetof_t(aw_rtmp_t,t)
#define AVCSTR(str)    {str,sizeof(str)-1}

#define OSS    "GNU"

#define DEFAULT_VERSTR    OSS " 10,0,32,18"
#define RTMP_LF_AUTH    0x0001    /* using auth param */
#define RTMP_LF_LIVE    0x0002    /* stream is live */
#define RTMP_LF_SWFV    0x0004    /* do SWF verification */
#define RTMP_LF_PLST    0x0008    /* send playlist before play */
#define RTMP_LF_BUFX    0x0010    /* toggle stream on BufferEmpty msg */
#define RTMP_LF_FTCU    0x0020    /* free tcUrl on close */

static struct aw_rtmp_urlopt
{
    aw_rtmp_aval_t name;
    unsigned int off;
    int otype;
    int miscType;
    char *use;
} options[19] =
{
  { AVCSTR("socks"),     OFFSET(link.sockshost),     STRING,  0,"Use the specified socket proxy" },
  { AVCSTR("app"),       OFFSET(link.app),           STRING,  0,"Name of target app on server" },
  { AVCSTR("tcUrl"),     OFFSET(link.tcUrl),         STRING,  0,"URL of played stream" },
  { AVCSTR("pageUrl"),   OFFSET(link.pageUrl),       STRING,  0,"URL of played media's web page" },
  { AVCSTR("swfUrl"),    OFFSET(link.swfUrl),        STRING,  0,"URL of played swf file" },
  { AVCSTR("flashver"),  OFFSET(link.flashVer),
     STRING,  0,"Flash version string (default " DEFAULT_VERSTR ")" },
  { AVCSTR("conn"),      OFFSET(link.extras),
    CONN, 0,"Append arbitrary AMF data to Connect message" },
  { AVCSTR("playpath"),  OFFSET(link.playpath),      STRING,  0,"Path of target media on server" },
  { AVCSTR("playlist"),  OFFSET(link.lFlags),
    BOOL, RTMP_LF_PLST, "Set playlist before call play command" },
  { AVCSTR("live"),      OFFSET(link.lFlags),
    BOOL, RTMP_LF_LIVE, "Stream is live, unseekable" },
  { AVCSTR("subscribe"), OFFSET(link.subscribepath), STRING,  0,"Stream to subscribe to" },
  { AVCSTR("jtv"),       OFFSET(link.usherToken),    STRING,  0,"Justin tv authentication token" },
  { AVCSTR("token"),     OFFSET(link.token),          STRING,  0,"Key for Secure Token response" },
  { AVCSTR("swfVfy"),    OFFSET(link.lFlags),
    BOOL, RTMP_LF_SWFV, "Perform swf Verification" },
  { AVCSTR("swfAge"),    OFFSET(link.swfAge),
    INT,  0,"Number of day to use cached SWF hash" },
  { AVCSTR("start"),     OFFSET(link.seekTime),      INT,  0,"Stream start position (ms)" },
  { AVCSTR("stop"),      OFFSET(link.stopTime),      INT,  0,"Stream stop position in ms" },
  { AVCSTR("buffer"),    OFFSET(nBufferMS),        INT,  0,"Buffer time in ms" },
  { AVCSTR("timeout"),   OFFSET(link.timeout),       INT,  0,"Session timeout (second)" }
};

static const aw_rtmp_aval_t truth[] = {
    AVCSTR("1"),
    AVCSTR("on"),
    AVCSTR("yes"),
    AVCSTR("true"),
    {0,0}
};

const char RTMPProtocolStringsLower[][7] =
{
  "rtmp",
  "rtmpt",
  "rtmpe",
  "rtmpte",
  "rtmps",
  "rtmpts",
  "",
  "",
  "rtmfp"
};

#define RTMP_DEFAULT_CHUNKSIZE    128
#define RTMP_PROBE_DATA_LEN 1024

extern void aw_rtmp_close(aw_rtmp_t *rtmp);
extern int aw_send_seek(aw_rtmp_t *rtmp, int64_t iTime);
extern void aw_set_rtmp_parameter(aw_rtmp_t* rtmp);
extern int aw_rtmp_is_connected(aw_rtmp_t *rtmp);
extern int aw_rtmp_read_packet(aw_rtmp_t*rtmp, aw_rtmp_packet_t *packet);
extern int aw_rtmp_client_packet(aw_rtmp_t*rtmp, aw_rtmp_packet_t *packet);
extern void aw_rtmp_packet_free(aw_rtmp_packet_t *p);
extern char *aw_amf_encode_int24(char *output, char *outend, int nVal);
extern unsigned int aw_amf_decode_int24(const char *data);
extern unsigned int aw_amf_decode_int32(const char *data);
extern char *aw_amf_encode_int32(char *output, char *outend, int nVal);
extern int aw_rtmp_connect(aw_rtmp_t *rtmp, aw_rtmp_packet_t *cp);
extern int aw_rtmp_connect_stream(aw_rtmp_t *rtmp, int seekTime);

//***************************************************************************************//
//***************************************************************************************//
/*
 * Extracts playpath from RTMP URL. playpath is the file part of the
 * URL, i.e. the part that comes after rtmp://host:port/app/
 *
 * Returns the stream name in a format understood by FMS. The name is
 * the playpath part of the URL with formatting depending on the stream
 * type:
 *
 * mp4 streams: prepend "mp4:", remove extension
 * mp3 streams: prepend "mp3:", remove extension
 * flv streams: remove extension
 */
void aw_rtmp_parse_playpath(aw_rtmp_aval_t *pIn, aw_rtmp_aval_t *pOut)
{
    int mp4Ext = 0;
    int mp3Ext = 0;
    int addSubExt = 0;
    char *playpath = NULL;
    char *temp = NULL;
    char *q = NULL;
    char *pExt = NULL;  /*后缀名*/
    char *pListStart = NULL;
    char *pStreamUrl = NULL;
    char *pDest = NULL;
    char *p = NULL;
    int listLen  = 0;
    unsigned int a;

    pOut->pVal = NULL;
    pOut->len = 0;
    listLen = pIn->len;
    playpath = pIn->pVal;
    pListStart = playpath;

    /*   .mp4?XXX=XXX&XXX=XXX    */
    if(((temp=strstr(pListStart, "slist=")) != 0) && (*pListStart == '?'))
    {
        pListStart = temp+6;
        listLen = strlen(pListStart);
        temp = strchr(pListStart, '&');
        if(temp)
        {
            listLen = temp-pListStart;
        }
    }

    q = strchr(pListStart, '?');
    if(listLen >= 4)
    {
        if(q)
        {
            pExt = q-4;
        }
        else
        {
            pExt = &pListStart[listLen-4];
        }
        if((strncmp(pExt, ".f4v", 4) == 0) ||(strncmp(pExt, ".mp4", 4) == 0))
        {
            mp4Ext = 1;
            addSubExt = 1;
            /* Only remove .flv from rtmp URL, not slist params */
        }
        else if ((pListStart == playpath) &&(strncmp(pExt, ".flv", 4) == 0))
        {
            addSubExt = 1;
        }
        else if(strncmp(pExt, ".mp3", 4) == 0)
        {
            mp3Ext = 1;
            addSubExt = 1;
        }
    }

    pStreamUrl = (char *)malloc((listLen+4+1)*sizeof(char));
    if (!pStreamUrl)
    {
        return;
    }

    pDest = pStreamUrl;
    if(mp4Ext)
    {
        if(strncmp(pListStart, "mp4:", 4))
        {
            strcpy(pDest, "mp4:");
            pDest += 4;
        }
        else
        {
            addSubExt = 0;
        }
    }
    else if(mp3Ext)
    {
        if(strncmp(pListStart, "mp3:", 4))
        {
            strcpy(pDest, "mp3:");
            pDest += 4;
        }
        else
        {
            addSubExt = 0;
        }
    }

     for(p=(char*)pListStart; listLen >0;)
    {
        /* skip extension */
        if(addSubExt && p == pExt)
        {
            p += 4;
            listLen -= 4;
            continue;
        }

        if(*p == '%')
        {
            sscanf(p+1, "%02x", &a);
            *pDest++ = a;
            listLen -= 3;
            p += 3;
        }
        else
        {
            *pDest++ = *p++;
            listLen--;
        }
    }

    *pDest = '\0';
    pOut->pVal = pStreamUrl;
    pOut->len = pDest - pStreamUrl;
}

//***************************************************************************************//
//***************************************************************************************//
int aw_rtmp_parseUrl(char *url, int *protocol, aw_rtmp_aval_t *host, unsigned int *port,
                        aw_rtmp_aval_t *pPlayPath, aw_rtmp_aval_t *pApp)
{
    char *p = NULL;
    char *end = NULL;
    char *colon = NULL;
    char *question = NULL;
    char *slash = NULL;
    char *slash2 = NULL;
    char *slash3 = NULL;
    int len = 0;
    int hostlen = 0;
    int appLen = 0;
    int appNameLen = 0;
    unsigned int pTmp2 = 0;;
    aw_rtmp_aval_t av;

    *protocol = CDX_RTMP_PROTOCOL_RTMP;
    *port = 0;
     pPlayPath->len = 0;
     pPlayPath->pVal = NULL;
     pApp->len = 0;
     pApp->pVal = NULL;

    /* Old School Parsing */

    /* look for usual :// pattern */
    p = strstr(url, "://");
    if(!p)
    {
        return -1;
    }
    len = (int)(p-url);

    if(len == 4 && strncasecmp(url, "rtmp", 4)==0)
    {
        *protocol = CDX_RTMP_PROTOCOL_RTMP;
    }
    else if(len == 5 && strncasecmp(url, "rtmpt", 5)==0)
    {
        *protocol = CDX_RTMP_PROTOCOL_RTMPT;
    }
    else if(len == 5 && strncasecmp(url, "rtmps", 5)==0)
    {
        *protocol = CDX_RTMP_PROTOCOL_RTMPS;
    }
    else if(len == 5 && strncasecmp(url, "rtmpe", 5)==0)
    {
        *protocol = CDX_RTMP_PROTOCOL_RTMPE;
    }
    else if(len == 5 && strncasecmp(url, "rtmfp", 5)==0)
    {
        *protocol = CDX_RTMP_PROTOCOL_RTMFP;
    }
    else if(len == 6 && strncasecmp(url, "rtmpte", 6)==0)
    {
        *protocol = CDX_RTMP_PROTOCOL_RTMPTE;
    }
    else if(len == 6 && strncasecmp(url, "rtmpts", 6)==0)
    {
        *protocol = CDX_RTMP_PROTOCOL_RTMPTS;
    }

    /* let's get the hostname */
    p += 3;  // pass the 3 bytes "://"

    /* check for sudden death */
    if(*p == 0)
    {
        return -1;
    }

    end       = p + strlen(p);
    colon     = strchr(p, ':');
    question  = strchr(p, '?');
    slash     = strchr(p, '/');

    if(slash)
    {
        hostlen = slash - p;
    }
    else
    {
        hostlen = end - p;
    }
    if(colon && colon-p < hostlen)
    {
        hostlen = colon - p;
    }

    if(hostlen < 256)
    {
        host->pVal = p;
        host->len = hostlen;
    }
    p += hostlen;

    /* get the port number if available */
    if(*p == ':')
    {
        p++;
        pTmp2 = atoi(p);
        if(pTmp2 > 65535)
        {
            //LOGV("Invalid port number!");
        }
        else
        {
            *port = pTmp2;
        }
    }

    if(!slash)
    {
        return -1;
    }
    p = slash+1;
    /* parse application
     *
     * rtmp://host[:port]/app[/appinstance][/...]
     * application = app[/appinstance]
     */

    slash2 = strchr(p, '/');
    if(slash2)
    {
        slash3 = strchr(slash2+1, '/');
    }

    appLen = end-p; /* ondemand, pass all parameters as app */
    appNameLen = appLen; /* ondemand length */

    if(strstr(p, "slist=") && question)
    {
        /* whatever it is, the '?' and slist=
                means we need to use everything as app and parse plapath from slist= */
        appNameLen = question-p;
    }
    else if(strncmp(p, "ondemand/", 9)==0)
    {
        /* app = ondemand/foobar, only pass app=ondemand */
        appLen = 8;
        appNameLen = 8;
    }
    else
    {
        /* app!=ondemand, so app is app[/appinstance] */
        if(slash3)
        {
            appNameLen = slash3-p;
        }
        else if(slash2)
        {
            appNameLen = slash2-p;
        }
        appLen = appNameLen;
    }

    pApp->pVal = p;
    pApp->len = appLen;
    p += appNameLen;

    if(*p == '/')
    {
        p++;
    }

    if(end-p)
    {
        //av = {p, end-p};
        av.pVal = p;
        av.len = end-p;
        aw_rtmp_parse_playpath(&av, pPlayPath);
    }
    return 0;
}

//***************************************************************************************//
//***************************************************************************************//

void aw_amf_add_prop(aw_amf_object_t *obj, aw_amfobject_property_t *prop)
{
    if(!(obj->o_num & 0x0f))
    {
        obj->o_props = realloc(obj->o_props, (obj->o_num + 16) * sizeof(aw_amfobject_property_t));
    }
    memcpy(&obj->o_props[obj->o_num++], prop, sizeof(aw_amfobject_property_t));
}

static int aw_parse_amf(aw_amf_object_t *obj, aw_rtmp_aval_t *av, int *depth)
{
    int i;
    char *p = NULL;
    char *pArg = NULL;
    aw_amfobject_property_t prop;//{{0,0}};

    memset(&prop, 0, sizeof(aw_amfobject_property_t));

    pArg = av->pVal;

    if(pArg[1] == ':')
    {
        p = (char *)pArg+2;
        switch(pArg[0])
        {
            case 'B':
                prop.eType = RTMP_AMF_BOOLEAN;
                prop.uVu.nNum = atoi(p);
                break;
            case 'S':
                prop.eType = RTMP_AMF_STRING;
                prop.uVu.aval.pVal = p;
                prop.uVu.aval.len = av->len - (p-pArg);
                break;
            case 'N':
                prop.eType = RTMP_AMF_NUMBER;
                prop.uVu.nNum = strtod(p, NULL);
                break;
            case 'Z':
                prop.eType = RTMP_AMF_NULL;
                break;
            case 'O':
                i = atoi(p);
                if(i)
                {
                    prop.eType = RTMP_AMF_OBJECT;
                }
                else
                {
                    (*depth)--;
                    return 0;
                }
                break;
            default:
            {
                return -1;
            }
        }
    }
    else if ((pArg[2] == ':') && (pArg[0] == 'N'))
    {
        p = strchr(pArg+3, ':');
        if(!p || !*depth)
        {
            return -1;
        }
        prop.name.pVal = (char *)pArg+3;
        prop.name.len = p - (pArg+3);
        p++;
        switch(pArg[1])
        {
            case 'B':
                prop.eType = RTMP_AMF_BOOLEAN;
                prop.uVu.nNum = atoi(p);
                break;
            case 'S':
                prop.eType = RTMP_AMF_STRING;
                prop.uVu.aval.pVal = p;
                prop.uVu.aval.len = av->len - (p-pArg);
                break;
            case 'N':
                prop.eType = RTMP_AMF_NUMBER;
                prop.uVu.nNum = strtod(p, NULL);
                break;
            case 'O':
                prop.eType = RTMP_AMF_OBJECT;
                break;
            default:
                return -1;
        }
    }
    else
    {
        return -1;
    }

    if(*depth)
    {
        aw_amf_object_t *o2;
        for (i=0; i<*depth; i++)
        {
            o2 = &obj->o_props[obj->o_num-1].uVu.pObject;
            obj = o2;
        }
    }
    aw_amf_add_prop(obj, &prop);
    if(prop.eType == RTMP_AMF_OBJECT)
    {
        (*depth)++;
    }
    return 0;
}

int aw_rtmp_setOpt(aw_rtmp_t *rtmp, aw_rtmp_aval_t *opt, aw_rtmp_aval_t *pArg)
{
    int i;
    void *v;

    for(i=0; options[i].name.len; i++)
    {
       if(opt->len != options[i].name.len)
       {
            continue;
       }
       if(strcasecmp(opt->pVal, options[i].name.pVal))
       {
            continue;
       }

       v = (char *)rtmp + options[i].off;
       switch(options[i].otype)
       {
            case STRING:
            {
                aw_rtmp_aval_t *aptr = v;
                *aptr = *pArg;
            }
                break;
            case INT:
            {
                long l = strtol(pArg->pVal, NULL, 0);
                *(int *)v = l;
            }
                break;
            case BOOL:
            {
                int j, fl;
                fl = *(int *)v;
                for(j=0; truth[j].len; j++)
                {
                    if(pArg->len != truth[j].len)
                    {
                        continue;
                    }
                    if(strcasecmp(pArg->pVal, truth[j].pVal))
                    {
                        continue;
                    }
                    fl |= options[i].miscType;
                    break;
                }
                *(int *)v = fl;
          }
                break;
          case CONN:
                if(aw_parse_amf(&rtmp->link.extras, pArg, &rtmp->link.edepth))
                {
                    return 0;
                }
                break;
       }
       break;
  }

    if(!options[i].name.len)
    {
        return 0;
    }
    return 1;
}

//***************************************************************************************//
//***************************************************************************************//

int aw_rtmp_setupUrl2(CdxDataSourceT *datasource, aw_rtmp_t *rtmp)
{
    aw_rtmp_aval_t opt;
    aw_rtmp_aval_t arg;
    char *pTmp1 = NULL;
    char *pTmp2 = NULL;
    char *pSpace = NULL;
    int   ret = 0;
    int   len = 0;
    unsigned int port = 0;
    unsigned int a;

    if(strstr(datasource->uri, "live"))
    {
        rtmp->isLiveStream = 1;
        rtmp->link.lFlags |= RTMP_LF_LIVE;
    }

    pSpace = strchr(datasource->uri, ' ');
    if(pSpace != NULL)
    {
        *pSpace = '\0';
    }

    len = strlen(datasource->uri);
    ret = aw_rtmp_parseUrl(datasource->uri, &rtmp->link.protocol, &rtmp->link.hostname,
                             &port, &rtmp->link.playpath0, &rtmp->link.app);
    if(ret)
    {
        return ret;
    }

    rtmp->link.port = port;
    rtmp->link.playpath = rtmp->link.playpath0;

    while(pSpace)
    {
        *pSpace++ = '\0';
        pTmp1 = pSpace;
        pTmp2 = strchr(pTmp1, '=');
        if(!pTmp2)
        {
            break;
        }
        opt.pVal = pTmp1;
        opt.len = pTmp2 - pTmp1;
        *pTmp2++ = '\0';
        arg.pVal = pTmp2;
        pSpace = strchr(pTmp2, ' ');
        if(pSpace)
        {
            *pSpace = '\0';
            arg.len = pSpace - pTmp2;
            /* skip repeated spaces */
            while(pSpace[1] == ' ')
              {
                *pSpace++ = '\0';
            }
        }
        else
        {
            arg.len = strlen(pTmp2);
        }

         /* unescape */
        port = arg.len;
        for (pTmp1=pTmp2; port >0;)
        {
            if(*pTmp1 == '\\')
            {
                if(port < 3)
                {
                    return -1;
                }
                sscanf(pTmp1+1, "%02x", &a);
                *pTmp2++ = a;
                port -= 3;
                pTmp1 += 3;
            }
            else
            {
                *pTmp2++ = *pTmp1++;
                port--;
            }
        }
        arg.len = pTmp2 - arg.pVal;
        ret = aw_rtmp_setOpt(rtmp, &opt, &arg);
        if (!ret)
        {
            return ret;
        }
    }

    if(!rtmp->link.tcUrl.len)
    {
        rtmp->link.tcUrl.pVal = datasource->uri;
        if(rtmp->link.app.len)
        {
            if(rtmp->link.app.pVal < datasource->uri + len)
            {
                /* if app is part of original url, just use it */
                rtmp->link.tcUrl.len = rtmp->link.app.len + (rtmp->link.app.pVal - datasource->uri);
            }
            else
            {
                len = rtmp->link.hostname.len + rtmp->link.app.len +
                      sizeof("rtmpte://:65535/");
                rtmp->link.tcUrl.pVal = (char*)malloc(len);
                rtmp->link.tcUrl.len = snprintf(rtmp->link.tcUrl.pVal, len,
                                       "%s://%.*s:%d/%.*s",
                                        RTMPProtocolStringsLower[rtmp->link.protocol],
                                        rtmp->link.hostname.len, rtmp->link.hostname.pVal,
                                        rtmp->link.port,
                                        rtmp->link.app.len, rtmp->link.app.pVal);
                                        rtmp->link.lFlags |= RTMP_LF_FTCU;
            }
        }
        else
        {
            rtmp->link.tcUrl.len = strlen(datasource->uri);
        }
    }

    if(rtmp->link.port == 0)
    {
         if(rtmp->link.protocol & CDX_RTMP_FEATURE_SSL)
         {
            rtmp->link.port = 443;
         }
         else if(rtmp->link.protocol & CDX_RTMP_FEATURE_HTTP)
         {
            rtmp->link.port = 80;
         }
         else
         {
            rtmp->link.port = 1935;
         }
    }
    return 1;
}

//***************************************************************************//
//***************************************************************************//
int aw_rtmp_get_next_media_packet(aw_rtmp_t *rtmp, aw_rtmp_packet_t *packet)
{
    int bHasMediaPacket = 0;
    while(!bHasMediaPacket && aw_rtmp_is_connected(rtmp)
        && aw_rtmp_read_packet(rtmp, packet))
    {
        if(packet->nBytesRead != packet->nBodySize)
        {
            continue;
        }
        bHasMediaPacket = aw_rtmp_client_packet(rtmp, packet);

        if(!bHasMediaPacket)
        {
            aw_rtmp_packet_free(packet);
        }
        else if(rtmp->pausing == 3)
        {
            if(packet->nTimeStamp <= rtmp->mediaStamp)
            {
                bHasMediaPacket = 0;
                continue;
            }
            rtmp->pausing = 0;
        }
    }

    if(bHasMediaPacket)
    {
        rtmp->bPlaying = 1;
    }
    else if(rtmp->sb.sbTimedout && !rtmp->pausing)
    {
        rtmp->pauseStamp = rtmp->channelTimestamp[rtmp->nMediaChannel];
    }
    return bHasMediaPacket;
}

//**********************************************************************************************//
//**********************************************************************************************//
int aw_rtmp_read_one_packet(aw_rtmp_t *rtmp, char *buf, unsigned int buflen)
{
    unsigned int size = 0;
    int len = 0;
    int useAnciBufFlag = 0;
    int rtnGetNextMediaPacket = 0;
    char *ptr = NULL;
    char* pend = NULL;
    unsigned int prevTagSize = 0;
    unsigned int nTimeStamp = 0;
    aw_rtmp_packet_t packet;
    char *packetBody = NULL;
    unsigned int nPacketLen = 0;

    #define RTMP_PACKET_TYPE_AUDIO              0x08
    #define RTMP_PACKET_TYPE_VIDEO              0x09
    #define RTMP_PACKET_TYPE_INFO               0x12
    #define RTMP_PACKET_TYPE_FLASH_VIDEO        0x16

new_read_packet:
    memset(&packet, 0, sizeof(aw_rtmp_packet_t));
    rtnGetNextMediaPacket = aw_rtmp_get_next_media_packet(rtmp, &packet);

    while(rtnGetNextMediaPacket > 0)
    {
        if(rtmp->exitFlag == 1)
        {
            return -1;
        }
        packetBody = packet.pBody;
        nPacketLen = packet.nBodySize;  //size of message length
        /****Return CDX_RTMP_READ_COMPLETE
            if this was completed nicely with invoke message Play.Stop or Play.Complete*/
        if(rtnGetNextMediaPacket == 2)
        {
            return -1;
        }
        rtmp->read.dataType |= (((packet.mPacketType == RTMP_PACKET_TYPE_AUDIO) << 2) |
                 (packet.mPacketType == RTMP_PACKET_TYPE_VIDEO));

        if(packet.mPacketType == RTMP_PACKET_TYPE_VIDEO && nPacketLen <= 5)
         /*video chunk do not have data*/
        {

            goto new_read_packet;
        }
        if(packet.mPacketType == RTMP_PACKET_TYPE_AUDIO && nPacketLen <= 1)
         /*audio chunk do not have data*/
        {

            goto new_read_packet;
        }
        /* calculate packet size and allocate slop buffer if necessary */
        size = nPacketLen +
            ((packet.mPacketType == RTMP_PACKET_TYPE_AUDIO
            || packet.mPacketType == RTMP_PACKET_TYPE_VIDEO
            || packet.mPacketType == RTMP_PACKET_TYPE_INFO) ? 11 : 0) +   /*tag header*/
            (packet.mPacketType != RTMP_PACKET_TYPE_FLASH_VIDEO ? 4 : 0); /* privious tag size */

        if((size+4) <= buflen)
        {
            ptr = buf;
        }
        else
        {
            if(nPacketLen >= 128*1024)
            {
            }
            ptr = rtmp->flvDataBuffer;
            useAnciBufFlag = 1;
        }
        pend = ptr + size + 4;

        if(packet.mPacketType == RTMP_PACKET_TYPE_AUDIO
            || packet.mPacketType == RTMP_PACKET_TYPE_VIDEO
            || packet.mPacketType == RTMP_PACKET_TYPE_INFO)
        {
            nTimeStamp = rtmp->read.nResumeTS + packet.nTimeStamp;
            rtmp->timeStamp = nTimeStamp;
            prevTagSize = 11 + nPacketLen; /*rtmp stream do not contain flv tag header */
            *ptr = packet.mPacketType;  /* 0x08 or 0x09 or 0x12 */
            ptr++;
            ptr = aw_amf_encode_int24(ptr, pend, nPacketLen); /* tag size (3 bytes)*/

            ptr = aw_amf_encode_int24(ptr, pend, nTimeStamp); /* timestamp(3 bytes) */
            *ptr = (char)((nTimeStamp & 0xFF000000) >> 24);   /*ex time stamp*/
            ptr++;
            /* stream id */
            ptr = aw_amf_encode_int24(ptr, pend, 0);
        }
        memcpy(ptr, packetBody, nPacketLen); /* read tag data form packetBody */
        len = nPacketLen;

        /* find the keyframes, if  metadata has "keyframes", the stream is seekable */

/*
        if(packet.mPacketType == RTMP_PACKET_TYPE_INFO)
        {
            char* a = (char*)malloc(len+1);
            memcpy(a, ptr, len);
            a[len] = '\0';
            //printf("*****************\n%c\n******************\n",a);
            char* key = strstr(a, "p");
            if(key)
            {
                rtmp->isLiveStream = 1;
                printf("The stream has keyframes.\n");
            }
            else
            {
                printf("connot seek.\n");
            }

            char* datasize = strstr(a, "width");
            if(datasize)printf("******width******\n");
        }
*/
        /* correct tagSize and obtain timestamp if we have an FLV stream */
        if(packet.mPacketType == RTMP_PACKET_TYPE_FLASH_VIDEO)
        {
            unsigned int pos = 0;
            int delta;

            /* grab first timestamp and see if it needs fixing */
            nTimeStamp = aw_amf_decode_int24(packetBody + 4);
            nTimeStamp |= (packetBody[7] << 24);
            delta = packet.nTimeStamp - nTimeStamp + rtmp->read.nResumeTS;

            while(pos + 11 < nPacketLen)
            {
                if(rtmp->exitFlag==1)
                {
                    return -1;
                }
                /* size without header (11) and without prevTagSize (4) */
                unsigned int dataSize = aw_amf_decode_int24(packetBody + pos + 1);
                nTimeStamp = aw_amf_decode_int24(packetBody + pos + 4);
                nTimeStamp |= (packetBody[pos + 7] << 24);

                if(delta)
                {
                    nTimeStamp += delta;
                    aw_amf_encode_int24(ptr+pos+4, pend, nTimeStamp);
                    ptr[pos+7] = nTimeStamp>>24;
                }

                /* set data type */
                rtmp->read.dataType |= (((*(packetBody + pos) == 0x08) << 2) |
                     (*(packetBody + pos) == 0x09));

                if(pos + 11 + dataSize + 4 > nPacketLen)
                {
                    if(pos + 11 + dataSize > nPacketLen)
                    {

                        return -1;
                    }
                    /* we have to append a last tagSize! */
                    prevTagSize = dataSize + 11;
                    aw_amf_encode_int32(ptr + pos + 11 + dataSize, pend,prevTagSize);
                    size += 4;
                    len += 4;
                }
                else
                {
                    prevTagSize = aw_amf_decode_int32(packetBody + pos + 11 + dataSize);
                    if(prevTagSize != (dataSize + 11))
                    {
                        prevTagSize = dataSize + 11;
                        aw_amf_encode_int32(ptr + pos + 11 + dataSize, pend,prevTagSize);
                    }
                }
                pos += prevTagSize + 4;    /*(11+dataSize+4); */
            }
        }
        ptr += len;

        if(packet.mPacketType != RTMP_PACKET_TYPE_FLASH_VIDEO)
        {
            /* FLV tag packets contain their own prevTagSize */
            aw_amf_encode_int32(ptr, pend, prevTagSize);
        }

        /* In non-live this nTimeStamp can contain an absolute TS.
        * Update ext timestamp with this absolute offset in non-live mode
        * otherwise report the relative one
        */

        rtmp->read.timestamp = (rtmp->link.lFlags & RTMP_LF_LIVE) ? packet.nTimeStamp : nTimeStamp;
        break;
    }

    if(rtnGetNextMediaPacket)
    {
        aw_rtmp_packet_free(&packet);
    }
    if(useAnciBufFlag == 1)
    {
        #if 0
        memcpy(buf, rtmp->flvDataBuffer, buflen);
        if(buf+buflen > rtmp->bufEndPtr)
        {
            memcpy(rtmp->buffer, rtmp->flvDataBuffer+buflen, size-buflen);
        }
        #else
        if(size > buflen)
        {
            memcpy(buf, rtmp->flvDataBuffer, buflen);
            memcpy(rtmp->buffer, rtmp->flvDataBuffer+buflen, size-buflen);
        }
        else
        {
            memcpy(buf, rtmp->flvDataBuffer, size);
        }
        #endif
        useAnciBufFlag = 0;
    }
    return size;
}

//*********************************************************************************************//
//*********************************************************************************************//
int aw_rtmp_stream_read(aw_rtmp_t* rtmp, char* buf, int size)
{
    int readLen = 0;
    int nRead = 0;
    char *orgBuf = NULL;

    char flvHeader[] = { 'F', 'L', 'V', 0x01,
                          0x00,             /* 0x04 == audio, 0x01 == video */
                          0x00, 0x00, 0x00, 0x09,
                          0x00, 0x00, 0x00, 0x00
                        };

    if((rtmp->read.flags & CDX_RTMP_READ_HEADER) == 0)
    {
        orgBuf = buf;
        memcpy(buf, flvHeader, sizeof(flvHeader));
        buf += sizeof(flvHeader);
        readLen += sizeof(flvHeader);
        size -= sizeof(flvHeader);

        while(rtmp->read.timestamp == 0)
        {
            nRead = aw_rtmp_read_one_packet(rtmp, buf, size);
         /*读取网络流数据到rtmp客户端buffer*/
            if(nRead <= 0)
            {

                return -1;
            }
            buf += nRead;
            size -= nRead;
            readLen += nRead;
            if(rtmp->read.dataType == 5)
            {
                break;
            }
            if(rtmp->exitFlag == 1)
            {
                return -1;
            }
        }
        orgBuf[4] = rtmp->read.dataType;   //change the fifth byte of  flv header
        rtmp->read.flags |= CDX_RTMP_READ_HEADER;
    }
    else
    {
        nRead = aw_rtmp_read_one_packet(rtmp, buf, size);
        if(nRead <= 0)
        {
            return -1;
        }
        readLen += nRead;
    }
    return readLen;
}

void* aw_read_rtmp_stream2(void* p_arg)
{
    aw_rtmp_t*   rtmp = NULL;
    int remainSize = 0;
    int redDataLen = 0;
    rtmp = (aw_rtmp_t*)p_arg;
    int probeFlag = 1;

    while(1)
    {
        if(rtmp->exitFlag == 1)  /* player exit */
        {
            break;
        }
        else if((rtmp->streamBufSize-rtmp->validDataSize)<= 3*1024*1024) // protect buffer size 3M
        {
            usleep(4*1000);
            continue;
        }
        if(rtmp->bufWritePtr < rtmp->bufReleasePtr)
        {
            remainSize = rtmp->bufReleasePtr - rtmp->bufWritePtr;
        }
        else
        {
            remainSize = rtmp->bufEndPtr - rtmp->bufWritePtr+1;
        }

        redDataLen = aw_rtmp_stream_read(rtmp, rtmp->bufWritePtr, remainSize);
        if(redDataLen <= 0)
        {
            break;
        }

        rtmp->bufWritePtr += redDataLen;
        if(rtmp->bufWritePtr > rtmp->bufEndPtr)
        {
            rtmp->bufWritePtr -= rtmp->streamBufSize;
        }

        pthread_mutex_lock(&rtmp->bufferMutex);
        rtmp->validDataSize += redDataLen;  /**/
        rtmp->bufPos += redDataLen;
        pthread_mutex_unlock(&rtmp->bufferMutex);

        // for probe
        if(probeFlag && (rtmp->validDataSize >= RTMP_PROBE_DATA_LEN))
        {
             probeFlag = 0;
             memcpy(rtmp->probeData.buf, rtmp->buffer, RTMP_PROBE_DATA_LEN);
             pthread_mutex_lock(&rtmp->lock);
             rtmp->iostate = CDX_IO_STATE_OK;
             pthread_cond_signal(&rtmp->cond);
             pthread_mutex_unlock(&rtmp->lock);
        }
    }

    pthread_mutex_lock(&rtmp->lock);
    rtmp->iostate = CDX_IO_STATE_ERROR;
    pthread_cond_signal(&rtmp->cond);
    pthread_mutex_unlock(&rtmp->lock);
    rtmp->eof = 1;
    return NULL;
}

static int RtmpGetCacheState(struct StreamCacheStateS *cacheState, CdxStreamT *stream)//0424
{
    aw_rtmp_t* rtmp = NULL;
    if(!stream)
    {
        return -1;
    }

    rtmp = (aw_rtmp_t*)stream;

    cdx_int64 totSize = rtmp->fileSize;
    cdx_int64 bufPos = rtmp->bufPos;
    //cdx_int64 readPos = rtmp->dmxReadPos;
    double percentage = 0;
    cdx_int32 kbps = 0;//4000;//512KB/s

    memset(cacheState, 0, sizeof(struct StreamCacheStateS));

    if(totSize > 0)
    {
        percentage = 100.0 * (double)(bufPos)/totSize;
    }
    else
    {
        percentage = 0.0;
    }

    if (percentage > 100)
    {
        percentage = 100;
    }

    cacheState->nBandwidthKbps = kbps;
    cacheState->nCacheCapacity = rtmp->streamBufSize;
    cacheState->nCacheSize = rtmp->validDataSize;

    return 0;
}

void RtmpClose(aw_rtmp_t* rtmp)
{
    if(rtmp != NULL)
    {
        rtmp->exitFlag = 1;
        while(rtmp->eof == 0)
        {
            usleep(1000);
        }
        aw_rtmp_close(rtmp);

        if(rtmp->buffer != NULL)
        {
            free(rtmp->buffer);
            rtmp->buffer = NULL;
        }
        if(rtmp->probeData.buf != NULL)
        {
            free(rtmp->probeData.buf);
            rtmp->probeData.buf = NULL;
        }
        if(rtmp->flvDataBuffer != NULL)
        {
            free(rtmp->flvDataBuffer);
            rtmp->flvDataBuffer  = NULL;
        }
        pthread_mutex_destroy(&rtmp->bufferMutex);
        pthread_mutex_destroy(&rtmp->lock);
        pthread_cond_destroy(&rtmp->cond);

        pthread_mutex_destroy(rtmp->dnsMutex);
        pthread_cond_destroy(&rtmp->dnsCond);
        free(rtmp->dnsMutex);
        rtmp->dnsMutex = NULL;

        free(rtmp);
    }
}

//*****************************************************************************//
//*****************************************************************************//
CdxStreamProbeDataT* __RtmpGetProbeData(CdxStreamT * stream)
{
    aw_rtmp_t* rtmp = NULL;

    rtmp = (aw_rtmp_t*)stream;

    return &(rtmp->probeData);
}

int __RtmpRead(CdxStreamT* stream, void* buffer, unsigned int size)
{
    int sendSize = 0;
    int remainSize = 0;

    aw_rtmp_t* rtmp = NULL;

    rtmp = (aw_rtmp_t*)stream;

    CdxAtomicSet(&rtmp->mState, RTMP_STREAM_READING);
    sendSize = size;
    while(rtmp->validDataSize < (int)size)
    {
        if(rtmp->exitFlag == 1)
        {
            CdxAtomicSet(&rtmp->mState, RTMP_STREAM_IDLE);
            return -1;
        }
        if(rtmp->eof == 1)
        {
            sendSize = (sendSize>=rtmp->validDataSize)? rtmp->validDataSize: sendSize;
            break;
        }
        if(rtmp->forceStop)
        {
            CdxAtomicSet(&rtmp->mState, RTMP_STREAM_IDLE);
            return -1;
        }
        usleep(4*1000);
    }

    remainSize = rtmp->bufEndPtr-rtmp->bufReadPtr+1;
    if(remainSize >= sendSize)
    {
        memcpy((char*)buffer, rtmp->bufReadPtr, sendSize);
    }
    else
    {
        memcpy((char*)buffer, rtmp->bufReadPtr, remainSize);
        memcpy((char*)buffer+remainSize, rtmp->buffer, sendSize-remainSize);
    }
    rtmp->bufReadPtr += sendSize;
    if(rtmp->bufReadPtr > rtmp->bufEndPtr)
    {
        rtmp->bufReadPtr -= rtmp->streamBufSize;
    }

    rtmp->bufReleasePtr = rtmp->bufReadPtr;
    pthread_mutex_lock(&rtmp->bufferMutex);
    rtmp->dmxReadPos += sendSize;
    rtmp->validDataSize -= sendSize;

    pthread_mutex_unlock(&rtmp->bufferMutex);
    CdxAtomicSet(&rtmp->mState, RTMP_STREAM_IDLE);
    return sendSize;
}

int __RtmpClose(CdxStreamT* stream)
{
    aw_rtmp_t* rtmp = NULL;

    rtmp = (aw_rtmp_t*)stream;

    rtmp->exitFlag = 1;
    RtmpClose(rtmp);
    rtmp = NULL;
    return 0;
}

cdx_uint32 __RtmpAttribute(CdxStreamT * stream)
{
    CDX_UNUSE(stream);
    return CDX_STREAM_FLAG_NET;// | CDX_STREAM_FLAG_SEEK;
}

int __RtmpGetIoState(CdxStreamT * stream)
{
    aw_rtmp_t* rtmp = NULL;
    rtmp = (aw_rtmp_t*)stream;
    return rtmp->iostate;
}

cdx_int32 __RtmpForceStop(CdxStreamT *stream)
{
    aw_rtmp_t* rtmp = NULL;
    rtmp = (aw_rtmp_t*)stream;

    int ref;

    pthread_mutex_lock(&rtmp->lock);
    rtmp->exitFlag = 1;
    pthread_cond_broadcast(&rtmp->cond);
    pthread_mutex_unlock(&rtmp->lock);

    if(CdxAtomicRead(&rtmp->mState) == RTMP_STREAM_FORCESTOPPED)
    {
        return 0;
    }
    while((ref = CdxAtomicRead(&rtmp->mState)) != RTMP_STREAM_IDLE)
    {
        usleep(10000);
    }
    CdxAtomicSet(&rtmp->mState, RTMP_STREAM_FORCESTOPPED);

    return 0;
}

cdx_int32 __RtmpClrForceStop(CdxStreamT *stream)
{
    aw_rtmp_t* rtmp = NULL;
    rtmp = (aw_rtmp_t*)stream;

    rtmp->exitFlag = 0;
    CdxAtomicSet(&rtmp->mState, RTMP_STREAM_IDLE);
    return 0;
}

int __RtmpControl(CdxStreamT* stream, int cmd, void* param)
{
    aw_rtmp_t* rtmp = NULL;
    rtmp = (aw_rtmp_t*) stream;
    switch(cmd)
    {
        case STREAM_CMD_GET_CACHESTATE:
            return RtmpGetCacheState((struct StreamCacheStateS *)param, stream);

        case STREAM_CMD_SET_FORCESTOP:
            return __RtmpForceStop(stream);

        case STREAM_CMD_CLR_FORCESTOP:
            return __RtmpClrForceStop(stream);
        default:
            break;
    }

    return CDX_SUCCESS;
}

int __RtmpSeek(CdxStreamT* stream, cdx_int64 offset, int whence)
{
    aw_rtmp_t* rtmp = NULL;
    rtmp = (aw_rtmp_t*) stream;

    int64_t seekPos = 0;
    CdxAtomicSet(&rtmp->mState, RTMP_STREAM_SEEKING);
//CDX_LOGD("rtmp seek (offset = %lld, whence = %d)", offset, whence);
//CDX_LOGD("demux read pos = %lld", rtmp->dmxReadPos);

    switch(whence)
    {
        case SEEK_SET:
        {
            seekPos = offset;
            break;
        }
        case SEEK_CUR:
            seekPos = rtmp->dmxReadPos + offset;
            break;

        case SEEK_END:
            CdxAtomicSet(&rtmp->mState, RTMP_STREAM_IDLE);
            return -1;
    }

    if(seekPos < 0)
    {
        CDX_LOGW("we can not seek to this position");
        CdxAtomicSet(&rtmp->mState, RTMP_STREAM_IDLE);
        return -1;
    }

    // do not cache so much data, so wait
    while(seekPos > rtmp->bufPos)
    {
        if(rtmp->exitFlag)
        {
            CdxAtomicSet(&rtmp->mState, RTMP_STREAM_IDLE);
            return -1;
        }
        usleep(1000);
    }

    if(seekPos > (rtmp->dmxReadPos-1024*1024))
    {
        pthread_mutex_lock(&rtmp->bufferMutex);
        rtmp->bufReadPtr += (seekPos - rtmp->dmxReadPos);
        rtmp->validDataSize -= (seekPos - rtmp->dmxReadPos);
        if (rtmp->bufReadPtr > rtmp->bufEndPtr)
        {
            rtmp->bufReadPtr -= rtmp->streamBufSize;
        }
        else if(rtmp->bufReadPtr < rtmp->buffer)
        {
            rtmp->bufReadPtr += rtmp->streamBufSize;
        }
        rtmp->dmxReadPos = seekPos;
        pthread_mutex_unlock(&rtmp->bufferMutex);
    }
    else
    {
        CDX_LOGW("maybe the buffer is override by the new data,\
         so we can not support to seek to this position");
        pthread_mutex_lock(&rtmp->bufferMutex);
        rtmp->bufReadPtr += (seekPos - rtmp->dmxReadPos);
        rtmp->validDataSize -= (seekPos - rtmp->dmxReadPos);
        if (rtmp->bufReadPtr > rtmp->bufEndPtr)
        {
            rtmp->bufReadPtr -= rtmp->streamBufSize;
        }
        else if(rtmp->bufReadPtr < rtmp->buffer)
        {
            rtmp->bufReadPtr += rtmp->streamBufSize;
        }
        rtmp->dmxReadPos = seekPos;
        pthread_mutex_unlock(&rtmp->bufferMutex);
    }

    CdxAtomicSet(&rtmp->mState, RTMP_STREAM_IDLE);
    return 0;
}

int __RtmpSeekToTime(CdxStreamT* stream, cdx_int64 timeUs)
{
    aw_rtmp_t* rtmp = NULL;
    rtmp = (aw_rtmp_t*)stream;
    int64_t nTimeStamp = rtmp->timeStamp;
    int64_t offset;
    if(rtmp->isLiveStream)
    {
        return -1;
    }
    else
    {
        offset = timeUs - nTimeStamp;
        if(!aw_send_seek(rtmp, offset))
        {
            CDX_LOGE("seek error.\n");
            return -1;
        }
        else
        {
            return timeUs;
        }
    }
    return -1;
}

cdx_int64 __RtmpTell(CdxStreamT* stream)
{
    aw_rtmp_t* rtmp = NULL;
    rtmp = (aw_rtmp_t*)stream;
    pthread_mutex_lock(&rtmp->bufferMutex);
    int64_t ret = rtmp->dmxReadPos;
    pthread_mutex_unlock(&rtmp->bufferMutex);
    return ret;
}

int __RtmpEos(CdxStreamT* stream)
{
    aw_rtmp_t* rtmp = (aw_rtmp_t*)stream;
    return rtmp->eof;
}

cdx_int64 __RtmpSize(CdxStreamT* stream)
{
    aw_rtmp_t* rtmp = NULL;
    rtmp = (aw_rtmp_t*)stream;
    if(rtmp->isLiveStream)
        return -1;
    else
        return rtmp->fileSize;
}

static int __RtmpConect(CdxStreamT* stream)
{
    pthread_t    downloadTid;
    aw_rtmp_t* rtmp = NULL;
    rtmp = (aw_rtmp_t*)stream;
    int ret;

    CdxAtomicSet(&rtmp->mState, RTMP_STREAM_CONNECTING);

    if(!aw_rtmp_connect(rtmp, NULL) )
    {
        CDX_LOGW("rtmp connect error!");
        rtmp->eof = 1;
        CdxAtomicSet(&rtmp->mState, RTMP_STREAM_IDLE);

        pthread_mutex_lock(&rtmp->lock);
        rtmp->iostate = CDX_IO_STATE_ERROR;
        pthread_mutex_unlock(&rtmp->lock);
        return -1;
    }

    if(!aw_rtmp_connect_stream(rtmp, 0))
    {
        CDX_LOGW("rtmp connect stream error!");
        rtmp->eof = 1;
        CdxAtomicSet(&rtmp->mState, RTMP_STREAM_IDLE);

        pthread_mutex_lock(&rtmp->lock);
        rtmp->iostate = CDX_IO_STATE_ERROR;
        pthread_mutex_unlock(&rtmp->lock);
        return -1;
    }

    ret = pthread_create(&downloadTid, NULL, aw_read_rtmp_stream2, (void*)rtmp); /*create thread*/
    if(ret != 0)
    {
        downloadTid = (pthread_t)0;
    }

    pthread_mutex_lock(&rtmp->lock);
    while(rtmp->iostate != CDX_IO_STATE_OK
        && rtmp->iostate != CDX_IO_STATE_EOS
        && rtmp->iostate != CDX_IO_STATE_ERROR
        && !rtmp->exitFlag)
    {
        pthread_cond_wait(&rtmp->cond, &rtmp->lock);
    }
    pthread_mutex_unlock(&rtmp->lock);
    CdxAtomicSet(&rtmp->mState, RTMP_STREAM_IDLE);

    return 0;
}

static struct CdxStreamOpsS RtmpStreamOps =
{
    .connect        = __RtmpConect,
    .getProbeData   = __RtmpGetProbeData,
    .read           = __RtmpRead,
    .close          = __RtmpClose,
    .getIOState     = __RtmpGetIoState,
    .attribute      = __RtmpAttribute,
    .control        = __RtmpControl,
    .seek           = __RtmpSeek,
    .seekToTime     = __RtmpSeekToTime,
    .eos            = __RtmpEos,
    .tell           = __RtmpTell,
    .size           = __RtmpSize,
};

//***************************************************************************************//
//***************************************************************************************//

void aw_rtmp_init(aw_rtmp_t *rtmp)
{
    CDX_CHECK(rtmp);
    memset(rtmp, 0, sizeof(aw_rtmp_t));

    rtmp->probeData.buf = malloc (RTMP_PROBE_DATA_LEN);
    rtmp->probeData.len = RTMP_PROBE_DATA_LEN;
    memset(rtmp->probeData.buf, 0, RTMP_PROBE_DATA_LEN);

    rtmp->streaminfo.ops  = &RtmpStreamOps;

    rtmp->eof = 0;
    rtmp->isLiveStream = 1;
    rtmp->iostate = CDX_IO_STATE_INVALID;
    rtmp->sb.sbSocket = -1;
    rtmp->inChunkSize = RTMP_DEFAULT_CHUNKSIZE;
    rtmp->outChunkSize = RTMP_DEFAULT_CHUNKSIZE;
    rtmp->nBufferMS = 30000;
    rtmp->nClientBW = 2500000;
    rtmp->nClientBW2 = 2;
    rtmp->nServerBW = 2500000;
    rtmp->fAudioCodecs = 3191.0;
    rtmp->fVideoCodecs = 252.0;
    rtmp->link.timeout = 30;
    rtmp->link.swfAge = 30;

    aw_set_rtmp_parameter(rtmp);
}

CdxStreamT* aw_rtmp_streaming_start2(CdxDataSourceT* datasource, int flags)
{
    int ret = 0;

    #define URL_RDONLY 0
    #define URL_WRONLY 1
    #define URL_RDWR   2

    aw_rtmp_t *rtmp = (aw_rtmp_t*)malloc(sizeof(aw_rtmp_t));
    if(!rtmp)
    {
        return NULL;
    }
    aw_rtmp_init(rtmp);
    if(!aw_rtmp_setupUrl2(datasource, rtmp))
    {
        goto fail;
    }
    if(flags & URL_WRONLY)
    {
        rtmp->link.protocol |= CDX_RTMP_FEATURE_WRITE;
    }

    rtmp->streamBufSize = MAX_STREAM_BUF_SIZE;
    CdxAtomicSet(&rtmp->mState, RTMP_STREAM_IDLE);
    rtmp->buffer = (char*)malloc(sizeof(char)*rtmp->streamBufSize);  //*  SetBufferSize
    if(rtmp->buffer == NULL)
    {
        goto fail;
    }

    rtmp->flvDataBuffer = (char*)malloc(sizeof(char)*128*1024);
    if(rtmp->flvDataBuffer  == NULL)
    {
        goto fail;
    }

    rtmp->bufEndPtr      = rtmp->buffer + rtmp->streamBufSize - 1;
    rtmp->bufWritePtr    = rtmp->buffer;
    rtmp->bufReadPtr     = rtmp->buffer;
    rtmp->bufReleasePtr  = rtmp->buffer;
    rtmp->validDataSize  = 0;

    rtmp->dnsMutex = (pthread_mutex_t*)calloc(1,sizeof(pthread_mutex_t));
    if (rtmp->dnsMutex == NULL)
    {
        CDX_LOGE("malloc failed");
        return NULL;
    }
    pthread_mutex_init(rtmp->dnsMutex, NULL);
    pthread_cond_init(&rtmp->dnsCond, NULL);
    rtmp->dnsRet = SDS_PENDING;

    pthread_mutex_init(&rtmp->bufferMutex, NULL);
    rtmp->dmxReadPos = 0;

    ret = pthread_mutex_init(&rtmp->lock, NULL);
    CDX_CHECK(!ret);
    ret = pthread_cond_init(&rtmp->cond, NULL);

    return &(rtmp->streaminfo);

fail:
    if(rtmp)
    {
        CDX_LOGW("failed in rtmp stream start");
        rtmp->eof = 1;
        rtmp->iostate = CDX_IO_STATE_ERROR;
        RtmpClose(rtmp);
            free(rtmp);
        rtmp = NULL;
    }
    return NULL;
}

static CdxStreamT* __RtmpStreamOpen(CdxDataSourceT* dataSource)
{
    CDX_CHECK(dataSource);
    if(dataSource->uri == NULL)
    {
        return NULL;
    }

    CdxStreamT* streaminfo = aw_rtmp_streaming_start2(dataSource, URL_RDONLY);

    return streaminfo;
}

CdxStreamCreatorT rtmpStreamCtor =
{
    .create = __RtmpStreamOpen
};

