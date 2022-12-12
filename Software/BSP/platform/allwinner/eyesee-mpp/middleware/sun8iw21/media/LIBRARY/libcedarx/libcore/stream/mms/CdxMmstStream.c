/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxMmstStream.c
 * Description : MmstStream
 * History :
 *
 */

#include "CdxMmsStream.h"
#include <stdlib.h>

#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <errno.h>

#define MMST_BUF_SIZE 102400
#define MMST_HDR_BUF_SIZE 8192
#define MMST_MAX_STREAMS 20
#define READ_DATA_BUF_SIZE  128*1024
#define MMS_WAIT_DATA_TIME  30*1000

extern CdxUrlT* CdxUrlNew(char* url);
extern void CdxUrlFree(CdxUrlT* curUrl);
extern void CdxUrlUnescapeString(char *outbuf, const char *inbuf);
extern CdxUrlT* CdxCheck4Proxies(CdxUrlT *url);

extern int MmshNopStreamingReadFunc(aw_mms_inf_t* mmsStreamInf, int wantSize);

extern int Connect2Server(aw_mms_inf_t* mmsStreamInf, char *host, int  port, int verb);
//extern int aw_mms_http_streaming_start(CdxDataSourceT* datasource, aw_mms_inf_t* mmsStreamInf);
//extern int aw_mms_http_streaming_read_func(aw_mms_inf_t* mmsStreamInf);

extern int MmshStreamingStart(char *uri, aw_mms_inf_t* mmsStreamInf);
extern int MmshStreamingReadFunc(aw_mms_inf_t* mmsStreamInf, int wantSize);

/* Variables for the command line option -user, -passwd, -bandwidth,
   -user-agent and -nocookies
   client and server use this struct(including packet header and data) to send packet
   */
typedef struct COMMAND
{
    unsigned char buf[MMST_BUF_SIZE];
    int     numBytes;
} command_t;

//******************************************************************************//
//*****************************************************************************//
int MmsGetNetworkData(aw_mms_inf_t* mmsStreamInf, int fs, char* data, int dataLen)
{
    int readLen = 0;

    // ***** we cannot get the CdxSockAsynRecv here, it will error,
    //***** readLen will be 0, and error out;
    //readLen = CdxSockAsynRecv(fs, data, dataLen, 0, &mmsStreamInf->exitFlag);

#if 1
    int ret;
    fd_set       fds;
    fd_set       ers;
    struct timeval  time;

    while(1)
    {
        if(mmsStreamInf->exitFlag==1)
        {
            return -1;
        }
        FD_ZERO(&fds);
        FD_SET(mmsStreamInf->sockFd, &fds);
        FD_ZERO(&ers);
        FD_SET(mmsStreamInf->sockFd, &ers);
        time.tv_sec = 0;   //select waiting time = 10s
        time.tv_usec = 100000;
        ret = select(mmsStreamInf->sockFd+1, &fds, NULL, &ers, &time);
        switch(ret)
        {
            case -1:
            {
                CDX_LOGD("select = -1");
                return -1;
            }
            case 0:
            {
                break;
            }
            default:
            {
                if(FD_ISSET(mmsStreamInf->sockFd, &ers))  //检测sockFd是否发生变化
                {
                    CDX_LOGD("**************socket(shutdown).\n");
                }
                if(FD_ISSET(mmsStreamInf->sockFd, &fds))
                {
                    readLen = recv(fs, data, dataLen, 0);
                    if(readLen == -1)
                    {
                        CDX_LOGW("errno = %d", errno);
                    }

                    return readLen;
                }
                break;
            }
        }
    }
 #endif

    return readLen;
}

/* string to utf16 (2bytes :a char and 0) */
static void String2Utf16(char *pDst,char *pSrc, int len)
{
    int i = 0;
    if (len > 499)
    {
        len = 499;
    }
    for (i=0; i<len; i++)
    {
        pDst[i*2] = pSrc[i];
        pDst[i*2+1] = 0;
    }
    /* trailing zeroes */
    pDst[i*2] = 0;
    pDst[i*2+1] = 0;
}

//*********************************************************************************//
//*********************************************************************************//
static void Put32 (command_t *cmd, unsigned int val)
{
    cmd->buf[cmd->numBytes] = val % 256;        /* 取低8位 */
    val = val >> 8;
    cmd->buf[cmd->numBytes+1] = val % 256 ;
    val = val >> 8;
    cmd->buf[cmd->numBytes+2] = val % 256 ;
    val = val >> 8;
    cmd->buf[cmd->numBytes+3] = val % 256 ;
    cmd->numBytes += 4;
}

//************************************************************************//
//************************************************************************//
//* @client send command message to server
//* @param seqNum [IN]: sequence number.
//* @param s [IN]: server socket.
//* @param command [IN]: command type.
//* @param switches [IN]: prefix1.
//* @param extra [IN]: prefix2.
//* @param length [IN]: packet data body length.
//* @param data [IN]: packet data .
static int MmstSendCommand(int* seqNum,int s, int command, unsigned int switches,
                     unsigned int extra, int length, unsigned char *data)
{
    command_t  cmd;
    int        len8;
    int        ret;

    //packet header
    len8 = (length + 7) / 8;
    cmd.numBytes = 0;

    Put32 (&cmd, 0x00000001);     /* start sequence */
    Put32 (&cmd, 0xB00BFACE);     /* #-)) */
    Put32 (&cmd, len8*8 + 32);    /* (Command length)data length after protocol type*/
    Put32 (&cmd, 0x20534d4d);     /* protocol type "MMS " */
    Put32 (&cmd, len8 + 4);
   /* Length until end of packet in 8 byte boundary lengths，Including own data field */
    Put32 (&cmd, *seqNum);        /* sequence number '0000'  */
    (*seqNum)++;                   /* sequence number add one after send a command message */
    Put32 (&cmd, 0x0);        /* (8bytes) double decision time stamp(used for net timing) */
    Put32 (&cmd, 0x0);        /* (8bytes) double decision time stamp(used for net timing) */
    Put32 (&cmd, len8+2);
   /* Length until end of packet in 8 byte boundary lengths. Including own data field */
    Put32 (&cmd, 0x00030000 | command); /* dir(0x0003: client to server) | command */

    Put32 (&cmd, switches);    /*prefix 1*/
    Put32 (&cmd, extra);      /*prefix 2*/

    // packet body
    memcpy (&cmd.buf[48], data, length);
    if(length & 7)  //* if length cannot divide exactly by 8, need polishing 8bytes
    {
        memset(&cmd.buf[48 + length], 0, 8 - (length & 7));
    }

    ret = CdxSockAsynSend(s, cmd.buf, len8*8+48, 300*1000, NULL);
    //ret = send(s, cmd.buf, len8*8+48, 0);
    if(ret != (len8*8+48))
    {
        CDX_LOGW("*********send comand error");
        return -1;
    }
    return 0;
}

//**********************************************************************//
//*********************************************************************//
static unsigned int Get32 (unsigned char *pCmd, int nOffset)
{
    unsigned int ret;

    ret = pCmd[nOffset] ;
    ret |= pCmd[nOffset+1]<<8 ;
    ret |= pCmd[nOffset+2]<<16 ;
    ret |= pCmd[nOffset+3]<<24 ;
    return ret;
}

//**************************************************************************//
//*************************************************************************//
static int MmstGetAnswer (aw_mms_inf_t* mmsStreamInf, int s)
{
    char  data[MMST_BUF_SIZE];
    int   command = 0x1b;
    int   len = 0;

    while((command==0x1b) && (mmsStreamInf->exitFlag==0))
    {
        len = MmsGetNetworkData(mmsStreamInf,s, data,MMST_BUF_SIZE);
        if(len <= 0)
        {
            CDX_LOGW("****************the return value  of the recv is 0.\n");
            return -1;
        }
        command = Get32((unsigned char*)data, 36) & 0xFFFF;  // command in packet header
        if(command == 0x1b)  // command=0x1b,unkown
        {
            MmstSendCommand(&mmsStreamInf->seqNum, s, 0x1b, 0, 0, 0, (unsigned char*)data);
        }
    }
    return 0;
}
//***************************************************************************//
//***************************************************************************//
static int GetData (aw_mms_inf_t* mmsStreamInf, int s, unsigned char *buf, size_t count)
{
    ssize_t len;
    size_t total = 0;

    if(count == 0)
    {
        return 0;
    }

    while((total<count) &&(mmsStreamInf->exitFlag==0))
    {
        len = MmsGetNetworkData(mmsStreamInf,s,(char*)&buf[total], count-total);

        if(len <= 0)
        {
            CDX_LOGW("****************the return value  of the recv is 0.\n");
            return 0;
        }
        total += len;
        fflush (stdout);
    }
    return total;
}

//************************************************************************//
//************************************************************************//

static int MmstGetHeader (aw_mms_inf_t* mmsStreamInf, int s, unsigned char *header)
{
    unsigned char  preHeader[8];
    int            headerLen = 0;
    int            packetLen = 0;
    int            command;
    unsigned char  data[MMST_BUF_SIZE];

    headerLen = 0;
    while(1)
    {
        if(!GetData(mmsStreamInf, s, preHeader, 8))
        {
            return 0;
        }
        if(preHeader[4] == 0x02)   //* (packet td type = 0x02)asf header
        {
            packetLen = (preHeader[7]<< 8| preHeader[6]) - 8;
            if(packetLen < 0 || packetLen > MMST_HDR_BUF_SIZE - headerLen)
            {
                return 0;
            }
            if(!GetData (mmsStreamInf, s, &header[headerLen], packetLen))
            {
                return 0;
            }
            headerLen += packetLen;
            if((header[headerLen-1] == 1) && (header[headerLen-2]==1))
         //asf header end with 0x01 0x01
            {
                return headerLen;
            }
        }
        else   //  packet header of requesting header response
        {
            if (!GetData (mmsStreamInf, s, (unsigned char*)&packetLen, 4)) // cammand length
            {
                return 0;
            }
            packetLen = Get32((unsigned char*)&packetLen, 0) + 4;  // packet header length
            if(packetLen < 0 || packetLen > MMST_BUF_SIZE)
            {
                return 0;
            }
            if(!GetData (mmsStreamInf, s, data, packetLen))
            {
                return 0;
            }
            command = Get32 (data, 24) & 0xFFFF;
            if(command == 0x1b)
            {
                MmstSendCommand(&mmsStreamInf->seqNum, s,0x1b, 0, 0, 0, data);
            }
        }
   }
   return 0;
}

//*************************************************************************//
//*************************************************************************//
/*     parse the header data
 *   1.find the Minimum Data Packet Size(packet size) in ASF_File_Properties_Object
 *   2.find stream number in SF_Stream_Properties_Object
 */
static int MmstInterpHeader (aw_mms_inf_t* mmsStreamInf, unsigned char *header, int header_len)
{
    int i;
    int packetLength = -1;
    int streamId = 0;
    cdx_uint64  guid_1 = 0;
    cdx_uint64  guid_2 = 0;
    cdx_uint64  length = 0;

    i = 30;
    while (i<header_len)
    {
        guid_2 = (cdx_uint64)header[i] | ((cdx_uint64)header[i+1]<<8)
                |((cdx_uint64)header[i+2]<<16) | ((cdx_uint64)header[i+3]<<24)
                |((cdx_uint64)header[i+4]<<32) | ((cdx_uint64)header[i+5]<<40)
                |((cdx_uint64)header[i+6]<<48) | ((cdx_uint64)header[i+7]<<56);
        i += 8;

        guid_1 = (cdx_uint64)header[i] | ((cdx_uint64)header[i+1]<<8)
                |((cdx_uint64)header[i+2]<<16) | ((cdx_uint64)header[i+3]<<24)
                |((cdx_uint64)header[i+4]<<32) | ((cdx_uint64)header[i+5]<<40)
                |((cdx_uint64)header[i+6]<<48) | ((cdx_uint64)header[i+7]<<56);
        i += 8;

        length = (cdx_uint64)header[i] | ((cdx_uint64)header[i+1]<<8)
                |((cdx_uint64)header[i+2]<<16) | ((cdx_uint64)header[i+3]<<24)
                |((cdx_uint64)header[i+4]<<32) | ((cdx_uint64)header[i+5]<<40)
                |((cdx_uint64)header[i+6]<<48) | ((cdx_uint64)header[i+7]<<56);   //* Object size

        i += 8;

        if((0x6cce6200aa00d9a6ULL == guid_1) && (0x11cf668e75b22630ULL == guid_2) )
      /* ASF_HEADER_Object*/
        {
            logv("header object");
        }
        else if ((0x6cce6200aa00d9a6ULL == guid_1) && (0x11cf668e75b22636ULL == guid_2))
      /*ASF_Data_Object*/
        {
            logv("data object");
        }
        else if((0x6553200cc000e48eULL == guid_1) && (0x11cfa9478cabdca1ULL == guid_2))
      /*ASF_File_Properties_Object*/
        {
            packetLength = Get32(header, i+92-24);
         //*  Minimum Data Packet Size in ASF_File_Properties_Object
            logv("file property object");
        }
        else if((0x6553200cc000e68eULL == guid_1) && (0x11cfa9b7b7dc0791ULL == guid_2))
      /*ASF_Stream_Properties_Object*/
        {
            streamId = header[i+48] | header[i+49] << 8;   // Flag(bit0-6 is stream num)
            if(mmsStreamInf->numStreamIds < MMST_MAX_STREAMS)
            {
                mmsStreamInf->streamIds[mmsStreamInf->numStreamIds] = streamId;
                mmsStreamInf->numStreamIds++;
            }
            else
            {
                //mp_msg(MSGT_NETWORK,MSGL_ERR,MSGTR_MPDEMUX_MMST_2ManyStreamID);
            }

        }
        else
        {
           loge("unkown object");
        }
        i += length-24;
    }
    return packetLength;
}

//*****************************************************************************//
//*****************************************************************************//

int MmstStreamingStart(char *uri, aw_mms_inf_t* mmsStreamInf)
{
    int     i = 0;
    int     fs = 0;
    int     len = 0;
    char    str[1024];
    char    sendData[MMST_BUF_SIZE];
    int     asfHeaderLen = 0;
    int     packetLength = 0;;
    char    *path = NULL;
    char    *unescpath = NULL;
    CdxUrlT* url = NULL;
    int ret;

    CDX_UNUSE(uri);

    url = mmsStreamInf->awUrl;
    /* parse url, get the name of play media */
    path = strchr(url->file,'/') + 1;

    // mmst filename are not url_escaped by MS MediaPlayer and are expected as
    // "plain text" by the server, so need to decode it here
    unescpath = malloc(strlen(path)+1);
    if(!unescpath)
    {
        CDX_LOGE("******************unescpath faile.\n");
        return -1;
    }
    CdxUrlUnescapeString(unescpath,path);
    path = unescpath;
    if(url->port == 0)
    {
        url->port = 1755;
    }

    fs = Connect2Server(mmsStreamInf, url->hostname, url->port, 1);
    if(fs < 0)
    {
        free(path);
        return fs;
    }
    mmsStreamInf->seqNum = 0;

 /*
   * *******************************************************************************
   *                                       The First Interaction of client and server
   *
   * Send the initial connect info including player version no.
   Client GUID (random) and the host address being connected to.
   * This command is sent at the very start of protocol initiation.
       It sends local information to the serve cmd 1 0x01
   * ********************************************************************************
   */

    /* prepare for the url encoding conversion */
    /* NSPlayer/version；<spcae>{128bit client GUID}; <space>Host:<space>server IP address*/
    snprintf(str, 1023, "\034\003NSPlayer/7.0.0.1956; \
            {33715801-BAB3-9D85-24E9-03B90328270A}; Host: %s", url->hostname);

    String2Utf16 (sendData,str, strlen(str)); /* string to utf16 */
    //send_command(s, commandno ....)
    //* first packet: client send to server (tell server the player version)
    //* fs is server socket
    //* void MmstSendCommand(int* seqNum,int s, int command, unsigned int switches,
    //unsigned int extra, int length, unsigned char *sendData)
    ret = MmstSendCommand(&mmsStreamInf->seqNum, fs, 1, 0, 0x0004000b, strlen(str)*2+2,
                          (unsigned char*)sendData);
    if(ret < 0)
    {
        CDX_LOGW("--- send command error!");
        return -1;
    }

    /* client recv first packet from server, (server version and encryption protocol ) */
    len = MmsGetNetworkData(mmsStreamInf,fs, sendData, MMST_BUF_SIZE);
    if(len <= 0)
    {
        closesocket(fs);
        return -1;
    }
    CDX_LOGD("first command");

 /*
  **************************************************************************************
  *                                      The Second Interaction
  *  This sends details of the local machine IP address to a Funnel system at the server.
  *  Also, the TCP or UDP transport selection is sent.
  *
  *  here 192.168.0.1 is local ip address TCP/UDP states the tronsport we r using
  *  and 1037 is the  local TCP or UDP socket number cmd 2 0x02
  *************************************************************************************
  */
    String2Utf16(&sendData[8], "\002\000\\\\192.168.0.1\\TCP\\1037", 24);
    memset (sendData, 0, 8);
    /* send the second packet ( client transport protocol, IP, port)*/
    MmstSendCommand(&mmsStreamInf->seqNum, fs, 2, 0, 0, 24*2+10, (unsigned char*)sendData);
    /* recv the second packet ( receive the transport protocol )*/
    len = MmsGetNetworkData(mmsStreamInf,fs, sendData, MMST_BUF_SIZE);
    if(len <= 0)
    {
        closesocket(fs);
        return -1;
    }
    CDX_LOGD("---- second command");

 /*
  ****************************************************************************************
  *                                              The Third Interaction
  * C->S: This command sends file path (at server) and file name request to the server.* 0x5
  *
  *  S->C:  get the broadcast type (vod or live), file length, media packet length
  *
  ****************************************************************************************
  */
    String2Utf16(&sendData[8], path, strlen(path));
    memset(sendData, 0, 8);
    MmstSendCommand(&mmsStreamInf->seqNum,fs, 5, 0, 0, strlen(path)*2+10, (unsigned char*)sendData);
    free(path);
    if(MmstGetAnswer(mmsStreamInf, fs)== -1)
   //if the command in the server send packet is 0x1b(unkown command), deal it
    {
        closesocket(fs);
        return -1;
    }

 /*
  *****************************************************************************************
  *                          The FOURCE Interaction
  * The ASF header chunk request. Includes ?session' variable for pre header value.
  * After this command is sent,
  * the server replies with 0x11 command and then the header chunk with header sendData follows.
  * 0x15
  *****************************************************************************************
  */
    memset (sendData, 0, 40);
    sendData[32] = 2; //header packet id type
    MmstSendCommand(&mmsStreamInf->seqNum,fs, 0x15, 1, 0, 40, (unsigned char*)sendData);
    mmsStreamInf->numStreamIds = 0;  //?
    //recv the packet header of requesting header response, then recv the asf header
    asfHeaderLen = MmstGetHeader(mmsStreamInf, fs,(unsigned char*)mmsStreamInf->buffer);

    if(asfHeaderLen == 0) //error reading header
    {
        closesocket(fs);
        return -1;
    }
    mmsStreamInf->bufWritePtr += asfHeaderLen;
    mmsStreamInf->validDataSize += asfHeaderLen;
    mmsStreamInf->bufferDataSize += asfHeaderLen;
    mmsStreamInf->buf_pos += asfHeaderLen;

    packetLength = MmstInterpHeader(mmsStreamInf,(unsigned char*)mmsStreamInf->buffer,asfHeaderLen);
    mmsStreamInf->packetDataLen = packetLength;
   //* Minimum Data Packet Size in ASF_File_Properties_Object

 /*
  ******************************************************************************************
  *                                  The FIFTH
  * This command is the media stream MBR selector. Switches are always 6 bytes in length.
  * After all switch elements, sendData ends with bytes [00 00] 00 20 ac 40 [02].
  * Where:
  * [00 00] shows 0x61 0x00 (on the first 33 sent) or 0xff 0xff for ASF files,
     and with no ending sendData for WMV files.
  * It is not yet understood what all this means.
  * And the last [02] byte is probably the header ?session' value.
  *
  *  0x33
  ********************************************************************************************
  */
    memset (sendData, 0, 40);

    {
        for(i=1; i<mmsStreamInf->numStreamIds; i++)
        {
            sendData [ (i-1) * 6 + 2 ] = 0xFF;
            sendData [ (i-1) * 6 + 3 ] = 0xFF;
            sendData [ (i-1) * 6 + 4 ] = mmsStreamInf->streamIds[i];
            sendData [ (i-1) * 6 + 5 ] = 0x00;
        }
        MmstSendCommand(&mmsStreamInf->seqNum, fs, 0x33, mmsStreamInf->numStreamIds,
                   0xFFFF|mmsStreamInf->streamIds[0]<<16,
                         (mmsStreamInf->numStreamIds-1)*6+2, (unsigned char*)sendData);
    }
    MmstGetAnswer(mmsStreamInf, fs);

 /*
  *********************************************************************************************
  *                        The SIX
  * Start sending file from packet xx.
  * This command is also used for resume downloads or requesting a lost packet.
  * Also used for seeking by sending a play point value which seeks to the media time point.
  * Includes ?session' value in pre header and the maximum media stream time.
  * 0x07
  **********************************************************************************************
  */
    memset (sendData, 0, 40);
    for(i=8; i<16; i++)
    {
        sendData[i] = 0xFF;
    }
    sendData[20] = 0x04;
    MmstSendCommand(&mmsStreamInf->seqNum,fs, 0x07, 1, 0xFFFF
               | mmsStreamInf->streamIds[0] << 16, 24, (unsigned char*)sendData);
    len = MmsGetNetworkData(mmsStreamInf, fs,sendData, MMST_BUF_SIZE);
    if(len <= 0)
    {
        closesocket(fs);
        return -1;
    }
    mmsStreamInf->fd = fs;

   return mmsStreamInf->fd;
}

static int MmstStreamingRead(aw_mms_inf_t* mmsStreamInf)
{
    unsigned char  preHeader[8];
    int            packetLen = 0;
    int            command = 0;
    int            dataSize = 0;
    unsigned char  data[MMST_BUF_SIZE];
    int            remainSize = 0;
    int            ret = STREAM_READ_OK;

    /*
     * the structure of the packet with the asf data payload is as follow:
     * sequence number (4 bytes)
     * packet ID type(1 byte)
     * TCP flag(Middle of packet series)(1 byte)
     * packet length(2 bytes)
     */
    dataSize = GetData(mmsStreamInf, mmsStreamInf->fd, preHeader, 8);
    if(dataSize < 8)
    {
        CDX_LOGW("****************MMST_PreHeader Read Failed\n");
        ret = STREAM_READ_END;
        goto failed;
    }

    if(preHeader[4] == 0x04)  // Data packet
    {
        packetLen = (preHeader[7]<<8|preHeader[6])- 8;
        if(packetLen < 0 || packetLen > MMST_BUF_SIZE)
        {
             CDX_LOGW("MSGTR_MPDEMUX_MMST_InvalidMMSPacketSize");
             ret = STREAM_READ_ERROR;
             CDX_LOGW("************here1:mms packet len error.packetLen=%d\n", packetLen);
             goto failed;
        }
        while(mmsStreamInf->exitFlag == 0)
        {
            //if cache size is not enough, wait 40 ms
            if(mmsStreamInf->packetDataLen
            > (mmsStreamInf->stream_buf_size-mmsStreamInf->validDataSize))
            {
                usleep(40*1000);
                continue;
            }
            break;
        }
        if(mmsStreamInf->exitFlag == 1)
        {
             ret = STREAM_READ_END;
             goto failed;
        }

        if(mmsStreamInf->bufWritePtr <= mmsStreamInf->bufReleasePtr)
        {
            //* read the packet data, and make the size of Data Object equal to packetDataLen
            dataSize = GetData(mmsStreamInf, mmsStreamInf->fd,
                           (unsigned char*)mmsStreamInf->bufWritePtr, packetLen);
            memset(mmsStreamInf->bufWritePtr+dataSize, 0, mmsStreamInf->packetDataLen-dataSize);
        }
        else
        {
            remainSize = mmsStreamInf->bufEndPtr+1-mmsStreamInf->bufWritePtr;
            if(remainSize >=  mmsStreamInf->packetDataLen)
            {
                 dataSize = GetData(mmsStreamInf, mmsStreamInf->fd,
                            (unsigned char*)mmsStreamInf->bufWritePtr,packetLen);
                 memset(mmsStreamInf->bufWritePtr+dataSize, 0,
                    mmsStreamInf->packetDataLen-dataSize);
            }
            else
            {
                 memset(data, 0, mmsStreamInf->packetDataLen);
                 dataSize = GetData(mmsStreamInf, mmsStreamInf->fd,(unsigned char*)data,packetLen);
                 memcpy(mmsStreamInf->bufWritePtr, data, remainSize);
                 memcpy(mmsStreamInf->buffer, data+remainSize,
                    mmsStreamInf->packetDataLen-remainSize);
            }
        }

        mmsStreamInf->bufWritePtr+= mmsStreamInf->packetDataLen;
        if(mmsStreamInf->bufWritePtr > mmsStreamInf->bufEndPtr)
        {
            mmsStreamInf->bufWritePtr -= mmsStreamInf->stream_buf_size;
        }

        pthread_mutex_lock(&mmsStreamInf->bufferMutex);
        mmsStreamInf->bufferDataSize += mmsStreamInf->packetDataLen;
        mmsStreamInf->buf_pos += mmsStreamInf->packetDataLen;
        mmsStreamInf->validDataSize  += mmsStreamInf->packetDataLen;
        pthread_mutex_unlock(&mmsStreamInf->bufferMutex);
        if(dataSize < packetLen)
        {
            return STREAM_READ_END;
        }
        return STREAM_READ_OK;
    }
    else   // Command packet
    {
        if (!GetData (mmsStreamInf, mmsStreamInf->fd, (unsigned char*)&packetLen, 4))
        {
            CDX_LOGW("MMST_packet_len Read Failed");
            ret = STREAM_READ_END;
            goto failed;
        }
        packetLen = Get32((unsigned char*)&packetLen, 0) + 4;
        if(packetLen < 0 || packetLen > MMST_BUF_SIZE)
        {
             CDX_LOGW("************here2:mms packet len error.packetLen=%d\n", packetLen);
             ret = STREAM_READ_ERROR;
             goto failed;
        }

        if(!GetData (mmsStreamInf, mmsStreamInf->fd, data, packetLen))
        {
            ret = STREAM_READ_END;
            goto failed;
        }

        if ((preHeader[7] != 0xb0) || (preHeader[6] != 0x0b)
        || (preHeader[5] != 0xfa) || (preHeader[4] != 0xce))
        {

            return STREAM_READ_INVALID;
        }
        command = Get32 (data, 24) & 0xFFFF;
        if(command == 0x1b)
        {
            MmstSendCommand(&mmsStreamInf->seqNum,mmsStreamInf->fd, 0x1b, 0, 0, 0, data);
        }
        else if(command == 0x1e)
        {
            return STREAM_READ_INVALID;
        }
        else if(command == 0x21)
        {
            // Looks like it's new in WMS9 Unknown command, but ignoring it seems to work.
            return STREAM_READ_INVALID;
        }
        else if (command != 0x05)
        {
            // mp_msg(MSGT_NETWORK,MSGL_ERR,MSGTR_MPDEMUX_MMST_UnknownCmd,command);
            return STREAM_READ_INVALID;
        }
    }

 failed:
     CDX_LOGW("failed \n");
    if(ret==STREAM_READ_END ||ret ==STREAM_READ_ERROR)
    {
        closesocket(mmsStreamInf->sockFd);
        return ret;
    }
    return STREAM_READ_OK;
}

int AsfStreamStart(char *uri, aw_mms_inf_t* mmsStreamInf)
{
    char *proto = NULL;
    int fd = -1;
    int port = 0;
    CdxUrlT *url = NULL;

    url = CdxUrlNew(uri);
    if(NULL == url)
    {
        return -1;
    }

    mmsStreamInf->awUrl = CdxCheck4Proxies(url);
    CdxUrlFree(url);
    port = mmsStreamInf->awUrl->port;
    proto = mmsStreamInf->awUrl->protocol;   //* scheme

    //if the scheme is mms, we can not sure which one it is, mmsh or mmst
    // so we try mmst first then mmsh
    //Is protocol mms or mmst?
    if (!strcasecmp(proto, "mmst") || !strcasecmp(proto, "mms"))
    {
        CDX_LOGD("--- Trying mmst");
        fd = MmstStreamingStart(uri, mmsStreamInf);
        mmsStreamInf->awUrl->port = port;
        mmsStreamInf->mmsMode = PROTOCOL_MMS_T;
        mmsStreamInf->fd = fd;
        if (fd > -1)
        {
            return 0;
        }
        CDX_LOGW("*******************ASF/TCP failed, is not mmst.\n");

        if (fd == -2)
        {
            return -1;
        }
    }

    //Is protocol  mmsh?
    if (!strcasecmp(proto, "mmsh") || !strcasecmp(proto, "mmshttp") || !strcasecmp(proto, "mms"))
    {
        CDX_LOGD("********************Trying ASF/MMSH...\n");
        fd = MmshStreamingStart(uri, mmsStreamInf);
        mmsStreamInf->awUrl->port = port;
        mmsStreamInf->mmsMode = PROTOCOL_MMS_H;
        mmsStreamInf->fd = fd;
        if(fd > -1)
        {
            return 0;
        }
        CDX_LOGD("*********************ASF/MMSH failed\n");
        if(fd == -2)
        {
            return -1;
        }
    }

#if 0
    //Is protocol http or http_proxy
    if (!strcasecmp(proto, "http_proxy") || !strcasecmp(proto, "http") )
    {
           fd = aw_mms_http_streaming_start(datasource, mmsStreamInf);
           mmsStreamInf->awUrl->port = port;
        mmsStreamInf->mmsMode = PROTOCOL_MMS_HTTP;
        mmsStreamInf->fd = fd;
           //stream->bitrate = 300*1024;
           if(fd > -1)
        {
               return &(mmsStreamInf->streaminfo);
           }
           CDX_LOGI("*********************ASF/HTTP failed\n");
           if(fd == -2)
        {
               return NULL;
           }
       }
#endif
#if 0
    // Is protocol mms or mmsu?
    if(!strcasecmp(proto, "mmsu") || !strcasecmp(proto, "mms"))
    {
        //fd = aw_asf_mmsu_streaming_start(mmsStreamInf );
      //mmsu support is not implemented yet - using this code
        if(fd > -1)
        {
            return fd;
        }
        if(fd==-2)
        {
            return -1;
        }
    }
#endif

    return -1;
}

void* CdxReadAsfStream(void* p_arg)
{
    int ret = 0;
    int getProbeData = 1;
    aw_mms_inf_t* mmsStreamInf = (aw_mms_inf_t*)p_arg;
    aw_mmsh_ctrl_t *asfHttpCtrl  = NULL;

    CdxAtomicInc(&mmsStreamInf->ref);
    ret = AsfStreamStart(mmsStreamInf->uri, mmsStreamInf);
    CdxAtomicSet(&mmsStreamInf->mState, MMS_STREAM_IDLE);
    asfHttpCtrl= (aw_mmsh_ctrl_t*)mmsStreamInf->data;
    if( ret < 0)
    {
        CdxAtomicDec(&mmsStreamInf->ref);
        pthread_mutex_lock(&mmsStreamInf->lock);
        mmsStreamInf->iostate = CDX_IO_STATE_ERROR;
        pthread_mutex_unlock(&mmsStreamInf->lock);
        mmsStreamInf->eof = 1;
        return NULL;
    }

    while(1)
    {
        //
        if(mmsStreamInf->closeFlag == 1)
        {
            pthread_mutex_lock(&mmsStreamInf->lock);
            mmsStreamInf->iostate = CDX_IO_STATE_ERROR;
            pthread_mutex_unlock(&mmsStreamInf->lock);
            break;
        }

        if( (mmsStreamInf->stream_buf_size - mmsStreamInf->validDataSize) < 3*1024*1024)
      /* buffer 剩余空间不足*/
        {
            usleep(40*1000);
            continue;
        }

        // creat a thread to read the stream data
        if(mmsStreamInf->mmsMode == PROTOCOL_MMS_T)
        {
            ret = MmstStreamingRead(mmsStreamInf);
        }
        else if(mmsStreamInf->mmsMode == PROTOCOL_MMS_H)
        {
            if((asfHttpCtrl->streamingType==ASF_TYPE_PLAINTEXT)
            || (asfHttpCtrl->streamingType==ASF_TYPE_REDIRECTOR))
            {
                ret = MmshNopStreamingReadFunc(mmsStreamInf, 0);
            }
            else
            {
                ret = MmshStreamingReadFunc(mmsStreamInf, 0);
            }
        }
        else
        {
            ///.... "http"
            //ret = aw_mms_http_streaming_read_func(mmsStreamInf);
        }

        if(ret == STREAM_READ_END)
        {
            pthread_mutex_lock(&mmsStreamInf->lock);
            mmsStreamInf->iostate = CDX_IO_STATE_EOS;
            pthread_mutex_unlock(&mmsStreamInf->lock);
            CDX_LOGW("read the stream end.\n");
            break;
        }
        else if(ret == STREAM_READ_ERROR)
        {
            pthread_mutex_lock(&mmsStreamInf->lock);
            mmsStreamInf->iostate = CDX_IO_STATE_ERROR;
            pthread_mutex_unlock(&mmsStreamInf->lock);
            CDX_LOGE("read the stream error.\n");
            break;
        }

        if(getProbeData  && mmsStreamInf->validDataSize >= MMS_PROBE_DATA_LEN)
        {
            memcpy(mmsStreamInf->probeData.buf, "mms-allwinner", 13);
            memcpy(mmsStreamInf->probeData.buf+13, mmsStreamInf->buffer, MMS_PROBE_DATA_LEN-13);
            getProbeData = 0;
            pthread_mutex_lock(&mmsStreamInf->lock);
            mmsStreamInf->iostate = CDX_IO_STATE_OK;
            pthread_cond_signal(&mmsStreamInf->cond);
            pthread_mutex_unlock(&mmsStreamInf->lock);
        }

    }

    CdxAtomicDec(&mmsStreamInf->ref);
    CDX_LOGD("---read thread ref = %d", CdxAtomicRead(&mmsStreamInf->ref));
    mmsStreamInf->eof = 1;
    return NULL;
}

