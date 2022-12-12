/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxMkvParser.c
 * Description : MkvParser
 * History :
 *
 */

#include <stdint.h>
#include <CdxMkvParser.h>
#include "zlib.h"

#define PLAY_AUDIO_BITSTREAM    (1)
#define PLAY_VIDEO_BITSTREAM    (1)
#define PLAY_SUBTITILE_BITSTREAM    (1)

enum CdxMkvStatusE
{
    CDX_MKV_INITIALIZED, /*control , getMediaInfo, not prefetch or read, seekTo,.*/
    //CDX_PSR_prepared,
    CDX_MKV_IDLE,
    CDX_MKV_PREFETCHING,
    CDX_MKV_PREFETCHED,
    CDX_MKV_READING,
    CDX_MKV_SEEKING,
    CDX_MKV_EOS,
};

static cdx_uint32 GetBe16(char *s)
{
    cdx_uint32 val;
    val = (cdx_uint32)(*s) << 8;
    s += 1;
    val |= (cdx_uint32)(*s);
    return val;
}

static cdx_uint32 GetBe32(char *s)
{
    cdx_uint32 val;
    val = GetBe16(s) << 16;
    s += 2 ;
    val |= GetBe16(s);
    return val;
}

int MKVSetUserSubtitleStreamSel(void *pMkvInfo, int SubArrayIdx)
{
    struct CdxMkvParser *pMkvPsr = (struct CdxMkvParser*)pMkvInfo;
    MatroskaDemuxContext *matroska = NULL;
    if(NULL == pMkvPsr || NULL == pMkvPsr->priv_data)
    {
        CDX_LOGW("pMkvPsr==NULL\n");
        return -1;
    }

    matroska = (MatroskaDemuxContext*)pMkvPsr->priv_data;

    CDX_LOGW("SubArrayIdx=[%d]\n", SubArrayIdx);
    if((SubArrayIdx>=0) && (SubArrayIdx< pMkvPsr->nhasSubTitle))
    {
        pMkvPsr->UserSelSubStreamIdx = matroska->SubTitleStream[SubArrayIdx];
        pMkvPsr->SubTitleStreamIndex = pMkvPsr->UserSelSubStreamIdx;
        matroska->SubTitleStreamIndex = pMkvPsr->UserSelSubStreamIdx;
        matroska->SubTitleTrackIndex = matroska->SubTitleTrack[SubArrayIdx];
        pMkvPsr->tFormat.eCodecFormat = matroska->SubTitleCodecType[SubArrayIdx];
        pMkvPsr->tFormat.eTextFormat = matroska->SubTitledata_encode_type[SubArrayIdx];
        pMkvPsr->SubStreamSyncFlg = 1;

        return 0;
    }

    CDX_LOGW("not find stream_idx matching array, wrong! check!\n");
    return -1;
}

static cdx_uint8 asc2byte(cdx_uint8 data0, cdx_uint8 data1)
{
    data0 -= (data0 > 0x40 ? 0x57 : 0x30);
    data1 -= (data1 > 0x40 ? 0x57 : 0x30);
    return (data0<<4)+data1;
}

static cdx_int32 generate_rv_init_info(VideoStreamInfo* codec_fmt, struct CdxMkvParser* tmpMkvPsr)
{
    cdx_uint8*                    ptr;
    __vdec_rxg2_init_info      s;
    cdx_uint32                 ulSPOExtra;
    cdx_uint32                    ulStreamVersion;
    cdx_uint32                 ulMajorStreamVersion;
    cdx_uint32                 ulMinorStreamVersion;
    __rv_format_info         video_info;
    MatroskaDemuxContext*     matroska;

    memset(&video_info, 0x00, sizeof(__rv_format_info));

    matroska = (MatroskaDemuxContext *)tmpMkvPsr->priv_data;
    ptr = (unsigned char*)matroska->streams[tmpMkvPsr->VideoStreamIndex]->extradata;

    video_info.ulLength             = ptr[0]<<24 | ptr[1]<<16 | ptr[2]<<8 | ptr[3];
    ptr += 4;
    video_info.ulMOFTag             = ptr[0]<<24 | ptr[1]<<16 | ptr[2]<<8 | ptr[3];
    ptr += 4;
    video_info.ulSubMOFTag          = ptr[0]<<24 | ptr[1]<<16 | ptr[2]<<8 | ptr[3];
    ptr += 4;
    video_info.usWidth              = ptr[0]<<8 | ptr[1];
    ptr += 2;
    video_info.usHeight             = ptr[0]<<8 | ptr[1];
    ptr += 2;
    video_info.usBitCount           = ptr[0]<<8 | ptr[1];
    ptr += 2;
    video_info.usPadWidth           = ptr[0]<<8 | ptr[1];
    ptr += 2;
    video_info.usPadHeight          = ptr[0]<<8 | ptr[1];
    ptr += 2;
    video_info.ufFramesPerSecond    = ptr[0]<<24 | ptr[1]<<16 | ptr[2]<<8 | ptr[3];
    ptr += 4;

    video_info.ulOpaqueDataSize     = video_info.ulLength - 26;
    video_info.pOpaqueData          = ptr;

    // Fix up the subMOF tag
    if(video_info.ulSubMOFTag == HX_RVTRVIDEO_ID)
        video_info.ulSubMOFTag = HX_RV20VIDEO_ID;
    else if(video_info.ulSubMOFTag == HX_RVTR_RV30_ID)
        video_info.ulSubMOFTag = HX_RV30VIDEO_ID;

    tmpMkvPsr->vFormat.nFrameRate        = (int)(video_info.ufFramesPerSecond >> 16);
    // for frame_rate = 23.97, we set the frame rate to 24.
    if (((video_info.ufFramesPerSecond & 0xffff) != 0) && (tmpMkvPsr->vFormat.nFrameRate == 0x17))
        tmpMkvPsr->vFormat.nFrameRate++;
    if (tmpMkvPsr->vFormat.nFrameRate == 0)
        tmpMkvPsr->vFormat.nFrameRate = 30;

    //tmpMkvPsr->vFormat.nMicSecPerFrame   = 1000 * 1000 / tmpMkvPsr->vFormat.xFramerate;
    matroska->streams[matroska->VideoStreamIndex]->default_duration =
            1000 * 1000 / tmpMkvPsr->vFormat.nFrameRate;
    tmpMkvPsr->vFormat.nFrameRate       *= 1000;

    switch (video_info.ulSubMOFTag)
    {
        case HX_RV10VIDEO_ID:
        case HX_RV20VIDEO_ID:
        case HX_RVG2VIDEO_ID:
        {
            //tmpMkvPsr->vFormat.codec_type = VIDEO_RXG2;

            ulSPOExtra           = ptr[0]<<24 | ptr[1]<<16 | ptr[2]<<8 | ptr[3];
            ulStreamVersion      = ptr[4]<<24 | ptr[5]<<16 | ptr[6]<<8 | ptr[7];
            ulMajorStreamVersion = (ulStreamVersion>>28) & 0x0f;
            ulMinorStreamVersion = (ulStreamVersion>>20) & 0xff;
            s.uNum_RPR_Sizes     = 0;

            if(!(ulMinorStreamVersion & RAW_BITSTREAM_MINOR_VERSION))
            {
                if (ulMajorStreamVersion == RV20_MAJOR_BITSTREAM_VERSION ||
                    ulMajorStreamVersion == RV30_MAJOR_BITSTREAM_VERSION)
                {
                    if ((ulSPOExtra&RV40_SPO_BITS_NUMRESAMPLE_IMAGES) >>
                        RV40_SPO_BITS_NUMRESAMPLE_IMAGES_SHIFT)
                    {
                        s.uNum_RPR_Sizes = ((ulSPOExtra&RV40_SPO_BITS_NUMRESAMPLE_IMAGES) >>
                                RV40_SPO_BITS_NUMRESAMPLE_IMAGES_SHIFT) + 1;
                    }
                }
            }

            s.bUMV             = (ulSPOExtra & ILVC_MSG_SPHI_Unrestricted_Motion_Vectors) ? 1 : 0;
            s.bAP              = (ulSPOExtra & ILVC_MSG_SPHI_Advanced_Prediction) ? 1 : 0;
            s.bAIC             = (ulSPOExtra & ILVC_MSG_SPHI_Advanced_Intra_Coding) ? 1 : 0;
            s.bDeblocking      = (ulSPOExtra & ILVC_MSG_SPHI_Deblocking_Filter) ? 1: 0;
            s.bSliceStructured = (ulSPOExtra & ILVC_MSG_SPHI_Slice_Structured_Mode) ? 1 : 0;
            s.bRPS             = (ulSPOExtra & ILVC_MSG_SPHI_Reference_Picture_Selection) ? 1 : 0;
            s.bISD             = (ulSPOExtra & ILVC_MSG_SPHI_Independent_Segment_Decoding) ? 1: 0;
            s.bAIV             = (ulSPOExtra & ILVC_MSG_SPHI_Alternate_Inter_VLC) ? 1 : 0;
            s.bMQ              = (ulSPOExtra & ILVC_MSG_SPHI_Modified_Quantization) ? 1 : 0;
            s.RmCodecID        = CODEC_ID_RV20;
            s.FormatPlus       = EPTYPE;

            if (video_info.usWidth == 128 && video_info.usHeight == 96)
                s.FormatPlus = SQCIF;
            else if (video_info.usWidth == 176 && video_info.usHeight == 144)
                s.FormatPlus = QCIF;
            else if (video_info.usWidth == 352 && video_info.usHeight == 288)
                s.FormatPlus = CIF;
            else if (video_info.usWidth == 704 && video_info.usHeight == 576)
                s.FormatPlus = fCIF;
            else if (video_info.usWidth == 1408 && video_info.usHeight == 1152)
                s.FormatPlus = ssCIF;
            else if (video_info.usWidth == 0 && video_info.usHeight == 0)
                s.FormatPlus = FORBIDDEN;
            else
                s.FormatPlus = CUSTOM;

            switch(ulMajorStreamVersion)
            {
                case 0:
                {
                    if(ulMinorStreamVersion != 0)
                        return -1;
                    else
                        s.iRvSubIdVersion = PIA_FID_REALVIDEO21;
                    break;
                }

                case 2:
                {
                    if (ulMinorStreamVersion == 2)
                        s.iRvSubIdVersion = PIA_FID_REALVIDEO22;
                    else if (ulMinorStreamVersion == 1)
                        s.iRvSubIdVersion = PIA_FID_REALVIDEO21;
                    else if (ulMinorStreamVersion == 128)
                        s.iRvSubIdVersion = PIA_FID_H263PLUS;
                    else
                        return -1;

                    break;
                }

                case 3:
                {
                    if(ulMinorStreamVersion==2)
                        s.iRvSubIdVersion = PIA_FID_REALVIDEO22;

                    break;
                }

                case 4:
                {
                    if(ulMinorStreamVersion != 4)
                        return -1;
                    else
                        s.iRvSubIdVersion = PIA_FID_H263PLUS;

                    break;
                }

                default:
                    return -1;
            }

            codec_fmt->pCodecSpecificData = malloc(sizeof(__vdec_rxg2_init_info));
            if(codec_fmt->pCodecSpecificData == NULL)
            {
                return -1;
            }

            memcpy(codec_fmt->pCodecSpecificData, &s, sizeof(__vdec_rxg2_init_info));
            codec_fmt->nCodecSpecificDataLen = sizeof(__vdec_rxg2_init_info);

            return 0;
        }

        case HX_RV30VIDEO_ID:
        case HX_RV40VIDEO_ID:
        case HX_RV89COMBO_ID:
        {
            //codec_fmt.codec_type = VIDEO_RMVB9;
            cdx_char *ptr;
            int datalen = sizeof(__rv_format_info) - sizeof(unsigned char*);
            codec_fmt->pCodecSpecificData =
                    malloc(datalen + video_info.ulOpaqueDataSize );
            if(codec_fmt->pCodecSpecificData == NULL)
            {
                return -1;
            }

            memcpy(codec_fmt->pCodecSpecificData, &video_info, datalen);

            ptr = codec_fmt->pCodecSpecificData + datalen;
            memcpy(ptr, video_info.pOpaqueData, video_info.ulOpaqueDataSize);
            codec_fmt->nCodecSpecificDataLen =
                    datalen + video_info.ulOpaqueDataSize;
            return 0;
        }

        default:
        {
            return -1;
        }
    }

    return 0;
}

static int __CdxMkvParserInit(CdxParserT *parser)
{
    int ret;
    struct CdxMkvParser* impl = (struct CdxMkvParser*)parser;

    CDX_LOGD("---- __CdxMkvParserInit");
    CdxAtomicInc(&impl->ref);

    //open media file to parse file information
    ret = CdxMatroskaOpen(impl);
    if(ret < 0)
    {
        CDX_LOGE("open mkv reader failed@");
        impl->mErrno = PSR_OPEN_FAIL;
        impl->mStatus = CDX_MKV_IDLE;
        CdxAtomicDec(&impl->ref);
        return -1;
    }

    CDX_LOGD("***** mkv open success!!");
    CdxAtomicDec(&impl->ref);
    impl->mErrno = PSR_OK;
    impl->mStatus = CDX_MKV_IDLE;
    return 0;
}

static cdx_int32 __CdxMkvParserClose(CdxParserT *parser)
{
CDX_LOGD("--- __CdxMkvParserClose");
    int ret;
    struct CdxMkvParser* impl = (struct CdxMkvParser*)parser;
    if(!impl)
    {
        return -1;
    }
    impl->exitFlag = 1;
    CdxStreamForceStop(impl->stream);
    CdxAtomicDec(&impl->ref);
    while ((ret = CdxAtomicRead(&impl->ref)) != 0)
    {
        CDX_LOGD("wait for ref = %d", ret);
        usleep(10000);
    }

    CdxMatroskaClose(impl);
    CdxMatroskaExit(impl);
    impl = NULL;
    return 0;
}

static cdx_int32 __CdxMkvParserPrefetch(CdxParserT *parser, CdxPacketT * pkt)
{
    int ret;
    struct CdxMkvParser* impl = (struct CdxMkvParser*)parser;
    if(!impl || !impl->priv_data)
    {
        return -1;
    }
    MatroskaDemuxContext *matroska = (MatroskaDemuxContext *)impl->priv_data;
    MatroskaTrack *track;

    if(impl->mErrno == PSR_EOS)
    {
        CDX_LOGW("--- EOS");
        return -1;
    }
    if((impl->mStatus != CDX_MKV_IDLE) && (impl->mStatus != CDX_MKV_PREFETCHED))
    {
        CDX_LOGW("the status of prefetch is error");
        impl->mErrno = PSR_INVALID_OPERATION;
        return -1;
    }

    if(impl->mStatus == CDX_MKV_PREFETCHED)
    {
        memcpy(pkt, &impl->packet, sizeof(CdxPacketT));
        return 0;
    }
    memset(pkt, 0, sizeof(CdxPacketT));
    impl->mStatus = CDX_MKV_PREFETCHING;

    if(matroska->slice_len != 0)
    {
        matroska->slice_len = 0;
    }

    do
    {
        ret = CdxMatroskaRead(impl);
        //CDX_LOGD("matroska->data_rdy = %d", matroska->data_rdy);
        if(ret < 0)
        {
            CDX_LOGD("---- io error, ret(%d)", ret);
            impl->mStatus = CDX_MKV_IDLE;
            impl->mErrno = PSR_IO_ERR;
            return ret;
        }
        else if(ret == 1)
        {
            CDX_LOGD("--- eos");
            impl->mErrno = PSR_EOS;
            impl->mStatus = CDX_MKV_IDLE;
            return -1;
        }

        if(matroska->t_size<=0)
        {
            matroska->data_rdy = 0;
        }

        if(matroska->chk_type == MATROSKA_TRACK_TYPE_AUDIO)
        {
            if(impl->flag & DISABLE_AUDIO)
            {
                matroska->data_rdy = 0;
            }
            if(PLAY_AUDIO_BITSTREAM == 0)
            {
                matroska->data_rdy = 0;
            }
            if(impl->bDiscardAud)
            {
                matroska->data_rdy = 0;
            }
        }
        else if(impl->hasVideo && matroska->track == matroska->VideoTrackIndex)
        {
            if(PLAY_VIDEO_BITSTREAM == 0)
            {
                matroska->data_rdy = 0;
            }
            if(impl->flag & DISABLE_VIDEO)
            {
                matroska->data_rdy = 0;
            }
        }
        else if(matroska->chk_type == MATROSKA_TRACK_TYPE_SUBTITLE)
        {
            if(impl->flag & DISABLE_SUBTITLE)
            {
                matroska->data_rdy = 0;
            }
        }
        else
        {
            matroska->data_rdy = 0;
        }
    }while(matroska->data_rdy == 0);

    track = matroska->tracks[matroska->track];
    //CDX_LOGD("--- matroska->prefetch_track_num= %d", matroska->track);

    pkt->type = CDX_MEDIA_UNKNOWN;
    pkt->length = matroska->t_size;
    pkt->pts = (cdx_int64)(matroska->time_stamp-matroska->time_stamp_offset);
    pkt->duration = (cdx_int64)(matroska->blk_duration/1000);

    if(matroska->chk_type == MATROSKA_TRACK_TYPE_VIDEO)
    {
        pkt->type = CDX_MEDIA_VIDEO;
        // TODO: is_keyframe is not right
        if(matroska->is_keyframe == PKT_FLAG_KEY)
        {
            // we should not set keyframe, it will abort in  frenquencely seek in network
            //pkt->flags = KEY_FRAME;
        }
        pkt->length += 8*1024;
    }
    else if(matroska->chk_type == MATROSKA_TRACK_TYPE_AUDIO)
    {
        pkt->streamIndex = track->nStreamIdx;
        pkt->type = CDX_MEDIA_AUDIO;
        pkt->length += matroska->a_Header_size + 2*1024;
    }
    else if(matroska->chk_type == MATROSKA_TRACK_TYPE_SUBTITLE)
    {
        pkt->streamIndex = track->nStreamIdx;
        pkt->type = CDX_MEDIA_SUBTITLE;

        if( matroska->SubTitleCodecType[track->nStreamIdx] == SUBTITLE_CODEC_DVDSUB
            || matroska->SubTitleCodecType[track->nStreamIdx]  == SUBTITLE_CODEC_PGS)
        {
            if(pkt->length < 64*1024)
            {
                pkt->length = 64*1024;
            }
        }
    }

    pkt->flags |= (FIRST_PART|LAST_PART);

    memcpy(&impl->packet, pkt, sizeof(CdxPacketT));
    //CDX_LOGD("type = %d, pkt->length= %d, pkt->pts = %lld,
    //streamIndex = %d", pkt->type, pkt->length, pkt->pts, pkt->streamIndex);
    impl->mStatus = CDX_MKV_PREFETCHED;
    return 0;
}

static cdx_int32 __CdxMkvParserRead(CdxParserT *parser, CdxPacketT *pkt)
{
    int ret;
    unsigned char                *tmpBuf;
    unsigned int                tmpBufSize;
    cdx_uint8    *pkt_buf0 = pkt->buf;
    cdx_int32     pkt_size0 = pkt->buflen;
    cdx_uint8    *pkt_buf1 = pkt->ringBuf;
    cdx_int32     pkt_size1 = pkt->ringBufLen;

    struct CdxMkvParser* impl = (struct CdxMkvParser*)parser;
    if(!impl || !impl->priv_data)
    {
        return -1;
    }
    MatroskaDemuxContext *matroska = (MatroskaDemuxContext *)impl->priv_data;
    //CdxStreamT* fp = matroska->fp;
    MatroskaTrack       *track = matroska->tracks[matroska->track];

    if(impl->mStatus != CDX_MKV_PREFETCHED)
    {
        impl->mErrno = PSR_INVALID_OPERATION;
        return -1;
    }

    if(matroska->t_size <= 0)
    {
        return 0;
    }

    if(matroska->chk_type != MATROSKA_TRACK_TYPE_VIDEO
        && matroska->chk_type != MATROSKA_TRACK_TYPE_AUDIO
        && matroska->chk_type != MATROSKA_TRACK_TYPE_SUBTITLE)
    {
        impl->mStatus = CDX_MKV_IDLE;
        return -1;
    }

    if(track->content_comp == MATROSKA_TRACK_ENCODING_COMP_HEADERSTRIP)
    {
         if(pkt_size0 ==0)
        {
            CDX_LOGV("pkt_size0=0 return 0");
            return 0;
         }
        if(track->comp_settings_len > pkt_size0)
        {
            memcpy(pkt_buf0, track->comp_settings, pkt_size0);
            memcpy(pkt_buf1, track->comp_settings + pkt_size0,
                   track->comp_settings_len - pkt_size0);
            pkt_buf0 = (unsigned char*)pkt_buf1 + (track->comp_settings_len - pkt_size0);
            pkt_size0 = pkt_size1 - (track->comp_settings_len - pkt_size0);
        }
        else
        {
            memcpy(pkt_buf0, track->comp_settings, track->comp_settings_len);
            if(track->comp_settings_len == pkt_size0)
            {
                pkt_buf0 = pkt_buf1;
                pkt_size0 = pkt_size1;
            }
            else
            {
                pkt_buf0 += track->comp_settings_len;
                pkt_size0 -= track->comp_settings_len;
            }
        }
        pkt->length = track->comp_settings_len;
    }
    else
    {
        pkt->length = 0;
    }

    //prcoess video frame information
    if(matroska->chk_type == MATROSKA_TRACK_TYPE_VIDEO)
    {
        //init video decode message
        if(matroska->r_size == 0)
        {
            //first time to get the chunk data, set information for video decoder

            if(track->codec_id == CODEC_ID_MPEG2VIDEO)
            {
                matroska->frm_num = 0;
            }
            //set frame decode mode
        }

        if(track->codec_id == CODEC_ID_RV10 ||
            track->codec_id == CODEC_ID_RV20 ||
            track->codec_id == CODEC_ID_RV30 ||
            track->codec_id == CODEC_ID_RV40)
        {
            cdx_uint32 i;

            tmpBuf = pkt_buf0;
            tmpBufSize = pkt_size0;

            matroska->pkt_data     -= 8;
            for(i=0; i<matroska->slice_offset; i++)
            {
                matroska->pkt_data[i] = matroska->pkt_data[i+8];
            }

            while(1)
            {
                if(matroska->slice_len>0)
                {
                    cdx_int32 c_size = matroska->slice_len > (int)tmpBufSize ?
                            (int)tmpBufSize : matroska->slice_len;
                    memcpy(tmpBuf, matroska->pkt_data + matroska->slice_offset, c_size);
                       pkt->length            += c_size;
                       tmpBuf              += c_size;
                       matroska->r_size        += c_size;
                       matroska->slice_offset     += c_size;
                       matroska->slice_len     -= c_size;
                       tmpBufSize                -= c_size;
                       if(matroska->slice_len>0)
                       {
                           tmpBuf = pkt_buf1;
                        tmpBufSize = pkt_size1;
                        cdx_int32 c_size = matroska->slice_len;
                        memcpy(tmpBuf, matroska->pkt_data + matroska->slice_offset, c_size);
                           pkt->length        += c_size;
                           tmpBuf                  += c_size;
                           matroska->r_size        += c_size;
                           matroska->slice_offset     += c_size;
                           matroska->slice_len     -= c_size;
                           tmpBufSize                -= c_size;
                       }
                }
                else
                {
                    matroska->slice_len = 0;
                    if(matroska->slice_cnt < matroska->slice_num)
                    {
                        matroska->slice_offset =
                                (matroska->pkt_data[5+(matroska->slice_cnt<<3)] <<  0)
                                + (matroska->pkt_data[6+(matroska->slice_cnt<<3)] <<  8)
                                + (matroska->pkt_data[7+(matroska->slice_cnt<<3)] << 16)
                                + (matroska->pkt_data[8+(matroska->slice_cnt<<3)] << 24) ;
                        matroska->slice_offset += 8 + 1 + (matroska->slice_num<<3);
                        if(matroska->slice_cnt >= (matroska->slice_num-1))
                        {
                            matroska->slice_len = matroska->t_size + 8;
                        }
                        else
                        {
                            matroska->slice_len =
                                    (matroska->pkt_data[8+5+(matroska->slice_cnt<<3)] <<  0)
                                    + (matroska->pkt_data[8+6+(matroska->slice_cnt<<3)] <<  8)
                                    + (matroska->pkt_data[8+7+(matroska->slice_cnt<<3)] << 16)
                                    + (matroska->pkt_data[8+8+(matroska->slice_cnt<<3)] << 24) ;
                            matroska->slice_len += 8 + 1 + (matroska->slice_num<<3);
                            if(matroska->slice_len >= (matroska->t_size + 8))
                            {
                                matroska->slice_len = matroska->t_size + 8;
                            }
                        }
                        matroska->slice_len -= matroska->slice_offset;
                        matroska->slice_cnt++;

                        if(matroska->pkt_data[1+(matroska->slice_cnt<<3)] == 1 ||
                           matroska->slice_len > 0)
                        {
                            matroska->slice_offset     -= 6;
                            matroska->pkt_data[matroska->slice_offset+0] = matroska->slice_num;
                            matroska->pkt_data[matroska->slice_offset+1] = matroska->slice_cnt;
                            matroska->pkt_data[matroska->slice_offset+2] =
                                                                    (matroska->slice_len>> 0)&0xff;
                            matroska->pkt_data[matroska->slice_offset+3] =
                                                                    (matroska->slice_len>> 8)&0xff;
                            matroska->pkt_data[matroska->slice_offset+4] =
                                                                    (matroska->slice_len>>16)&0xff;
                            matroska->pkt_data[matroska->slice_offset+5] =
                                                                    (matroska->slice_len>>24)&0xff;
                            matroska->slice_len += 6;
                        }
                        else
                        {
                            matroska->slice_len = 0;
                        }
                    }
                    else
                    {
                        ret = 1; //all data of the chunk has been got out
                        break;
                    }
                }
            }
        }
        else if(matroska->video_direct_copy)
        {
            matroska->FilePos = CdxStreamTell(matroska->fp);
            ret = CdxStreamSeek(matroska->fp, matroska->vDataPos-matroska->FilePos, SEEK_CUR);
            if(ret < 0)
            {
                impl->mStatus = CDX_MKV_IDLE;
                return -1;
            }

            pkt->length += matroska->t_size;

            if(matroska->t_size <= pkt_size0)
            {
                ret = CdxStreamRead(matroska->fp, pkt_buf0, matroska->t_size);
                if(ret < 0)
                {
                    impl->mStatus = CDX_MKV_IDLE;
                    return -1;
                }
            }
            else
            {
                ret = CdxStreamRead(matroska->fp, pkt_buf0,  pkt_size0);
                ret = CdxStreamRead(matroska->fp, pkt_buf1, matroska->t_size - pkt_size0);
                if(ret < 0)
                {
                    impl->mStatus = CDX_MKV_IDLE;
                    return -1;
                }
            }

            matroska->FilePos -= CdxStreamTell(matroska->fp);
            ret = CdxStreamSeek(matroska->fp, matroska->FilePos, SEEK_CUR);
            if(ret < 0)
            {
                impl->mStatus = CDX_MKV_IDLE;
                return -1;
            }
        }
        else
        {
            pkt->length += matroska->t_size;

            if(matroska->t_size <= pkt_size0)
            {
                memcpy(pkt_buf0, matroska->pkt_data, matroska->t_size);
            }
            else
            {
                memcpy(pkt_buf0, matroska->pkt_data, pkt_size0);
                memcpy(pkt_buf1, matroska->pkt_data + pkt_size0, matroska->t_size - pkt_size0);
            }
        }
    }

    if(matroska->chk_type == MATROSKA_TRACK_TYPE_AUDIO)
    {
        pkt->length += matroska->t_size;

        if(matroska->t_size <= pkt_size0)
        {
            memcpy(pkt_buf0, matroska->pkt_data, matroska->t_size);
        }
        else
        {
            memcpy(pkt_buf0, matroska->pkt_data, pkt_size0);
            memcpy(pkt_buf1, matroska->pkt_data + pkt_size0, matroska->t_size - pkt_size0);
        }
    }

    //prcoess video frame information
    if(matroska->chk_type == MATROSKA_TRACK_TYPE_SUBTITLE )
    {
        if(pkt_size0 == 0)
        {
            CDX_LOGV("matroska->chk_type=0x%x  pkt_size0=0 return 0",matroska->chk_type);
            return 0;
        }
        if(track->encoding_scope == 1)
        {
            if(track->content_comp == MATROSKA_TRACK_ENCODING_COMP_HEADERSTRIP)
            {
                 pkt->length += matroska->t_size;
                 memcpy(pkt_buf0, matroska->pkt_data, matroska->t_size);
            }
            else
            {
                cdx_int32 zlib_ret;
                tmpBufSize = pkt_size0;
                zlib_ret = decode_zlib_data(matroska->pkt_data, pkt_buf0,
                                            matroska->t_size, pkt_size0);
                if(zlib_ret > 0 && zlib_ret < (int)tmpBufSize)
                {
                    pkt->length += zlib_ret;
                }
            }
        }
        else
        {
            pkt->length += matroska->t_size;

            if(matroska->t_size <= pkt_size0)
            {
                memcpy(pkt_buf0, matroska->pkt_data, matroska->t_size);
            }
            else
            {
                memcpy(pkt_buf0, matroska->pkt_data, pkt_size0);
                memcpy(pkt_buf1, matroska->pkt_data + pkt_size0, matroska->t_size - pkt_size0);
            }
        }

        if(impl->SubStreamSyncFlg)
        {
            impl->SubStreamSyncFlg = 0;
            matroska->data_rdy = 0;
        }
    }

    if(matroska->lace_cnt >= matroska->laces)
    {
        matroska->data_rdy = 0;
    }

    //CDX_LOGD("type = %d, pkt->length= %d, pkt->pts = %lld, offset=%lld",
    //pkt->type, pkt->length, pkt->pts, CdxStreamTell(matroska->fp));

    impl->mStatus = CDX_MKV_IDLE;
    return 0;
}

static cdx_int32 __CdxMkvParserGetMediaInfo(CdxParserT *parser, CdxMediaInfoT * pMediaInfo)
{
    int i, j;
    struct CdxMkvParser* impl = (struct CdxMkvParser*)parser;
    if(!impl || !impl->priv_data)
    {
        return -1;
    }
    MatroskaDemuxContext *matroska = impl->priv_data;

    while(impl->mErrno != PSR_OK)
    {
        usleep(100);
        if(impl->exitFlag)
        {
            break;
        }
    }
    memset(pMediaInfo, 0, sizeof(CdxMediaInfoT));

    pMediaInfo->programNum = 1;
    pMediaInfo->programIndex = 0;
    //pMediaInfo->bSeekable = (matroska->num_indexes>0) ? 1 : 0;
    if(matroska->bitrate != 0 || matroska->num_indexes>0)
    {
        pMediaInfo->bSeekable = 1;
    }
    else
    {
        CDX_LOGW("cannot seek");
        pMediaInfo->bSeekable = 0;
    }
    pMediaInfo->fileSize = matroska->fileEndPst;
    CDX_LOGD("---- seekable = %d", pMediaInfo->bSeekable);

    pMediaInfo->program[0].audioNum    = impl->nhasAudio;
    pMediaInfo->program[0].videoNum    = impl->nhasVideo;
    pMediaInfo->program[0].subtitleNum = impl->nhasSubTitle;

    //set total time
    pMediaInfo->program[0].duration = matroska->segment_duration;

    //**< set the default stream index
    pMediaInfo->program[0].audioIndex    = 0;
    pMediaInfo->program[0].videoIndex    = 0;
    pMediaInfo->program[0].subtitleIndex = 0;

    //**< frameRate should multi 1000, (24000, 50000)
    //set video bitstream information
    memcpy(&pMediaInfo->program[0].video[0], &impl->vFormat, sizeof(VideoStreamInfo));
    CDX_LOGD("*********** video stream ************");
    CDX_LOGD("****eCodecFormat:           %x", impl->vFormat.eCodecFormat);
    CDX_LOGD("****nFrameRate:             %d", impl->vFormat.nFrameRate);
    CDX_LOGD("****nWidth:                 %d", impl->vFormat.nWidth);
    CDX_LOGD("****nHeight:                %d", impl->vFormat.nHeight);
    CDX_LOGD("****nCodecSpecificDataLen:  %d", impl->vFormat.nCodecSpecificDataLen);
    CDX_LOGD("************************************");

    //set audio bitstream information
    for(i=0; i<impl->nhasAudio; i++)
    {
        strcpy((char*)pMediaInfo->program[0].audio[i].strLang,
                (const char*)matroska->tracks[impl->AudioTrackIndex[i]]->language);

        pMediaInfo->program[0].audio[i].eDataEncodeType = SUBTITLE_TEXT_FORMAT_UTF8;
        pMediaInfo->program[0].audio[i].eCodecFormat = impl->aFormat_arry[i].eCodecFormat;
        pMediaInfo->program[0].audio[i].eSubCodecFormat = impl->aFormat_arry[i].eSubCodecFormat;
        pMediaInfo->program[0].audio[i].nChannelNum = impl->aFormat_arry[i].nChannelNum;
        pMediaInfo->program[0].audio[i].nAvgBitrate = impl->aFormat_arry[i].nAvgBitrate;
        pMediaInfo->program[0].audio[i].nMaxBitRate = impl->aFormat_arry[i].nMaxBitRate;
        pMediaInfo->program[0].audio[i].nSampleRate = impl->aFormat_arry[i].nSampleRate;
        pMediaInfo->program[0].audio[i].nBlockAlign = impl->aFormat_arry[i].nBlockAlign;
        pMediaInfo->program[0].audio[i].nBitsPerSample = impl->aFormat_arry[i].nBitsPerSample;
        pMediaInfo->program[0].audio[i].nCodecSpecificDataLen =
                                                    impl->aFormat_arry[i].nCodecSpecificDataLen;
        pMediaInfo->program[0].audio[i].pCodecSpecificData =
                                                    impl->aFormat_arry[i].pCodecSpecificData;

        CDX_LOGD("***********audio stream %d************", i);
        CDX_LOGD("****language:               %s", pMediaInfo->program[0].audio[i].strLang);
        CDX_LOGD("****eCodecFormat:           %d", impl->aFormat_arry[i].eCodecFormat);
        CDX_LOGD("****eSubCodecFormat:        %d", impl->aFormat_arry[i].eSubCodecFormat);
        CDX_LOGD("****nChannelNum:            %d", impl->aFormat_arry[i].nChannelNum);
        CDX_LOGD("****nAvgBitrate:            %d", impl->aFormat_arry[i].nAvgBitrate);
        CDX_LOGD("****nMaxBitRate:            %d", impl->aFormat_arry[i].nMaxBitRate);
        CDX_LOGD("****nSampleRate:            %d", impl->aFormat_arry[i].nSampleRate);
        CDX_LOGD("****nBlockAlign:            %d", impl->aFormat_arry[i].nBlockAlign);
        CDX_LOGD("****nCodecSpecificDataLen:  %d", impl->aFormat_arry[i].nCodecSpecificDataLen);
        CDX_LOGD("************************************");
    }

    if(!(impl->vFormat.eCodecFormat == VIDEO_CODEC_FORMAT_RXG2 ||
       impl->vFormat.eCodecFormat == VIDEO_CODEC_FORMAT_RX))
    {
        pMediaInfo->program[0].video[0].bIs3DStream = 0;
        if(impl->vFormat.pCodecSpecificData)
        {
            pMediaInfo->program[0].video[0].nCodecSpecificDataLen =
                                                                impl->vFormat.nCodecSpecificDataLen;
            pMediaInfo->program[0].video[0].pCodecSpecificData = impl->vFormat.pCodecSpecificData;
        }
        else
        {
            pMediaInfo->program[0].video[0].nCodecSpecificDataLen = 0;
            pMediaInfo->program[0].video[0].pCodecSpecificData = 0;
        }
    }
    else
    {
        // for 3D
        if(generate_rv_init_info(&pMediaInfo->program[0].video[0], impl) != 0)
            return -1;
    }

    for(i=0; i<pMediaInfo->program[0].subtitleNum; i++)
    {
        // subtitle
        cdx_int32 stream_idx = matroska->SubTitleStream[i];
        pMediaInfo->program[0].subtitle[i].nStreamIndex = i;
        pMediaInfo->program[0].subtitle[i].nReferenceVideoWidth = impl->vFormat.nWidth;
        pMediaInfo->program[0].subtitle[i].nReferenceVideoHeight = impl->vFormat.nHeight;
        pMediaInfo->program[0].subtitle[i].nReferenceVideoFrameRate = impl->vFormat.nFrameRate;

        pMediaInfo->program[0].subtitle[i].eTextFormat = matroska->SubTitledata_encode_type[i];
        pMediaInfo->program[0].subtitle[i].eCodecFormat = matroska->SubTitleCodecType[i];
        memcpy(pMediaInfo->program[0].subtitle[i].strLang,
                matroska->language[i], MAX_SUBTITLE_LANG_SIZE);
        pMediaInfo->program[0].subtitle[i].nCodecSpecificDataLen =
                                                    matroska->streams[stream_idx]->extradata_size;
        pMediaInfo->program[0].subtitle[i].pCodecSpecificData =
                                                    matroska->streams[stream_idx]->extradata;

        CDX_LOGD("***********subtitle %d****************", i);
        CDX_LOGD("****streamindex:   %d", pMediaInfo->program[0].subtitle[i].nStreamIndex);
        CDX_LOGD("****width:         %d", pMediaInfo->program[0].subtitle[i].nReferenceVideoWidth);
        CDX_LOGD("****height:        %d", pMediaInfo->program[0].subtitle[i].nReferenceVideoHeight);
        CDX_LOGD("****TextFormat:    %d", pMediaInfo->program[0].subtitle[i].eTextFormat);
        CDX_LOGD("****eCodecFormat:  %x", pMediaInfo->program[0].subtitle[i].eCodecFormat);
        CDX_LOGD("****Language:      %s", pMediaInfo->program[0].subtitle[i].strLang);
        CDX_LOGD("****extradataSize: %d", matroska->streams[stream_idx]->extradata_size);
        CDX_LOGD("***************************************");
        if(matroska->SubTitleCodecType[i] == SUBTITLE_CODEC_DVDSUB)
        {
            cdx_uint8 *extradata = (cdx_uint8 *)matroska->streams[stream_idx]->extradata;
            cdx_int32 extradata_size = matroska->streams[stream_idx]->extradata_size;

            pMediaInfo->program[0].subtitle[i].sPaletteInfo.bValid = 1;
            pMediaInfo->program[0].subtitle[i].sPaletteInfo.nEntryCount = 16;
            while(extradata_size>=(7+16*8))
            {
                if (extradata[0] == 0x70 && extradata[1] == 0x61 &&
                    extradata[2] == 0x6C && extradata[3] == 0x65 &&
                    extradata[4] == 0x74 && extradata[5] == 0x74 &&
                    extradata[6] == 0x65 && extradata[7] == 0x3A)
                {
                    extradata +=9;
                    for(j=0; j<32; j++)
                    {
                        pMediaInfo->program[0].subtitle[i].sPaletteInfo.entry[j]
                        = (asc2byte(extradata[0], extradata[1]) << 16) +
                          (asc2byte(extradata[2], extradata[3]) << 8) +
                           asc2byte(extradata[4], extradata[5]);
                        if(extradata[6] != 0x2C)
                        {
                                break;
                        }
                        extradata += 8;
                    }
                    break;
                }
                extradata++;
                extradata_size--;
           }
        }
    }
    //CDX_BUF_DUMP(pMediaInfo->program[0].video[0].pCodecSpecificData,
    //pMediaInfo->program[0].video[0].nCodecSpecificDataLen);
    impl->mStatus = CDX_MKV_IDLE;
    return 0;

}

cdx_int32 __CdxMkvParserClrForceStop(CdxParserT *parser)
{
    int ret;
    struct CdxMkvParser* impl = (struct CdxMkvParser*)parser;
    if(!impl || !impl->priv_data)
    {
        return -1;
    }
    MatroskaDemuxContext *matroska = (MatroskaDemuxContext *)impl->priv_data;
    CdxStreamT* fp = matroska->fp;

    ret = CdxStreamClrForceStop(fp);

    impl->exitFlag = 0;
    impl->mStatus = CDX_MKV_IDLE;
    impl->mErrno = PSR_OK;
    return 0;

}

cdx_int32 __CdxMkvParserForceStop(CdxParserT *parser)
{
    int ret;
    struct CdxMkvParser* impl = (struct CdxMkvParser*)parser;
    if(!impl || !impl->priv_data)
    {
        return -1;
    }
    MatroskaDemuxContext *matroska = (MatroskaDemuxContext *)impl->priv_data;
    CdxStreamT* fp = matroska->fp;

    impl->exitFlag = 1;
    impl->mErrno = PSR_USER_CANCEL;
    if(fp)
    {
        ret = CdxStreamForceStop(fp);
    }
    while((impl->mStatus != CDX_MKV_IDLE) &&
          (impl->mStatus != CDX_MKV_PREFETCHED) &&
          (impl->mStatus != CDX_MKV_EOS))
    {
        usleep(2000);
    }
    impl->mStatus = CDX_MKV_IDLE;
    return 0;
}

static int __CdxMkvParserControl(CdxParserT *parser, int cmd, void *param)
{
    struct CdxMkvParser* impl = (struct CdxMkvParser*)parser;
    if(!impl || !impl->priv_data)
    {
        return -1;
    }
    //MatroskaDemuxContext *matroska = (MatroskaDemuxContext *)impl->priv_data;
    int idx;

    switch(cmd)
    {
    case CDX_PSR_CMD_SWITCH_AUDIO:
        break;

    case CDX_PSR_CMD_SWITCH_SUBTITLE:
        idx = *(int*)param;
        return MKVSetUserSubtitleStreamSel(impl, idx);

    case CDX_PSR_CMD_SET_FORCESTOP:
        return __CdxMkvParserForceStop(parser);
    case CDX_PSR_CMD_CLR_FORCESTOP:
        return __CdxMkvParserClrForceStop(parser);

    default:
        break;

    }

    return 0;
}

cdx_int32 __CdxMkvParserSeekTo(CdxParserT *parser, cdx_int64  timeUs, SeekModeType seekModeType)
{
    CDX_UNUSE(seekModeType);
    struct CdxMkvParser* impl = (struct CdxMkvParser*)parser;
    MatroskaDemuxContext *matroska = (MatroskaDemuxContext *)impl->priv_data;

    //if seekto the end of file, set status to eos, so we cannot prefetch anymore
    if(timeUs/1000 >= matroska->segment_duration)
    {
        CDX_LOGD("EOS, timeUs = %lld, duration = %lld", timeUs/1000, matroska->segment_duration);
        impl->mErrno = PSR_EOS;
        return 0;
    }

    impl->mStatus = CDX_MKV_SEEKING;
    int ret = CdxMatroskaSeek(impl, timeUs);
    if(ret == -1)
    {
        impl->mStatus = CDX_MKV_IDLE;
        impl->mErrno = PSR_UNKNOWN_ERR;
        return ret;
    }
    else if(ret == -2)
    {
        impl->mStatus = CDX_MKV_IDLE;
        impl->mErrno = PSR_USER_CANCEL;
        return ret;
    }
    else
    {
        impl->mStatus = CDX_MKV_IDLE;
        impl->mErrno = PSR_OK;
        return 0;
    }
}

cdx_uint32 __CdxMkvParserAttribute(CdxParserT *parser) /*return falgs define as open's falgs*/
{
    //struct CdxMkvParser* tmpMmsPsr = (struct CdxMkvParser*)parser;
    (void)parser;

    return -1;
}

cdx_int32 __CdxMkvParserGetStatus(CdxParserT *parser)
{
    struct CdxMkvParser* impl = (struct CdxMkvParser*)parser;

    return impl->mErrno;
}

static struct CdxParserOpsS mkvParserOps =
{
    .control         = __CdxMkvParserControl,
    .prefetch         = __CdxMkvParserPrefetch,
    .read             = __CdxMkvParserRead,
    .getMediaInfo     = __CdxMkvParserGetMediaInfo,
    .close             = __CdxMkvParserClose,
    .seekTo         = __CdxMkvParserSeekTo,
    .attribute        = __CdxMkvParserAttribute,
    .getStatus        = __CdxMkvParserGetStatus,
    .init           = __CdxMkvParserInit
};

static CdxParserT *__CdxMkvParserOpen(CdxStreamT *stream, cdx_uint32 flag)
{
    int ret;
    struct CdxMkvParser* impl = NULL;

    CDX_LOGD("------ __CdxMkvParserOpen");
    // create mkv reader
    impl = CdxMatroskaInit(&ret);
    if(!impl || (ret < 0))
    {
        CDX_LOGW("Create mkv file reader failed!");
        if(stream)
            CdxStreamClose(stream);
        goto open_error;
    }
    impl->stream = stream;
    impl->flag = flag;

    impl->mStatus = CDX_MKV_INITIALIZED;
    CdxAtomicSet(&impl->ref, 1);
    CDX_LOGD("mkv parser open stream = %p", stream);
    impl->parserinfo.ops = &mkvParserOps;
    impl->parserinfo.type = CDX_PARSER_MKV;

    //ret = pthread_create(&impl->openTid, NULL, MkvOpenThread, (void*)impl);
    //if(ret != 0)
    //{
    //    impl->openTid = (pthread_t)0;
    //}

    impl->SubStreamSyncFlg = 1;

    return &impl->parserinfo;

open_error:
    CDX_LOGW("open failed");
    CdxMatroskaClose(impl);
    CdxMatroskaExit(impl);
    return NULL;
}

static cdx_uint32 __CdxMkvParserProbe(CdxStreamProbeDataT *probeData)
{
CDX_LOGD("mkv probe");
    //static const char *const cdx_matroska_doctypes[] = { "matroska", "webm" };

    //uint64_t total = 0;
    //int len_mask = 0x80, size = 1, n = 1, i,datalen;

    // we only test the CDX_EBML_ID_HEADER whether in the begin of file
    // if you want to probe accurate, see ffmpeg
    if(GetBe32(probeData->buf) != CDX_EBML_ID_HEADER)
    {
        return 0;
    }
#if 0
    total = probeData->buf[4];
    while(size<=8 && !(total & len_mask))
    {
        size ++;
        len_mask >>= 1;
    }
    if(size > 8)
        return 0;

    total &= (len_mask - 1);
    while(n < size)
    {
        total = (total << 8) | probeData->buf[4 + n++];
    }

    /* Does the probe data contain the whole header? */
    if (probeData->len < 4 + size + total)
        return 0;

   /* The header should contain a known document type. For now,
     * we don't parse the whole header but simply check for the
     * availability of that array of characters inside the header.
     * Not fully fool-proof, but good enough. */
    for (i = 0; i < ARRAY_SIZE(cdx_matroska_doctypes); i++) {
        datalen = strlen(cdx_matroska_doctypes[i]);
        if (total < datalen)
            continue;
        for (n = size+4; n <= (size+total-datalen+4); n++)
        {
            if (!memcmp(p->buf+n, cdx_matroska_doctypes[i], datalen))
                return CDX_TRUE;
        }
    }
#endif
    CDX_LOGD(" mkv file ");
    return 100;
}

CdxParserCreatorT mkvParserCtor =
{
    .create = __CdxMkvParserOpen,
    .probe     = __CdxMkvParserProbe
};

