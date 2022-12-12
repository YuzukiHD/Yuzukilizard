/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxMovParser.c
 * Description : Mov parser.
 * History :
 *
 */

//#define CONFIG_LOG_LEVEL    OPTION_LOG_LEVEL_DETAIL
//#define LOG_TAG "CdxMovParser"

#include <assert.h>
#include "CdxMovParser.h"
#include "zlib.h"

#if BOARD_USE_PLAYREADY_NEW
extern int CreatePlayReadyCryptoPlugin();
extern int ClosePlayReadyCryptoPlugin();
extern int PlayReadyCryptoPluginCrypto(uint64_t iv,uint32_t ClearDataNum,uint32_t EncryptedDataNum,
    uint8_t **opaque_buf);
extern void* PlayReadyGetInputBuffer();

extern int HwSecureAllocCopy(void *dest, void *src, size_t n);

extern int HwSecureAllocRead(void *dest, void *src, size_t n);

static int PlayReadyOpenDrm(MOVContext *c)
{
    CDX_UNUSE(c);
    int ret = CreatePlayReadyCryptoPlugin();
    return ret;
}
static int PlayReadyCloseDrm(MOVContext *c)
{
    CDX_UNUSE(c);
    int ret = ClosePlayReadyCryptoPlugin();
    return ret;
}
static cdx_int32 PlayReadyRead(MOVContext *c, CdxStreamT *fp, CdxPacketT *pkt)
{
    int sample_index = c->chunk_info.index;
    uint8_t *opaque_buf = NULL;
    //native_handle_t *nh = native_handle_create(0 /*numFds*/, 1 /*numInts*/);
    //nh->data[0] = (int)(uintptr_t)opaque_buf;
    void *buf = PlayReadyGetInputBuffer();
    if (buf == NULL) {
        ALOGE("malloc failed.");
        return -1;
    }
    int size = CdxStreamRead(fp, buf, pkt->length);
    if (c->senc_data && sample_index >= 0) {
        if (c->senc_data[sample_index].region_count == 0) {
            c->senc_data[sample_index].region_count = 2;
            c->senc_data[sample_index].regions[0] = 0;
            c->senc_data[sample_index].regions[1] = pkt->length;
        }
        ALOGV("regions[0]=[%d] regions[1]=[%d]",
            c->senc_data[sample_index].regions[0],
            c->senc_data[sample_index].regions[1]);

        int result = PlayReadyCryptoPluginCrypto(c->senc_data[sample_index].iv,
            c->senc_data[sample_index].regions[0],
            c->senc_data[sample_index].regions[1],
            &opaque_buf);
        if(result < 0 || opaque_buf == NULL)
            return -1;

#if BOARD_PLAYREADY_USE_SECUREOS
        if(pkt->type == CDX_MEDIA_VIDEO) {
            if (pkt->buf)
                HwSecureAllocCopy(pkt->buf, opaque_buf, pkt->buflen);
            if (pkt->ringBuf)
                HwSecureAllocCopy(pkt->ringBuf, (cdx_int32 *)opaque_buf + pkt->buflen, pkt->length -
                pkt->buflen);
        } else {
            if (pkt->buf)
                HwSecureAllocRead(pkt->buf, opaque_buf, pkt->buflen);
            if (pkt->ringBuf)
                HwSecureAllocRead(pkt->ringBuf, (cdx_int32 *)opaque_buf + pkt->buflen, pkt->length -
                pkt->buflen);
        }
#else
        ALOGV("data[%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x]",
            opaque_buf[0],opaque_buf[1],opaque_buf[2],opaque_buf[3],
            opaque_buf[4],opaque_buf[5],opaque_buf[6],opaque_buf[7],
            opaque_buf[8],opaque_buf[9],opaque_buf[10],opaque_buf[11],
            opaque_buf[12],opaque_buf[13],opaque_buf[14],opaque_buf[15]);
        if (pkt->buf && (result >= pkt->buflen))
            memcpy(pkt->buf, opaque_buf, pkt->buflen);
        if (pkt->ringBuf && (result >= pkt->buflen))
            memcpy(pkt->ringBuf, (cdx_int32 *)opaque_buf + pkt->buflen, result - pkt->buflen);
#endif
    }
    return size;
}
#elif BOARD_USE_PLAYREADY_OLD
#include <drmcrt.h>
#include <drmcontextsizes.h>
#include <drmmanager.h>
#include <drmrevocation.h>
#include <drmcmdlnpars.h>
#include <drmtoolsutils.h>
#include <drmtoolsinit.h>
#include <drmconstants.h>

#define PLAYREADY_TMPFILE_DIR "/data/mediadrm/"

extern int HwSecureAllocCopy(void *dest, void *src, size_t n);
extern int HwSecureAllocRead(void *dest, void *src, size_t n);
extern void* HwSecureAllocGetPhysicAddressCpu(void *virt);

struct DrmInfoS {
    DRM_BOOL IsInit;
    DRM_APP_CONTEXT *AppContext;
    DRM_BYTE *OpaqueBuffer;
    DRM_BYTE *RevocationBuffer;
    DRM_DECRYPT_CONTEXT DecryptContext;
    DRM_BOOL IsDecryptInit;
} drminfo = { 0 };

static DRM_RESULT DRM_CALL BindCallback(
    __in     const DRM_VOID                 *f_pvPolicyCallbackData,
    __in           DRM_POLICY_CALLBACK_TYPE  f_dwCallbackType,
    __in_opt const DRM_KID                  *f_pKID,
    __in_opt const DRM_LID                  *f_pLID,
    __in     const DRM_VOID                 *f_pv )
{
    CDX_UNUSE(f_pKID);
    CDX_UNUSE(f_pLID);
    CDX_UNUSE(f_pv);
    return DRMTOOLS_PrintPolicyCallback( f_pvPolicyCallbackData, f_dwCallbackType );
}

static void DebugAnalyze(unsigned long dr, const char* file, unsigned long line, const char* expr)
{
    if (dr != 0)
        CDX_LOGD("%s:%ld 0x%x(%s)\n", strrchr(file, '/') + 1, line, (int)dr, expr);
}

static DRM_BOOL DRM_CALL LoadFile(const DRM_CONST_STRING * pPath, DRM_BYTE **ppBuffer,
    DRM_DWORD *pSize)
{
    DRM_RESULT dr = DRM_SUCCESS;
    OEM_FILEHDL fp = OEM_INVALID_HANDLE_VALUE;
    DRM_DWORD cbRead = 0;

    ChkBOOL( ( fp = Oem_File_Open( NULL,
                                     pPath->pwszString,
                                     OEM_GENERIC_READ,
                                     OEM_FILE_SHARE_READ,
                                     OEM_OPEN_EXISTING,
                                     OEM_ATTRIBUTE_NORMAL ) ) != OEM_INVALID_HANDLE_VALUE,
             DRM_E_FILEOPEN );

    ChkBOOL( Oem_File_SetFilePointer( fp,
                                      0,
                                      OEM_FILE_END,
                                      pSize ), DRM_E_FILE_SEEK_ERROR );

    ChkMem( *ppBuffer = ( DRM_BYTE * )Oem_MemAlloc( *pSize ) );

    ChkBOOL( Oem_File_SetFilePointer( fp,
                                      0,
                                      OEM_FILE_BEGIN,
                                      NULL ), DRM_E_FILE_SEEK_ERROR );

    ChkBOOL( Oem_File_Read( fp,
                            *ppBuffer,
                            *pSize,
                            &cbRead ), DRM_E_FILE_READ_ERROR );

    ChkBOOL( cbRead == *pSize, DRM_E_FILE_READ_ERROR );

ErrorExit:

    if ( fp != OEM_INVALID_HANDLE_VALUE )
    {
        Oem_File_Close( fp );
    }

    return dr;

}

static inline void stringToWString(DRM_WCHAR *dst, char *src)
{
    while ((*dst++ = *src++));
}

static DRM_RESULT PlayReadyOpenDrm(MOVContext *c)
{
    DRM_RESULT dr = DRM_SUCCESS;
    CDX_UNUSE(c);
    char prhds[] = PLAYREADY_TMPFILE_DIR "pr.hds";
    DRM_WCHAR wprhds[sizeof(prhds)] = {0};
    stringToWString(wprhds, prhds);
    const DRM_CONST_STRING pdstrDataStoreFile = DRM_CREATE_DRM_STRING(wprhds);
    const DRM_CONST_STRING *rights[1] = { &g_dstrWMDRM_RIGHT_PLAYBACK };

    DRM_BYTE *pbHeader = NULL;
    DRM_DWORD cbHeader = 0;
    DRM_BYTE *pbHeader2 = NULL;
    DRM_CONST_STRING dstrHeader = DRM_EMPTY_DRM_STRING;
    DRM_SUBSTRING dasstrHeader1 = DRM_EMPTY_DRM_SUBSTRING;

    char pin[] = PLAYREADY_TMPFILE_DIR "sstr.xml";
    DRM_WCHAR wpin[sizeof(pin)] = {0};
    stringToWString(wpin, pin);
    const DRM_CONST_STRING strin = DRM_CREATE_DRM_STRING(wpin);

    //SetDbgAnalyzeFunction(DebugAnalyze);

    if (drminfo.IsInit == 0) {

        memset(&drminfo, 0, sizeof(drminfo));

        drminfo.IsDecryptInit = FALSE;

        ChkDR( DRMTOOLS_DrmInitializeWithOpaqueBuffer(NULL,
                                                      &pdstrDataStoreFile,
                                                      TOOLS_DRM_BUFFER_SIZE,
                                                      &drminfo.OpaqueBuffer,
                                                      &drminfo.AppContext) );
        if (DRM_REVOCATION_IsRevocationSupported())
        {
            ChkMem( drminfo.RevocationBuffer = (DRM_BYTE *)Oem_MemAlloc(REVOCATION_BUFFER_SIZE) );
            ChkDR( Drm_Revocation_SetBuffer(drminfo.AppContext, drminfo.RevocationBuffer,
                REVOCATION_BUFFER_SIZE) );
        }

        ChkDR( LoadFile(&strin, &pbHeader, &cbHeader) );
        ChkMem( pbHeader2 = (DRM_BYTE *)Oem_MemAlloc( cbHeader * sizeof( DRM_WCHAR ) ) );
        DRM_DSTR_FROM_PB( &dstrHeader, pbHeader2, cbHeader * sizeof( DRM_WCHAR ) );
        dasstrHeader1.m_cch = cbHeader;

        DRM_UTL_PromoteASCIItoUNICODE( (const DRM_CHAR *)pbHeader,
                                      &dasstrHeader1,
                                      ( DRM_STRING * )&dstrHeader );

        ChkDR( Drm_Content_SetProperty( drminfo.AppContext,
                                        DRM_CSP_AUTODETECT_HEADER,
                                        DRM_PB_DSTR( &dstrHeader ),
                                        DRM_CB_DSTR( &dstrHeader ) ) );

        ChkDR( Drm_Reader_Bind(drminfo.AppContext,
                              rights,
                              DRM_NO_OF(rights),
                              (DRMPFNPOLICYCALLBACK)BindCallback,
                              NULL,
                              &drminfo.DecryptContext) );
        drminfo.IsDecryptInit = TRUE;

        ChkDR( Drm_Reader_Commit(drminfo.AppContext, NULL, NULL) );

        SAFE_OEM_FREE(pbHeader);
        SAFE_OEM_FREE(pbHeader2);
        drminfo.IsInit = 1;
    }
    CDX_LOGD("OpenDrm succeed.");
    return dr;
ErrorExit:
    CDX_LOGD("OpenDrm failed.");
    if (drminfo.IsDecryptInit)
    {
        Drm_Reader_Close(&drminfo.DecryptContext);
    }

    DRMTOOLS_DrmUninitializeWithOpaqueBuffer(&drminfo.OpaqueBuffer, &drminfo.AppContext);
    SAFE_OEM_FREE(drminfo.RevocationBuffer);
    SAFE_OEM_FREE(pbHeader);
    SAFE_OEM_FREE(pbHeader2);

    return dr;
}

static DRM_RESULT PlayReadyCloseDrm(MOVContext *c)
{
    DRM_RESULT dr = DRM_SUCCESS;
    CDX_UNUSE(c);
    if (drminfo.IsInit == 1) {
        if (drminfo.IsDecryptInit)
        {
            Drm_Reader_Close(&drminfo.DecryptContext);
        }

        DRMTOOLS_DrmUninitializeWithOpaqueBuffer(&drminfo.OpaqueBuffer, &drminfo.AppContext);
        SAFE_OEM_FREE(drminfo.RevocationBuffer);
        drminfo.IsInit = 0;
    }
    return dr;
}

static cdx_int32 PlayReadyRead(MOVContext *c, CdxStreamT *fp, CdxPacketT *pkt)
{
    int sample_index = c->chunk_info.index;
    void *buf = malloc(pkt->length);
    if (buf == NULL) {
        ALOGE("malloc failed.");
        return -1;
    }
    int size = CdxStreamRead(fp, buf, pkt->length);
    if (c->senc_data && sample_index >= 0) {
        if (c->senc_data[sample_index].region_count == 0) {
            c->senc_data[sample_index].region_count = 2;
            c->senc_data[sample_index].regions[0] = 0;
            c->senc_data[sample_index].regions[1] = pkt->length;
        }
        unsigned char *opaque_buf;
        unsigned int opaque_size;
        if (Drm_Reader_DecryptOpaque(&drminfo.DecryptContext,
                        c->senc_data[sample_index].region_count,
                        c->senc_data[sample_index].regions,
                        c->senc_data[sample_index].iv,
                        pkt->length,
                        buf,
                        &opaque_size,
                        &opaque_buf) != DRM_SUCCESS) {
            CDX_LOGE("decrypt fail.");
            free(buf);
            DRM_Reader_FreeOpaqueDecryptedContent(&drminfo.DecryptContext, opaque_size, opaque_buf);
            return -1;
        }
        //CDX_LOGV("pkt->buf=%p(%p), pkt->length=%d, opaque_buf=%p(%p), opaque_size=%d,
        //    pkt->type=%d", pkt->buf, SecureMemAdapterGetPhysicAddressCpu(pkt->buf), pkt->length,
        //    opaque_buf, SecureMemAdapterGetPhysicAddressCpu(opaque_buf), opaque_size, pkt->type);
#if BOARD_PLAYREADY_USE_SECUREOS
        if(pkt->type == CDX_MEDIA_VIDEO) {
            if (pkt->buf)
                HwSecureAllocCopy(pkt->buf, opaque_buf, pkt->buflen);
            if (pkt->ringBuf)
                HwSecureAllocCopy(pkt->ringBuf, opaque_buf + pkt->buflen, pkt->length -
                pkt->buflen);
        } else {
            if (pkt->buf)
                HwSecureAllocRead(pkt->buf, opaque_buf, pkt->buflen);
            if (pkt->ringBuf)
                HwSecureAllocRead(pkt->ringBuf, opaque_buf + pkt->buflen, pkt->length -
                pkt->buflen);
        }
#else
        if (pkt->buf)
            memcpy(pkt->buf, opaque_buf, pkt->buflen);
        if (pkt->ringBuf)
            memcpy(pkt->ringBuf, opaque_buf + pkt->buflen, pkt->length - pkt->buflen);
#endif
        free(buf);
        DRM_Reader_FreeOpaqueDecryptedContent(&drminfo.DecryptContext, opaque_size, opaque_buf);
    }
    return size;
}

#else
static int PlayReadyOpenDrm(MOVContext *c)
{
    CDX_UNUSE(c);
    return 0;
}
static int PlayReadyCloseDrm(MOVContext *c)
{
    CDX_UNUSE(c);
    return 0;
}
static cdx_int32 PlayReadyRead(MOVContext *c, CdxStreamT *fp, CdxPacketT *pkt)
{
    CDX_UNUSE(c);
    CDX_UNUSE(fp);
    CDX_UNUSE(pkt);
    return 0;
}
#endif

enum CdxMovStatusE
{
    CDX_MOV_INITIALIZED, /*control , getMediaInfo, not prefetch or read, seekTo,.*/
    CDX_MOV_IDLE,
    CDX_MOV_PREFETCHING,
    CDX_MOV_PREFETCHED,
    CDX_MOV_READING,
    CDX_MOV_SEEKING,
    CDX_MOV_EOS,
};

extern CDX_U32 ReadStss(MOVContext *c, MOVStreamContext *st, cdx_uint32 idx);

static CDX_U32 GetLe16(char *s)
{
    CDX_U32 val;
    val = (CDX_U32)(*s);
    s += 1;
    val |= (CDX_U32)(*s) << 8;
    return val;
}

static CDX_U32 GetLe32(char *s)
{
    CDX_U32 val;
    val = GetLe16(s);
    s += 2;
    val |= GetLe16(s) << 16;
    return val;
}

static int MovGetCacheState(struct CdxMovParser *impl, struct ParserCacheStateS *cacheState)
{
    struct StreamCacheStateS streamCS;
    if (CdxStreamControl(impl->stream, STREAM_CMD_GET_CACHESTATE, &streamCS) < 0)
    {
        CDX_LOGE("STREAM_CMD_GET_CACHESTATE fail");
        return -1;
    }

    cacheState->nCacheCapacity = streamCS.nCacheCapacity;
    cacheState->nCacheSize = streamCS.nCacheSize;
    cacheState->nBandwidthKbps = streamCS.nBandwidthKbps;
    cacheState->nPercentage = streamCS.nPercentage;
    return 0;
}

// check the sample is key frame
static int checkKeyFrame(struct CdxMovParser *impl, MOVStreamContext *st)
{
    MOVContext               *c;
    //int                       ret;
    int stss_sample_idx;
    int cur_sample_idx = st->mov_idx_curr.sample_idx;

    c = (MOVContext*)impl->privData;

    if(st->mov_idx_curr.stss_idx > (int)st->stss_size)
    {
        return 0;
    }

    stss_sample_idx = ReadStss(c, st, st->mov_idx_curr.stss_idx<<2);
    if(stss_sample_idx == cur_sample_idx)
    {
        st->mov_idx_curr.stss_idx ++;
        return 1;
    }

    while(stss_sample_idx < cur_sample_idx)
    {

        if(st->mov_idx_curr.stss_idx > (int)st->stss_size)
        {
            break;
        }

        //CDX_LOGD("--- st->mov_idx_curr.stss_idx = %d", st->mov_idx_curr.stss_idx);
        stss_sample_idx = ReadStss(c, st, st->mov_idx_curr.stss_idx<<2);
        st->mov_idx_curr.stss_idx ++;

        if(stss_sample_idx == cur_sample_idx)
        {
            return 1;
        }
    }

    return 0;
}

static cdx_int32 __CdxMovParserClose(CdxParserT *parser)
{
    struct CdxMovParser          *tmpMovPsr;
    MOVContext                   *c;
    int ret;

    tmpMovPsr = (struct CdxMovParser *)parser;
    if(!tmpMovPsr)
    {
        CDX_LOGE("Close mov file parser module error, there is no file information!");
        return -1;
    }

    c = (MOVContext *)tmpMovPsr->privData;

    tmpMovPsr->exitFlag = 1;

    CdxStreamForceStop(tmpMovPsr->stream);
    CdxAtomicDec(&tmpMovPsr->ref);
    while ((ret = CdxAtomicRead(&tmpMovPsr->ref)) != 0)
    {
        CDX_LOGV("wait for ref = %d", ret);
        usleep(10000);
    }

    #if SAVE_VIDEO
    fclose(tmpMovPsr->fp);
    #endif

    if (c->bPlayreadySegment)
    {
        PlayReadyCloseDrm(c);
    }
    EraseId3(&c->id3v2);
    CdxMovClose(tmpMovPsr);
    CdxMovExit(tmpMovPsr);
    tmpMovPsr = NULL;

    return 0;
}

static cdx_int32 __CdxMovParserPrefetch(CdxParserT *parser, CdxPacketT * pkt)
{
    struct CdxMovParser          *tmpMovPsr;
    MOVContext                    *c;
    int                        result;
    MOVStreamContext       *st = NULL;

    tmpMovPsr = (struct CdxMovParser *)parser;
    c = (MOVContext*)tmpMovPsr->privData;
    CdxStreamT      *fp = c->fp;
    if(tmpMovPsr->mErrno == PSR_EOS)
    {
        CDX_LOGW("---EOS");
        return -1;
    }

    if((tmpMovPsr->mStatus != CDX_MOV_IDLE) && (tmpMovPsr->mStatus != CDX_MOV_PREFETCHED))
    {
        CDX_LOGW("the status of prefetch is error");
        tmpMovPsr->mErrno = PSR_INVALID_OPERATION;
        return -1;
    }

    if(tmpMovPsr->exitFlag)
    {
        tmpMovPsr->mStatus = CDX_MOV_IDLE;
        return -1;
    }

    // if we call prefetch and do not read, we get the same packet
    if(tmpMovPsr->mStatus == CDX_MOV_PREFETCHED)
    {
        CDX_LOGW("----- prefetch in prefetching, care******");
        pkt->length   = tmpMovPsr->packet.length;
        pkt->type     = tmpMovPsr->packet.type;
        pkt->pts      = tmpMovPsr->packet.pts;
        pkt->duration = tmpMovPsr->packet.duration;
        pkt->flags    = tmpMovPsr->packet.flags;
        tmpMovPsr->mStatus = CDX_MOV_PREFETCHED;
        return 0;
    }
    tmpMovPsr->mStatus = CDX_MOV_PREFETCHING;
    memset(pkt, 0x00, sizeof(CdxPacketT));

    if(c->chunk_info.no_completed_read && c->chunk_info.type == 2)
    {
        pkt->length   = tmpMovPsr->packet.length;
        pkt->type     = tmpMovPsr->packet.type;
        pkt->pts      = tmpMovPsr->packet.pts;
        pkt->duration = tmpMovPsr->packet.duration;
        pkt->flags    = tmpMovPsr->packet.flags;
        tmpMovPsr->mStatus = CDX_MOV_PREFETCHED;
        c->chunk_info.cycle_num++;
        if(c->chunk_info.cycle_num >= c->chunk_info.cycle_to)
        {
             c->chunk_info.no_completed_read = 0;
             c->chunk_info.cycle_num = 0;
        }
        c->chunk_info.offset += pkt->length;
        return 0;
    }

    result = CdxMovRead(tmpMovPsr);
    if(result == 1)
    {
        tmpMovPsr->mErrno = PSR_EOS;
        CDX_LOGW("Try to read sample failed! end of stream");
        tmpMovPsr->mStatus = CDX_MOV_IDLE;
        return -1;
    }
    else if (result == -1)
    {
        //tmpMovPsr->mErrno = PSR_IO_ERR;
        CDX_LOGW("--- parser IO_ERR, maybe forcestop");
        tmpMovPsr->mStatus = CDX_MOV_IDLE;
        return -1;
    }
    st = c->streams[c->prefetch_stream_index];

    pkt->length = c->chunk_info.length;

    if(c->chunk_info.no_completed_read && c->chunk_info.type == 2)
    {
        pkt->length = c->chunk_info.audio_segsz;
        c->chunk_info.cycle_num++;
        if(c->chunk_info.cycle_num >= c->chunk_info.cycle_to)
        {
             c->chunk_info.no_completed_read = 0;
             c->chunk_info.cycle_num = 0;
        }
    }

    if(1 == st->stsd_type)
    {
        // the unit of pkt pts is us
        pkt->type = CDX_MEDIA_VIDEO;
        pkt->pts = (CDX_S64)st->mov_idx_curr.current_dts * 1000 * 1000
                                / st->time_scale + st->basetime*1000*1000;

        if(!tmpMovPsr->bSmsSegment)
        {
            if(checkKeyFrame(tmpMovPsr, st) == 1)
            {
                pkt->flags |= KEY_FRAME;
                //CDX_LOGD("--- smaple = %x, falg = %d", st->mov_idx_curr.sample_idx, pkt->flags );
            }
        }

    }
    else if (2 == st->stsd_type)
    {
        pkt->streamIndex = st->stream_index;
        pkt->type = CDX_MEDIA_AUDIO;
        pkt->pts = (CDX_S64)st->mov_idx_curr.current_dts * 1000*1000
                            / st->time_scale + st->basetime*1000*1000;
    }
    else if(3 == st->stsd_type)
    {
        pkt->streamIndex = st->stream_index;
        pkt->type = CDX_MEDIA_SUBTITLE;
        pkt->pts = (CDX_S64)st->mov_idx_curr.current_dts * 1000*1000
                            / st->time_scale + st->basetime*1000*1000;
        pkt->duration = (CDX_S64)st->mov_idx_curr.current_duration * 1000*1000
                            / st->time_scale + st->basetime*1000*1000;

        // it is the 0x0000, skip it
        if(st->eCodecFormat == SUBTITLE_CODEC_TIMEDTEXT)
        {
            if(pkt->length == 2)
            {
                tmpMovPsr->mStatus = CDX_MOV_IDLE;
                return __CdxMovParserPrefetch(parser, pkt);
            }

            char buf[2] = {0};
            result = CdxStreamSeek(fp, c->chunk_info.offset, SEEK_SET);
            if(result < 0)
            {
                CDX_LOGW(" seek failed");
                tmpMovPsr->mStatus = CDX_MOV_IDLE;
                return -1;
            }

            result = CdxStreamRead(fp, buf, 2);
            if(result < 0)
            {
                tmpMovPsr->mStatus = CDX_MOV_IDLE;
                tmpMovPsr->mErrno = PSR_IO_ERR;
                CDX_LOGW("---  read error");
                return -1;
            }
            int length = buf[0] << 8 | buf[1];
            if(length < pkt->length-2)
            {
                pkt->length = length;
            }
            c->chunk_info.offset += 2;
        }
    }

    pkt->flags |= (FIRST_PART|LAST_PART);
    tmpMovPsr->packet.length    = pkt->length;
    tmpMovPsr->packet.type      = pkt->type;
    tmpMovPsr->packet.pts       = pkt->pts;
    tmpMovPsr->packet.duration  = pkt->duration;
    tmpMovPsr->packet.flags     = pkt->flags;
    CDX_LOGV("type = %d, streamindex = %d, offset = %llx, pts = %lld, length = %d, "
     " duration = %lld, st->stsd_type=%d",
     pkt->type, pkt->streamIndex, c->chunk_info.offset, pkt->pts, pkt->length,
     pkt->duration, st->stsd_type);
    tmpMovPsr->mStatus = CDX_MOV_PREFETCHED;
    return 0;
}

static cdx_int32 __CdxMovParserRead(CdxParserT *parser, CdxPacketT *pkt)
{
    CdxStreamT      *fp;
    struct CdxMovParser          *tmpMovPsr;
    MOVContext                  *c;
    int ret;
    MOVStreamContext       *st = NULL;

    if(!parser)
        return -1;

    tmpMovPsr = (struct CdxMovParser *)parser;
    c = (MOVContext*)tmpMovPsr->privData;

    if(tmpMovPsr->mStatus != CDX_MOV_PREFETCHED)
    {
        tmpMovPsr->mErrno = PSR_INVALID_OPERATION;
        CDX_LOGE("tmpMovPsr->mStatus != CDX_MOV_PREFETCHED");
        return -1;
    }

    tmpMovPsr->mStatus = CDX_MOV_READING;

    st = c->streams[c->prefetch_stream_index];

    fp = c->fp;

    if(c->bSeekAble)
    {
        //CDX_LOGD("--- mov read, seek(offset = %lld), whence = %d",
        //c->chunk_info.offset, SEEK_SET);
        ret = CdxStreamSeek(fp, c->chunk_info.offset, SEEK_SET);
        if(ret < 0)
        {
            CDX_LOGW(" seek failed");
            tmpMovPsr->mStatus = CDX_MOV_IDLE;
            return -1;
        }
    }
    else
    {
        cdx_int64 diff = c->chunk_info.offset - CdxStreamTell(fp);
        if(diff < 0)
        {
            tmpMovPsr->mStatus = CDX_MOV_IDLE;
            CDX_LOGE("diff(%lld)", diff);
            return -1;
        }
        else if(diff < 40960)
        {
            ret = CdxStreamSkip(fp, diff);
            if(ret < 0)
            {
                CDX_LOGW("skip error");
                tmpMovPsr->mStatus = CDX_MOV_IDLE;
                return -1;
            }
        }
        else
        {
            unsigned char* buf = malloc(diff);
            ret = CdxStreamRead(fp, buf, diff);
            free(buf);
            if(ret < 0)
            {
                CDX_LOGW("read error , diff = %lld", diff);
                tmpMovPsr->mStatus = CDX_MOV_IDLE;
                tmpMovPsr->mErrno = PSR_IO_ERR;
                return -1;
            }
        }
    }

    int size = 0;
    if(pkt->length <= pkt->buflen)
    {
        if (c->senc_data)
        {
            size = PlayReadyRead(c, fp, pkt);
        }
        else
        {
            size = CdxStreamRead(fp, pkt->buf, pkt->length);
            if(size < 0)
            {
                tmpMovPsr->mStatus = CDX_MOV_IDLE;
                tmpMovPsr->mErrno = PSR_IO_ERR;
                CDX_LOGE("read failed.");
                return -1;
            }
        }
        if(size < pkt->length)
        {
            CDX_LOGW("read error");
            tmpMovPsr->mStatus = CDX_MOV_IDLE;
            tmpMovPsr->mErrno = PSR_IO_ERR;
            return -1;
        }

        #if SAVE_VIDEO
        if(pkt->type == CDX_MEDIA_VIDEO)
        {
            char buf[4096];
            fprintf(tmpMovPsr->fp, "\n");
            sprintf(buf, "%s%llx", "offset:", c->chunk_info.offset);
            fprintf(tmpMovPsr->fp, "%s\n", buf);
            sprintf(buf, "%s%llx", "tell:", CdxStreamTell(fp));
            fprintf(tmpMovPsr->fp, "%s\n", buf);
            sprintf(buf, "%s%lld", "pts:", pkt->pts);
            fprintf(tmpMovPsr->fp, "%s\n", buf);
            sprintf(buf, "%s%d", "length:", pkt->length);
            fprintf(tmpMovPsr->fp, "%s\n", buf);
            fwrite(pkt->buf, 1, pkt->length, tmpMovPsr->fp);
        }
        #endif
    }
    else
    {
        if (c->senc_data)
        {
            size = PlayReadyRead(c, fp, pkt);
        }
        else
        {
            size = CdxStreamRead(fp, pkt->buf, pkt->buflen);
            if(size < 0)
            {
                tmpMovPsr->mStatus = CDX_MOV_IDLE;
                tmpMovPsr->mErrno = PSR_IO_ERR;
                CDX_LOGE("read failed.");
                return -1;
            }
            //CDX_LOGD(" pkt->length=%d ,  pkt->buflen=%d",  pkt->length,  pkt->buflen);
            ret = CdxStreamRead(fp, pkt->ringBuf, pkt->length - pkt->buflen);
            if(ret < 0)
            {
                tmpMovPsr->mStatus = CDX_MOV_IDLE;
                tmpMovPsr->mErrno = PSR_IO_ERR;
                CDX_LOGE("read failed.");
                return -1;
            }
            size += ret;
        }

        if(size < pkt->length)
        {
            CDX_LOGW("read size<%d> less than length<%d>", size, pkt->length);
            tmpMovPsr->mStatus = CDX_MOV_IDLE;
            return -1;
        }

        #if SAVE_VIDEO
        if(pkt->type == CDX_MEDIA_VIDEO)
        {
            char buf[4096];
            fprintf(tmpMovPsr->fp, "\n");
            sprintf(buf, "%s%llx", "offset:", c->chunk_info.offset);
            fprintf(tmpMovPsr->fp, "%s\n", buf);
            sprintf(buf, "%s%llx", "tell:", CdxStreamTell(fp));
            fprintf(tmpMovPsr->fp, "%s\n", buf);
            sprintf(buf, "%s%lld", "pts:", pkt->pts);
            fprintf(tmpMovPsr->fp, "%s\n", buf);
            sprintf(buf, "%s%d", "length:", pkt->length);
            fprintf(tmpMovPsr->fp, "%s\n", buf);
            fwrite(pkt->buf, 1, pkt->buflen, tmpMovPsr->fp);
            fwrite(pkt->ringBuf, 1, pkt->length - pkt->buflen, tmpMovPsr->fp);
        }
        #endif
    }

   tmpMovPsr->mStatus = CDX_MOV_IDLE;
    return 0;
}

static cdx_int32 __CdxMovParserGetMediaInfo(CdxParserT *parser, CdxMediaInfoT * pMediaInfo)
{
    CDX_U8 i;
    struct CdxMovParser* tmpMovPsr ;
    MOVContext                    *c;
    //CDX_S32                     nSubStrmNum = 0;
    CDX_U32              video_max_bitrate;
    CDX_U32              video_avg_bitrate = 0;

    CDX_CHECK(parser);
    tmpMovPsr = (struct CdxMovParser*)parser;
    if(!tmpMovPsr)
    {
        CDX_LOGE("mov file parser lib has not been initiated!");
        return -1;
    }
    while(tmpMovPsr->mErrno != PSR_OK)
    {
        usleep(100);
        if(tmpMovPsr->exitFlag)
        {
            break;
        }
    }
    memset(pMediaInfo, 0, sizeof(CdxMediaInfoT));

    c = (MOVContext*)tmpMovPsr->privData;

    pMediaInfo->fileSize = CdxStreamSize(c->fp);
    pMediaInfo->programNum = 1;
    pMediaInfo->programIndex = 0;

    //* copy the location info
    memcpy(pMediaInfo->location,c->location,32*sizeof(cdx_uint8));
    CDX_LOGV("Get mediainfo");
    if(c->id3v2 && c->id3v2->mIsValid)
    {
        CDX_LOGV("Mov id3v2 has vaild parsed...");
        pMediaInfo->id3v2HadParsed = 1;
        Id3BaseGetMetaData(pMediaInfo, c->id3v2);
        /*
              *  It seems that 'date' metadata is not included in id3,  based on stagefright.
              *  To pass the cts test, those infomations not extracted from id3 parser,
              *  should be send to mediainfo departly.
              */
        SetMetaData(pMediaInfo, DATE, (const char*)c->date, ENCODE_DEFAULT);
    }

    VideoStreamInfo* video = NULL;
    AudioStreamInfo* audio = NULL;
    SubtitleStreamInfo* subtitle = NULL;
    //****< classical the type of stream: audio, video, subtitle
    // NOTE:   we only support one video, audio, subtitle stream .
    // we should modify the code of stsd and hdlr, if we want to support the multi video stream
    for(i=0; i<c->nb_streams; i++)
    {
        MOVStreamContext *st = c->streams[i];
        if(st->codec.codecType == CODEC_TYPE_VIDEO && pMediaInfo->program[0].videoNum < VIDEO_STREAM_LIMIT && !st->unsurpoort)
        {
            video = &pMediaInfo->program[0].video[pMediaInfo->program[0].videoNum];
            video->eCodecFormat = st->eCodecFormat;
            CDX_LOGV("--- codecformat = %x", video->eCodecFormat);
            video->nWidth = st->codec.width;
            video->nHeight = st->codec.height;
            pMediaInfo->androidCaptureFps = 0;
            if(c->android_capture_fps)
            {
                pMediaInfo->androidCaptureFps = c->android_capture_fps;
                video->nFrameRate = pMediaInfo->androidCaptureFps;
            }
            else
            {
                if(st->sample_duration)
                {
                    video->nFrameRate = (int64_t)(st->time_scale)*1000/ (int64_t)st->sample_duration;
                    CDX_LOGV("---- frame rate = %d, st->time_scale: %d, st->sample_duration: %d",
                        video->nFrameRate, st->time_scale, st->sample_duration);
                }
                else
                {
                    CDX_LOGW("sample duration is 0, cannot get the framerate");
                    video->nFrameRate = 0;
                }
            }

            CDX_LOGV("width = %d, height = %d", video->nWidth, video->nHeight);
            if(st->codec.extradataSize)
            {
                CDX_LOGV("extradataSize = %d", st->codec.extradataSize);
                video->nCodecSpecificDataLen = st->codec.extradataSize;
                video->pCodecSpecificData = st->codec.extradata;
            }
            c->v2st[pMediaInfo->program[0].videoNum] = i;
            pMediaInfo->program[0].videoNum ++;
            if(pMediaInfo->program[0].videoNum > MAX_STREAM_NUM-1)
            {
                CDX_LOGW("video stream number<%d> > MAX_STREAM_NUM",
                    pMediaInfo->program[0].videoNum);
            }
            video_max_bitrate = st->codec.max_bitrate;
            video_avg_bitrate = st->codec.avg_bitrate;

            memcpy(pMediaInfo->rotate,st->rotate,4*sizeof(cdx_uint8));
        }
        else if(st->codec.codecType == CODEC_TYPE_AUDIO && !st->unsurpoort)
        {
            audio = &pMediaInfo->program[0].audio[pMediaInfo->program[0].audioNum];
            audio->eCodecFormat    = st->eCodecFormat;
            audio->eSubCodecFormat = st->eSubCodecFormat;
            audio->nChannelNum     = st->codec.channels;
            audio->nBitsPerSample  = st->codec.bitsPerSample;
            audio->nSampleRate     = st->codec.sampleRate;
            audio->nAvgBitrate     = st->codec.avg_bitrate;
            audio->nMaxBitRate     = st->codec.max_bitrate;
            memcpy(audio->strLang, st->language, 32);

            #ifdef PRINT_MEDIAINFO
            CDX_LOGD("********* audio %d************", pMediaInfo->program[0].audioNum);
            CDX_LOGD("****eCodecFormat:    %d", audio->eCodecFormat);
            CDX_LOGD("****eSubCodecFormat: %d", audio->eSubCodecFormat);
            CDX_LOGD("****nChannelNum:     %d", audio->nChannelNum);
            CDX_LOGD("****nBitsPerSample:  %d", audio->nBitsPerSample);
            CDX_LOGD("****nSampleRate:     %d", audio->nSampleRate);
            CDX_LOGD("****nAvgBitrate:     %d", audio->nAvgBitrate);
            CDX_LOGD("****nMaxBitRate:     %d", audio->nMaxBitRate);
            CDX_LOGD("****extradataSize    %d", st->codec.extradataSize);
            CDX_LOGD("***************************");
            #endif
            // set private data for audio decoder
            if(st->codec.extradataSize)
            {
                audio->pCodecSpecificData = st->codec.extradata;
                audio->nCodecSpecificDataLen = st->codec.extradataSize;
            }
            c->a2st[pMediaInfo->program[0].audioNum] = i;
            pMediaInfo->program[0].audioNum ++;
            if(pMediaInfo->program[0].audioNum > MAX_STREAM_NUM-1)
            {
                CDX_LOGW("audio stream number<%d> > MAX_STREAM_NUM",
                    pMediaInfo->program[0].videoNum);
            }
        }
        else if(st->codec.codecType == CODEC_TYPE_SUBTITLE)
        {
            subtitle = &pMediaInfo->program[0].subtitle[pMediaInfo->program[0].subtitleNum];
            subtitle->eCodecFormat = st->eCodecFormat;
            subtitle->eTextFormat = st->eSubCodecFormat;
            //subtitle->nReferenceVideoHeight = video->nHeight;
            //subtitle->nReferenceVideoWidth  = video->nWidth;
            memcpy(subtitle->strLang, st->language, 32);
            if(st->codec.extradataSize)
            {
                subtitle->pCodecSpecificData = st->codec.extradata;
                subtitle->nCodecSpecificDataLen = st->codec.extradataSize;
            }
            #ifdef PRINT_MEDIAINFO
            CDX_LOGD("***************subtitle %d**********", pMediaInfo->program[0].subtitleNum);
            CDX_LOGD("****eCodecFormat:    %d", subtitle->eCodecFormat);
            CDX_LOGD("****eSubCodecFormat: %d", subtitle->eTextFormat);
            CDX_LOGD("****strLang:         %s", subtitle->strLang);
            CDX_LOGD("****extradatasize:   %d", st->codec.extradataSize);
            CDX_LOGD("************************************");
            #endif
            c->s2st[pMediaInfo->program[0].subtitleNum] = i;
            st->SubStreamSyncFlg = 1;
            pMediaInfo->program[0].subtitleNum++;
        }
    }

    CDX_LOGV("streamNum = %d, videoNum = %d, audioNum = %d, subtitleNum = %d",
        c->nb_streams, pMediaInfo->program[0].videoNum, pMediaInfo->program[0].audioNum,
        pMediaInfo->program[0].subtitleNum);

    //**< set the default stream index
    pMediaInfo->program[0].audioIndex    = 0;
    pMediaInfo->program[0].videoIndex    = c->select_video_stream_idx;
    pMediaInfo->program[0].subtitleIndex = 0;
    c->video_stream_idx     = c->v2st[pMediaInfo->program[0].videoIndex];
    c->audio_stream_idx     = c->a2st[pMediaInfo->program[0].audioIndex];
    c->subtitle_stream_idx  = c->s2st[pMediaInfo->program[0].subtitleIndex];

    CdxMovSetStream(tmpMovPsr);

    pMediaInfo->program[0].duration = tmpMovPsr->totalTime;
    CDX_LOGV("-- mov duration = %d", tmpMovPsr->totalTime);
    if(tmpMovPsr->hasVideo)
    {
        if(video_avg_bitrate == 0 && tmpMovPsr->totalTime)
        {
            video_avg_bitrate = pMediaInfo->fileSize * 8 / tmpMovPsr->totalTime * 1000;
            CDX_LOGV("****video_avg_bitrate:  %d",video_avg_bitrate);
        }
    }

    pMediaInfo->bitrate = video_avg_bitrate;

    // set date
    memcpy(pMediaInfo->date,(const char*)c->date,strlen((const char*)c->date));
    pMediaInfo->datesz = strlen((const char*)c->date);

    if(!c->bSeekAble || (tmpMovPsr->hasVideo && (!tmpMovPsr->hasIdx &&
        pMediaInfo->program[0].videoNum && video->eCodecFormat != VIDEO_CODEC_FORMAT_MJPEG)))
    {
        CDX_LOGD("can not seek. bSeekable [%s], hasVideo [%s], hasIdx [%s].",
            c->bSeekAble?"Yes":"No", tmpMovPsr->hasVideo?"Yes":"No", tmpMovPsr->hasIdx?"Yes":"No");
        pMediaInfo->bSeekable = 0;
    }
    else
    {
        pMediaInfo->bSeekable = 1;
        if(tmpMovPsr->hasVideo && pMediaInfo->program[0].videoNum &&
                video->eCodecFormat == VIDEO_CODEC_FORMAT_MJPEG)
            c->bSeekWithoutKeyframe = 1;
    }

    MOVStreamContext *st = NULL;
    for(i=0; i<c->nb_streams; i++)
    {
        st = c->streams[i];
        CDX_LOGV("--i = %d, stsd_type = %d, stream_index = %d, nb_streams = %d", i, st->stsd_type,
        st->stream_index ,c->nb_streams);
    }

    // tmpMovPsr->mStatus = CDX_MOV_IDLE;  // bug fixing: this function will be called to get the file duration when do seek operation,
                                            // and this statement will change the value(CDX_MOV_PREFETCHED) of tmpMovPsr->mStatus setted by 
                                            // __CdxMovParserPrefetch()function to CDX_MOV_IDLE, and this changement will cause the failure of __CdxMovParserRead() function,
                                            // the failure of __CdxMovParserRead() function will cause the blocking of ui .
    return 0;
}

static cdx_int32 __CdxMovParserForceStop(CdxParserT *parser)
{
    struct CdxMovParser* tmpMovPsr;
    MOVContext                    *c;
    int ret;

    tmpMovPsr = (struct CdxMovParser*)parser;
    if(!tmpMovPsr || !tmpMovPsr->privData)
    {
        CDX_LOGE("mov file parser lib has not been initiated!");
        return -1;
    }
    c = (MOVContext*)tmpMovPsr->privData;

    CdxStreamT* fp = c->fp;
    tmpMovPsr->exitFlag = 1;

    if(fp)
    {
        ret = CdxStreamForceStop(fp);
    }

    while((tmpMovPsr->mStatus != CDX_MOV_IDLE) && (tmpMovPsr->mStatus != CDX_MOV_PREFETCHED))
    {
        usleep(2000);
    }

    tmpMovPsr->mStatus = CDX_MOV_IDLE;
    tmpMovPsr->mErrno = PSR_USER_CANCEL;
    CDX_LOGV("-- mov ForceStop end");

    return 0;
}

static cdx_int32 __CdxMovParserClrForceStop(CdxParserT *parser)
{
    struct CdxMovParser* tmpMovPsr;
    MOVContext                    *c;
    int ret;

    tmpMovPsr = (struct CdxMovParser*)parser;
    if(!tmpMovPsr || !tmpMovPsr->privData)
    {
        CDX_LOGE("mov file parser lib has not been initiated!");
        return -1;
    }
    c = (MOVContext*)tmpMovPsr->privData;

    if(tmpMovPsr->mStatus < CDX_MOV_IDLE)
    {
        CDX_LOGW("mov->status < CDX_PSR_IDLE, can not forceStop");
        tmpMovPsr->mErrno = PSR_INVALID_OPERATION;
        return -1;
    }

    CdxStreamT* fp = c->fp;
    tmpMovPsr->exitFlag = 0;

    if(fp)
    {
        ret = CdxStreamClrForceStop(fp);
    }

    CDX_LOGD("-- mov clear ForceStop end");
    return 0;
}

static int __CdxMovParserControl(CdxParserT *parser, int cmd, void *param)
{
    struct CdxMovParser *impl;

    CDX_CHECK(parser);
    impl = (struct CdxMovParser*)parser;
    MOVContext      *c = (MOVContext*)impl->privData;
    //VirCacheContext *vc = impl->vc;
    CdxStreamT* stream = (CdxStreamT*)param;

    switch(cmd)
    {
        case CDX_PSR_CMD_SET_VIDEO_INDEX:
        {
            c->select_video_stream_idx = *(cdx_int32 *)param;
            c->video_stream_idx = c->v2st[c->select_video_stream_idx];
            break;
        }
        case CDX_PSR_CMD_SWITCH_AUDIO:
        {
            CDX_LOGD("-- switch audio ");
            break;
        }
        case CDX_PSR_CMD_SWITCH_SUBTITLE:
        {
            CDX_LOGI("--- switch Subtitle");
            break;
        }

        case CDX_PSR_CMD_REPLACE_STREAM:
        // for dash and sms
            CDX_LOGD("replace stream! previous stream=%p, current stream=%p",impl->stream, stream);
            if(impl->stream)
            {
                CdxStreamClose(impl->stream);
            }
            impl->mErrno = PSR_OK;
            impl->stream = stream;  //
            c->fp = stream;        // the impl and c is the same stream, we must reset both of them
            c->moof_end_offset = CdxStreamTell(stream);
            break;

        case CDX_PSR_CMD_GET_CACHESTATE:
        {
            return MovGetCacheState(impl, param);
        }

        case CDX_PSR_CMD_SET_FORCESTOP:
            return __CdxMovParserForceStop(parser);
        case CDX_PSR_CMD_CLR_FORCESTOP:
            return __CdxMovParserClrForceStop(parser);
        default:
            break;
    }

    return 0;
}

static cdx_int32 __CdxMovParserSeekTo(CdxParserT *parser, cdx_int64  timeUs, SeekModeType seekModeType)
{
    struct CdxMovParser* tmpMovPsr = (struct CdxMovParser*)parser;
    MOVContext                    *c;

    c = (MOVContext*)tmpMovPsr->privData;

    c->chunk_info.no_completed_read = 0;
    c->chunk_info.cycle_num = 0;
    c->chunk_info.cycle_to  = 0;
    c->chunk_info.audio_segsz = 0;

    if(tmpMovPsr->exitFlag)
    {
        return -1;
    }

    tmpMovPsr->mStatus = CDX_MOV_SEEKING;
    int ret = CdxMovSeek(tmpMovPsr, timeUs, seekModeType);
    if(ret < 0)
    {
        CDX_LOGD("--- seek error");
        tmpMovPsr->mErrno = PSR_UNKNOWN_ERR;
        tmpMovPsr->mStatus = CDX_MOV_IDLE;
        return ret;
    }
    else if(ret == 1)
    {
        tmpMovPsr->mErrno = PSR_EOS; // reset the eos
        tmpMovPsr->mStatus = CDX_MOV_IDLE;
        return 0;
    }

    tmpMovPsr->mErrno = PSR_OK; // reset the eos
    tmpMovPsr->mStatus = CDX_MOV_IDLE;
    return 0;
}

static cdx_uint32 __CdxMovParserAttribute(CdxParserT *parser)
{
    struct CdxMovParser* tmpMovPsr = (struct CdxMovParser*)parser;
    CDX_UNUSE(tmpMovPsr);
    return -1;
}

cdx_int32 __CdxMovParserGetStatus(CdxParserT *parser)
{
    struct CdxMovParser* tmpMovPsr = (struct CdxMovParser*)parser;
    return tmpMovPsr->mErrno;
}

int __CdxMovParserInit(CdxParserT *parser)
{
    int ret;
    struct CdxMovParser* tmpMovPsr = (struct CdxMovParser*)parser;
    MOVContext *c = (MOVContext *)tmpMovPsr->privData;
    CdxAtomicInc(&tmpMovPsr->ref);

    //open media file to parse file information
    ret = CdxMovOpen(tmpMovPsr, tmpMovPsr->stream);
    c->select_video_stream_idx = 0;
    if(ret < 0)
    {
        CDX_LOGE("open mov/mp4 reader failed@");
        tmpMovPsr->mErrno = PSR_OPEN_FAIL;
        tmpMovPsr->mStatus = CDX_MOV_IDLE;
        CdxAtomicDec(&tmpMovPsr->ref);
        return -1;
    }

    if (c->bPlayreadySegment)
    {
        if(PlayReadyOpenDrm(c)<0)
        {
            CDX_LOGE("mov open playready drm fail!");
            tmpMovPsr->mErrno = PSR_OPEN_FAIL;
            CdxAtomicDec(&tmpMovPsr->ref);
            return -1;
        }
    }

    CDX_LOGV("***** mov open success!!");
    CdxAtomicDec(&tmpMovPsr->ref);
    tmpMovPsr->mErrno = PSR_OK;
    tmpMovPsr->mStatus = CDX_MOV_IDLE;
    return 0;
}

static struct CdxParserOpsS movParserOps =
{
    .control         = __CdxMovParserControl,
    .prefetch         = __CdxMovParserPrefetch,
    .read             = __CdxMovParserRead,
    .getMediaInfo     = __CdxMovParserGetMediaInfo,
    .close             = __CdxMovParserClose,
    .seekTo            = __CdxMovParserSeekTo,
    .attribute        = __CdxMovParserAttribute,
    .getStatus        = __CdxMovParserGetStatus,
    .init           = __CdxMovParserInit
};

static CdxParserT *__CdxMovParserOpen(CdxStreamT *stream, cdx_uint32 flags)
{
    int                       result;
    struct CdxMovParser       *tmpMovPsr;
    MOVContext          *c;

    if(flags > 0)
    {
        CDX_LOGV("mov parser is not support multi-stream yet!!!");
    }

    //init mov parser lib module
    tmpMovPsr = CdxMovInit(&result);
    if(!tmpMovPsr)
    {
        CDX_LOGE("Initiate mov file parser lib module failed!");
        goto error;
    }

    #if SAVE_VIDEO
    tmpMovPsr->fp = fopen("/data/camera/video.es", "wb");
    #endif

    c = (MOVContext*)tmpMovPsr->privData;
    tmpMovPsr->mErrno = PSR_INVALID;
    tmpMovPsr->mStatus = CDX_MOV_INITIALIZED;
    tmpMovPsr->stream = stream;
    CdxAtomicSet(&tmpMovPsr->ref, 1);

    if(CdxStreamAttribute(stream) & CDX_STREAM_FLAG_SEEK)
    {
        c->bSeekAble = 1;
    }
    else
    {
        c->bSeekAble = 0;
    }
    CDX_LOGV("--- c->bSeekAble = %d", c->bSeekAble);

    tmpMovPsr->parserinfo.ops = &movParserOps;

    if(result < 0)
    {
        CDX_LOGE("Initiate mov file parser lib module error!");
        goto error;
    }
    if(flags & SEGMENT_SMS) //* for sms
    {
        CDX_LOGD("smooth streaming...");
        tmpMovPsr->bSmsSegment = 1;
        c->bSmsSegment = 1;
    }
    if(flags & SEGMENT_PLAYREADY) //* for playready sms
    {
        CDX_LOGD("playready smooth streaming...");
        c->bPlayreadySegment = 1;
    }

    if(flags & SEGMENT_MP4)
    {
        CDX_LOGD("---- flags = %x", flags);
        tmpMovPsr->bDashSegment = 1;
        c->bDash = 1;
    }

    //result = pthread_create(&tmpMovPsr->openTid, NULL, MovOpenThread, (void*)tmpMovPsr);
    //if(result != 0)
    //{
    //    tmpMovPsr->openTid = (pthread_t)0;
    //}

    //initial some global parameter
    tmpMovPsr->nVidPtsOffset = 0;
    tmpMovPsr->nAudPtsOffset = 0;
    tmpMovPsr->nSubPtsOffset = 0;

    return &tmpMovPsr->parserinfo;

error:
    if(stream)
        CdxStreamClose(stream);
    return NULL;

}

static cdx_uint32 __CdxMovParserProbe(CdxStreamProbeDataT *probeData)
{
    /* check file header */
    if(probeData->len < 8)
    {
        CDX_LOGE("Probe data is not enough.");
        return 0;
    }

    cdx_uint32 type = GetLe32(probeData->buf+4);
    if(type == MKTAG( 'f', 't', 'y', 'p' ) || type == MKTAG( 'm', 'd', 'a', 't' )
        || type == MKTAG( 'm', 'o', 'o', 'f' ) || type == MKTAG( 's', 't', 'y', 'p' )
        || type == MKTAG( 'm', 'o', 'o', 'v' ) || type == MKTAG( 's', 'k', 'i', 'p' )
        || type == MKTAG( 'u', 'd', 't', 'a' )|| type == MKTAG( 'u', 'u', 'i', 'd' ))
    {
        CDX_LOGV(" --- probe: it is mov parser");
        return 100;
    }

    return 0;
}

CdxParserCreatorT movParserCtor =
{
    .create = __CdxMovParserOpen,
    .probe     = __CdxMovParserProbe
};

