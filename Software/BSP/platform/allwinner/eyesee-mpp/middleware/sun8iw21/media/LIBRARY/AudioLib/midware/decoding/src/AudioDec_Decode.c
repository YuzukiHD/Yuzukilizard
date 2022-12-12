#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ad_cedarlib_com.h"
#include "cedar_abs_packet_hdr.h"
#include "AudioDec_Decode.h"
#include "CDX_Fileformat.h"
#include "CDX_Common.h"

#include <dlfcn.h>
#ifdef MEMCHECK
#include "memname.h"
#include "memcheck.h"
#endif

#include "alib_log.h"

#ifdef  LOG_TAG
#undef  LOG_TAG
#define LOG_TAG "AllwinnerAlibs"
#endif

#ifdef  ALIB_DEBUG
#undef  ALIB_DEBUG
#define ALIB_DEBUG 1
#endif

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

//#define AUTH_T

//-----------------------------------version information---------------------------------------->
#define REPO_TAG "audiocodec-v1.2"
#define REPO_BRANCH "new"
#define REPO_COMMIT "3ba65962c01cbf1280ddda19d843009b6ef8ce85"
#define REPO_DATE "Tue Jan 8 16:25:27 2019 +0800"

static inline void LogVersionInfo(void)
{
    alib_logd("\n"
         ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> Audio <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n"
         "tag   : %s\n"
         "branch: %s\n"
         "commit: %s\n"
         "date  : %s\n"
         "----------------------------------------------------------------------\n",
         REPO_TAG, REPO_BRANCH, REPO_COMMIT, REPO_DATE);
}
//<------------------------------------------------------------------------------------------


/////////////////////Cedar Codecs Array Defined From Here///////////////////////
#define AD_CEDAR_CTRL_FLAG_BUILD_WMAHDR  (1<<0)
#define AD_CEDAR_CTRL_FLAG_BUILD_AACHDR  (1<<1)
#define AD_CEDAR_CTRL_FLAG_BUILD_RAHDR   (1<<2)
#define AD_CEDAR_CTRL_FLAG_BUILD_AMRHDR  (1<<3)
#define AD_CEDAR_CTRL_FLAG_BUILD_COMMONHDR  (1<<4)
#define AD_CEDAR_CTRL_FLAG_BUILD_PCMHDR  (1<<5)
#define AD_CEDAR_CTRL_FLAG_BUILD_APEHDR  (1<<6)
#define AD_CEDAR_CTRL_FLAG_BUILD_RMVB_SIPR_HDR (1<<7)

#define ALIB_SAFE_FREE(ptr) if(ptr != NULL){ \
                                free(ptr); \
                                ptr = NULL; \
                            }

#define IS_RMVB_INFO_HEADER(p) ((p[0] == 0xff) && (p[1] == 0xff) && \
                                (p[2] == 0xff) && (p[3] == 0xff))


#define ADEC_PREFIX         Audio
#define ADEC_INIT_SUFFIX    DecInit
#define ADEC_EXIT_SUFFIX    DecExit
#define ADEC_AAC            AAC
#define ADEC_WAV            WAV
#define ADEC_MP3            MP3
#define ADEC_G711A          G711a
#define ADEC_G711U          G711u
#define ADEC_G726          G726

#define __ADEC_FUNC_NAME1(FuncPrefix, Name, FuncSuffix) FuncPrefix##Name##FuncSuffix
#define __ADEC_FUNC_NAME0(FuncPrefix, Name, FuncSuffix) __ADEC_FUNC_NAME1(FuncPrefix, Name, FuncSuffix)
#define ADEC_FUNC_NAME(Name, FuncSuffix) __ADEC_FUNC_NAME0(ADEC_PREFIX, Name, FuncSuffix)

#define ADEC_AAC_INIT_FUNC  ADEC_FUNC_NAME(ADEC_AAC, ADEC_INIT_SUFFIX) //"AudioAACDecInit"
#define ADEC_AAC_EXIT_FUNC  ADEC_FUNC_NAME(ADEC_AAC, ADEC_EXIT_SUFFIX) //"AudioAACDecExit"
#define ADEC_WAV_INIT_FUNC  ADEC_FUNC_NAME(ADEC_WAV, ADEC_INIT_SUFFIX) //"AudioWAVDecInit",
#define ADEC_WAV_EXIT_FUNC  ADEC_FUNC_NAME(ADEC_WAV, ADEC_EXIT_SUFFIX) //"AudioWAVDecExit",
#define ADEC_MP3_INIT_FUNC  ADEC_FUNC_NAME(ADEC_MP3, ADEC_INIT_SUFFIX) //"AudioMP3DecInit",
#define ADEC_MP3_EXIT_FUNC  ADEC_FUNC_NAME(ADEC_MP3, ADEC_EXIT_SUFFIX) //"AudioMP3DecExit",
#define ADEC_G711A_INIT_FUNC  ADEC_FUNC_NAME(ADEC_G711A, ADEC_INIT_SUFFIX) //"AudioG711aDecInit",
#define ADEC_G711A_EXIT_FUNC  ADEC_FUNC_NAME(ADEC_G711A, ADEC_EXIT_SUFFIX) //"AudioG711aDecExit",
#define ADEC_G711U_INIT_FUNC  ADEC_FUNC_NAME(ADEC_G711U, ADEC_INIT_SUFFIX) //"AudioG711uDecInit",
#define ADEC_G711U_EXIT_FUNC  ADEC_FUNC_NAME(ADEC_G711U, ADEC_EXIT_SUFFIX) //"AudioG711uDecExit",
#define ADEC_G726_INIT_FUNC  ADEC_FUNC_NAME(ADEC_G726, ADEC_INIT_SUFFIX)   //"AudioG726DecInit",
#define ADEC_G726_EXIT_FUNC  ADEC_FUNC_NAME(ADEC_G726, ADEC_EXIT_SUFFIX)   //"AudioG726DecExit",

#define _STRING(x) #x
#define STRING(x) _STRING(x)

#define DECLARE_ADEC_INIT_FUNC(Name) extern struct __AudioDEC_AC320 *Name(void)
#define DECLARE_ADEC_EXIT_FUNC(Name) extern int Name(struct __AudioDEC_AC320 *p)

InitCodecInfo InitAacDecT = {
    .handle = "libaw_aacdec.so",
    .name   = "cedaraac",
    .init   = STRING(ADEC_AAC_INIT_FUNC), //"AudioAACDecInit",
    .exit   = STRING(ADEC_AAC_EXIT_FUNC), //"AudioAACDecExit",
    .flag   = AD_CEDAR_CTRL_FLAG_BUILD_AACHDR,
};
/*#if 1
InitCodecInfo InitWmaPassThru = {
    .handle = "libWmaPassThru.so",
    .name   = "cedarwmaPassThru",
    .init   = "AudiowmaPassThruInit",
    .exit   = "AudiowmaPassThruExit",
    .flag   = AD_CEDAR_CTRL_FLAG_BUILD_WMAHDR,
};
#endif
InitCodecInfo InitApeDecT = {
    .handle = "libaw_apedec.so",
    .name   = "cedarape",
    .init   = "AudioAPEDecInit",
    .exit   = "AudioAPEDecExit",
    .flag   = AD_CEDAR_CTRL_FLAG_BUILD_APEHDR,
};

InitCodecInfo InitCookDecT = {
    .handle = "libaw_cookdec.so",
    .name   = "cedarcook",
    .init   = "AudioCOOKDecInit",
    .exit   = "AudioCOOKDecExit",
    .flag   = AD_CEDAR_CTRL_FLAG_BUILD_RAHDR,
};

InitCodecInfo InitSiprDecT = {
    .handle = "libaw_siprdec.so",
    .name   = "cedarsipr",
    .init   = "AudioSIPRDecInit",
    .exit   = "AudioSIPRDecExit",
    .flag   = AD_CEDAR_CTRL_FLAG_BUILD_RMVB_SIPR_HDR,
};

InitCodecInfo InitAtrcDecT = {
    .handle = "libaw_atrcdec.so",
    .name   = "cedaratrc",
    .init   = "AudioATRCDecInit",
    .exit   = "AudioATRCDecExit",
    .flag   = AD_CEDAR_CTRL_FLAG_BUILD_RAHDR,
};
InitCodecInfo InitOggDecT = {
    .handle = "libaw_oggdec.so",
    .name   = "cedarogg",
    .init   = "AudioOGGDecInit",
    .exit   = "AudioOGGDecExit",
    .flag   = AD_CEDAR_CTRL_FLAG_BUILD_COMMONHDR,
};*/
InitCodecInfo InitWavDecT = {
    .handle = "libaw_wavdec.so",
    .name   = "cedarwav",
    .init   = STRING(ADEC_WAV_INIT_FUNC), //"AudioWAVDecInit",
    .exit   = STRING(ADEC_WAV_EXIT_FUNC), //"AudioWAVDecExit",
    .flag   = AD_CEDAR_CTRL_FLAG_BUILD_PCMHDR,
};
/*InitCodecInfo InitAmrDecT = {
    .handle = "libaw_amrdec.so",
    .name   = "cedaramr",
    .init   = "AudioAMRDecInit",
    .exit   = "AudioAMRDecExit",
    .flag   = AD_CEDAR_CTRL_FLAG_BUILD_AMRHDR,
};
InitCodecInfo InitFlacDecT = {
    .handle = "libaw_flacdec.so",
    .name   = "cedarflac",
    .init   = "AudioFLACDecInit",
    .exit   = "AudioFLACDecExit",
    .flag   = AD_CEDAR_CTRL_FLAG_BUILD_COMMONHDR,
};*/
InitCodecInfo InitMp3DecT = {
    .handle = "libaw_mp3dec.so",
    .name   = "cedarmp3",
    .init   = STRING(ADEC_MP3_INIT_FUNC), //"AudioMP3DecInit",
    .exit   = STRING(ADEC_MP3_EXIT_FUNC), //"AudioMP3DecExit",
    .flag   = 0,
};

InitCodecInfo InitG711aDecT = {
    .handle = "libaw_g711adec.so",
    .name   = "cedarg711a",
    .init   = STRING(ADEC_G711A_INIT_FUNC),     //  "AudioG711aDecInit"
    .exit   = STRING(ADEC_G711A_EXIT_FUNC),     //  "AudioG711aDecExit"
    .flag   = 0,
};
InitCodecInfo InitG711uDecT = {
    .handle = "libaw_g711udec.so",
    .name   = "cedarg711u",
    .init   = STRING(ADEC_G711U_INIT_FUNC),     //  "AudioG711uDecInit"
    .exit   = STRING(ADEC_G711U_EXIT_FUNC),     //  "AudioG711uDecExit"
    .flag   = 0,
};
    
InitCodecInfo InitG726DecT = {
    .handle = "libaw_g726dec.so",
    .name   = "cedarg726",
    .init   = STRING(ADEC_G726_INIT_FUNC),     //  "AudioG726DecInit"
    .exit   = STRING(ADEC_G726_EXIT_FUNC),     //  "AudioG726DecExit"
    .flag   = 0,
};


/*InitCodecInfo InitRaDecT = {
    .handle = "libaw_radec.so",
    .name   = "cedarra",
    .init   = "AudioRADecInit",
    .exit   = "AudioRADecExit",
    .flag   = AD_CEDAR_CTRL_FLAG_BUILD_COMMONHDR,
};
InitCodecInfo InitAlacDecT = {
    .handle = "libaw_alacdec.so",
    .name   = "cedaralac",
    .init   = "AudioALACDecInit",
    .exit   = "AudioALACDecExit",
    .flag   = AD_CEDAR_CTRL_FLAG_BUILD_COMMONHDR,
};
InitCodecInfo InitG729DecT = {
    .handle = "libaw_g729dec.so",
    .name   = "cedarg729",
    .init   = "AudioG729DecInit",
    .exit   = "AudioG729DecExit",
    .flag   = 0,
};
InitCodecInfo InitDsdDecT = {
    .handle = "libaw_dsddec.so",
    .name   = "cedardsd",
    .init   = "AudioDSDDecInit",
    .exit   = "AudioDSDDecExit",
    .flag   = AD_CEDAR_CTRL_FLAG_BUILD_COMMONHDR,
};
InitCodecInfo InitOpusDecT = {
    .handle = "libaw_opusdec.so",
    .name   = "cedaropus",
    .init   = "AudioOpusDecInit",
    .exit   = "AudioOpusDecExit",
    .flag   = AD_CEDAR_CTRL_FLAG_BUILD_COMMONHDR,
};*/


InitCodecInfo* InitCodesT[] =
{
    &InitAacDecT,
    0,
    0,
    0,//&InitWmaPassThru,
    0,//&InitApeDecT,
    0,//&InitCookDecT,
    0,//&InitSiprDecT,
    0,//&InitAtrcDecT,
    0,//&InitOggDecT,
    &InitWavDecT,
    0,//&InitAmrDecT,
    0,//&InitFlacDecT,
    &InitMp3DecT,
    0,//&InitRaDecT,
    0,//&InitAlacDecT,
    0,//&InitG729DecT,
    0,//&InitDsdDecT,
    0,//&InitOpusDecT,
    &InitG711aDecT, 
    &InitG711uDecT,
    &InitG726DecT
};

enum AUDIO_CODEC_INDEX
{
    AUDIO_CODEC_INDEX_AAC,
    AUDIO_CODEC_INDEX_ERRORA,
    AUDIO_CODEC_INDEX_ERRORD,
    AUDIO_CODEC_INDEX_ERRORW,
    AUDIO_CODEC_INDEX_APE,
    AUDIO_CODEC_INDEX_COOK,
    AUDIO_CODEC_INDEX_SIPR,
    AUDIO_CODEC_INDEX_ATRC,
    AUDIO_CODEC_INDEX_OGG,
    AUDIO_CODEC_INDEX_WAV,
    AUDIO_CODEC_INDEX_AMR,
    AUDIO_CODEC_INDEX_FLAC,
    AUDIO_CODEC_INDEX_MP3,
    AUDIO_CODEC_INDEX_RA,
    AUDIO_CODEC_INDEX_ALAC,
    AUDIO_CODEC_INDEX_G729,
    AUDIO_CODEC_INDEX_DSD,
    AUDIO_CODEC_INDEX_OPUS,
    AUDIO_CODEC_INDEX_G711a,
    AUDIO_CODEC_INDEX_G711u,
    AUDIO_CODEC_INDEX_G726,
};

/////////////////////Cedar Codecs Array Defined End Here///////////////////////

#ifdef __AD_CEDAR_DBG_WRITEOUT_BITSTREAM
FILE *pFileBs = NULL;
#endif

#ifdef __AD_CEDAR_DBG_WRITEOUT_PCM_BUFFER
FILE *pFilePcm = NULL;
#endif

static void FlushPtsAtOnce(void *mem)
{
    Ac320FileRead *FileInfo = (Ac320FileRead *)mem;
    AudioDecoderContextLib *pAudioDecodeLib = NULL;

    if(FileInfo == NULL)
    {
        return ;
    }

    pAudioDecodeLib  = (AudioDecoderContextLib *)FileInfo->tmpGlobalAudioDecData;

    if(FileInfo->frmFifo->ValidchunkCnt>0 && FileInfo->frmFifo->inFrames[FileInfo->frmFifo->rdIdx].ptsValid)
    {
        if(pAudioDecodeLib->pCedarAudioDec->BsInformation->NowPTSTime !=FileInfo->frmFifo->inFrames[FileInfo->frmFifo->rdIdx].pts)
        {
            pAudioDecodeLib->pCedarAudioDec->BsInformation->NowPTSTime = FileInfo->frmFifo->inFrames[FileInfo->frmFifo->rdIdx].pts;
        }
        FileInfo->frmFifo->inFrames[FileInfo->frmFifo->rdIdx].ptsValid = 0;
    }
}

static int ShowAbsBits(void *buf, int Len, void *mem)   // read len bytes data from bs buffer
{
    Ac320FileRead *FileInfo = (Ac320FileRead *)mem;
    AudioDecoderContextLib *pAudioDecodeLib  = NULL;
    int     Retlen = 0;

    if(FileInfo == NULL)
    {
        return 0;
    }

    pAudioDecodeLib  = (AudioDecoderContextLib *)(FileInfo->tmpGlobalAudioDecData);
    while(1)
    {
        if(FileInfo->BufLen >= Len)
        {
            if((FileInfo->bufReadingPtr+Len) > (FileInfo->bufStart+FileInfo->BufToTalLen))
            {
                int len1 = (FileInfo->bufStart+FileInfo->BufToTalLen-FileInfo->bufReadingPtr);
                memcpy((void *)buf,(void *)FileInfo->bufReadingPtr,len1);
                memcpy((void *)((char *)buf+len1),(void *)FileInfo->bufStart,Len-len1);
            }
            else
            {
                memcpy(buf,FileInfo->bufReadingPtr,Len);
            }

            if(!FileInfo->BigENDFlag)
            {
                int     count;
                short   *wordptr = (short *)buf;
                short   byte1,byte2,word;
                for (count = 0; count < Len/2; count++)
                {
                    word = *wordptr;//pk_buf[count];
                    byte1 = ( word << 8 ) & 0xff00;
                    byte2 = ( word >> 8 ) & 0x00ff;
                    *wordptr++ = (short)(byte1 | byte2);
                }
            }
            Retlen = Len;
            break;
        }
        else
        {
            //TODO: how to deal here ??
            // nShowBitsReturn is a legarcy variable, never used...
            if(pAudioDecodeLib->pCedarAudioDec->BsInformation->nShowBitsReturn)
            {
                //alib_logv("audio showbits empty...\n");
                pAudioDecodeLib->pInternal.ulExitMean= 0;//clear exit
                pAudioDecodeLib->pCedarAudioDec->BsInformation->nBitStreamUnderFlow = 1;
                Len = FileInfo->BufLen;
                if((FileInfo->bufReadingPtr+Len) > (FileInfo->bufStart+FileInfo->BufToTalLen) )
                {
                    int len1 = (FileInfo->bufStart+FileInfo->BufToTalLen-FileInfo->bufReadingPtr);
                    memcpy((void *)buf,(void *)FileInfo->bufReadingPtr,len1);
                    memcpy((void *)((char *)buf+len1),(void *)FileInfo->bufStart,Len-len1);
                }
                else
                {
                    memcpy(buf,FileInfo->bufReadingPtr,Len);
                }

                if(!FileInfo->BigENDFlag)
                {
                    int     count;
                    short   *wordptr = (short *)buf;
                    short   byte1,byte2,word;
                    for(count=0; count<Len/2; count++)
                    {
                        word = *wordptr;//pk_buf[count];
                        byte1 = ( word << 8 ) & 0xff00;
                        byte2 = ( word >> 8 ) & 0x00ff;
                        *wordptr++ = (short)(byte1 | byte2);
                    }
                }

                Retlen = Len;
                break;
            }
            else
            {
                if(pAudioDecodeLib->pInternal.ulExitMean == 1)
                {
                    return 0;
                }
                else
                {
                    usleep(5000); //.tbd add semphore for future
                }
            }
        }
    }
    return Retlen;
}

static void FlushAbsBits(int Len, void *mem)
{
    int nDataLen  = 0;  // merged data size
    int nFrameLen = 0;
    Ac320FileRead *FileInfo = (Ac320FileRead *)mem;
    AudioDecoderContextLib *pAudioDecodeLib = NULL;

    if(FileInfo == NULL)
    {
        return ;
    }

    pAudioDecodeLib = (AudioDecoderContextLib *)FileInfo->tmpGlobalAudioDecData;
    //check if length of audio bitstream is valid
    if(Len > FileInfo->BufLen)
    {
        //alib_logw("length of audio bitstream to be flushed is invalid!\n");
        //flush all audio bitstream
        Len = FileInfo->BufLen;
    }

    cdx_mutex_lock(&pAudioDecodeLib->mutex_audiodec_thread);
    FileInfo->bufReadingPtr += Len;
    if(FileInfo->bufReadingPtr >= FileInfo->bufStart+FileInfo->BufToTalLen)
    {
        FileInfo->bufReadingPtr -= FileInfo->BufToTalLen;
    }
    FileInfo->BufLen -= Len;
    FileInfo->BufValideLen += Len;
    cdx_mutex_unlock(&pAudioDecodeLib->mutex_audiodec_thread);
    //alib_logv("--buflen:%d,len:%d\n",FileInfo->BufLen,Len);
    //some audio bitstream buffer is free, send message to wake up audio decoder

    FileInfo->FileReadingOpsition += Len;

    FlushPtsAtOnce(FileInfo);

    cdx_mutex_lock(&pAudioDecodeLib->mutex_audiodec_thread);

    while((Len>nDataLen)&&(FileInfo->frmFifo->ValidchunkCnt))
    {
        nFrameLen = FileInfo->frmFifo->inFrames[FileInfo->frmFifo->rdIdx].len
            - FileInfo->frmFifo->inFrames[FileInfo->frmFifo->rdIdx].readlen;
        if( nFrameLen <= (Len - nDataLen))
        {
            FileInfo->frmFifo->ValidchunkCnt--;
            nDataLen += nFrameLen;
            FileInfo->frmFifo->rdIdx++;
            if(FileInfo->frmFifo->rdIdx > (AUDIO_BITSTREAM_BUFFER_MAX_FRAME_NUM - 1))
            {
                    FileInfo->frmFifo->rdIdx = 0;
            }
        }
        else
        {
            FileInfo->frmFifo->inFrames[FileInfo->frmFifo->rdIdx].readlen += Len - nDataLen;
            nDataLen += Len - nDataLen;
        }
    }

    cdx_mutex_unlock(&pAudioDecodeLib->mutex_audiodec_thread);
}

static int FREAD_AC320(void *buf, int Len, void *mem)
{
    int     Retlen;
    Ac320FileRead *FileInfo = (Ac320FileRead *)mem;

    Retlen = ShowAbsBits(buf, Len, (void*)FileInfo);
    FlushAbsBits(Retlen, (void*)FileInfo);

    //alib_logv("----FREAD_AC320:%d,%d\n",Len,Retlen);
    return Retlen;
}

static int FSeek_AC320(int Len, void *mem)
{
    int nDataLen = 0;
    int nFrameLen = 0;
    Ac320FileRead *FileInfo = (Ac320FileRead *)mem;
    AudioDecoderContextLib *pAudioDecodeLib = NULL;

    if(FileInfo == NULL)
    {
        return 0;
    }

    pAudioDecodeLib  = (AudioDecoderContextLib *)FileInfo->tmpGlobalAudioDecData;

    if(Len == 0)
    {
        return 0;
    }
    //alib_logv("ad_cedar fseek %d",Len);
    cdx_mutex_lock(&pAudioDecodeLib->mutex_audiodec_thread);
    if(Len < 0)
    {
        FileInfo->bufReadingPtr = FileInfo->bufWritingPtr;
        FileInfo->BufValideLen = FileInfo->BufToTalLen;
        FileInfo->BufLen = 0;
        FileInfo->FileReadingOpsition += Len;
        FileInfo->updataoffsetflag = pAudioDecodeLib->pCedarAudioDec->BsInformation->nDecodeMode;
        FileInfo->need_offset = FileInfo->FileReadingOpsition;
        //maybe flush frmFifo
        memset(FileInfo->frmFifo->inFrames, 0, AUDIO_BITSTREAM_BUFFER_MAX_FRAME_NUM * sizeof(aframe_info_t));
        FileInfo->frmFifo->maxchunkNum = AUDIO_BITSTREAM_BUFFER_MAX_FRAME_NUM;
        FileInfo->frmFifo->rdIdx = 0;
        FileInfo->frmFifo->wtIdx = 0;
        FileInfo->frmFifo->ValidchunkCnt = 0;
        FileInfo->frmFifo->dLastValidPTS = -1;
    }
    else
    {
        if(Len > FileInfo->BufLen)
        {
            //flush bs buffer
            FileInfo->bufReadingPtr = FileInfo->bufWritingPtr;
            FileInfo->BufValideLen = FileInfo->BufToTalLen;
            FileInfo->BufLen = 0;
            FileInfo->FileReadingOpsition += Len;
            FileInfo->updataoffsetflag = pAudioDecodeLib->pCedarAudioDec->BsInformation->nDecodeMode;
            FileInfo->need_offset = FileInfo->FileReadingOpsition;
            memset(FileInfo->frmFifo->inFrames, 0, AUDIO_BITSTREAM_BUFFER_MAX_FRAME_NUM * sizeof(aframe_info_t));
            FileInfo->frmFifo->maxchunkNum = AUDIO_BITSTREAM_BUFFER_MAX_FRAME_NUM;
            FileInfo->frmFifo->rdIdx = 0;
            FileInfo->frmFifo->wtIdx = 0;
            FileInfo->frmFifo->ValidchunkCnt = 0;
            FileInfo->frmFifo->dLastValidPTS = -1;
        }
        else
        {
            FileInfo->BufValideLen += Len;
            FileInfo->bufReadingPtr += Len;
            FileInfo->BufLen -= Len;
            FileInfo->FileReadingOpsition += Len;
            if(FileInfo->bufReadingPtr >= FileInfo->bufStart+FileInfo->BufToTalLen)
            {
                FileInfo->bufReadingPtr -= FileInfo->BufToTalLen;
            }
            while((Len>nDataLen)&&(FileInfo->frmFifo->ValidchunkCnt))
            {
                nFrameLen = FileInfo->frmFifo->inFrames[FileInfo->frmFifo->rdIdx].len
                         - FileInfo->frmFifo->inFrames[FileInfo->frmFifo->rdIdx].readlen;
                if( nFrameLen <= (Len - nDataLen))
                {
                    FileInfo->frmFifo->ValidchunkCnt --;
                    nDataLen += nFrameLen;
                    FileInfo->frmFifo->rdIdx ++;
                    if(FileInfo->frmFifo->rdIdx > (AUDIO_BITSTREAM_BUFFER_MAX_FRAME_NUM - 1))
                    {
                         FileInfo->frmFifo->rdIdx = 0;
                    }
                }
                else
                {
                    FileInfo->frmFifo->inFrames[FileInfo->frmFifo->rdIdx].readlen += Len - nDataLen;
                    nDataLen += Len - nDataLen;
                }
            }
            //alib_logv("FSeek_AC320_frmFifo_info:flush rdIdx[%d]\n", FileInfo->frmFifo->rdIdx);
        }
    }
    cdx_mutex_unlock(&pAudioDecodeLib->mutex_audiodec_thread);
    return 0;
}

static int FSeek_AC320_Origin(int Len, void *mem, int origin)
{
    Ac320FileRead *FileInfo = (Ac320FileRead *)mem;

    if(FileInfo == NULL)
    {
        return -1;
    }

    if(origin == 0)
    {
        if(Len < 0)
        {
            return -1;
        }
        Len -= FileInfo->FileReadingOpsition;
    }

    return FSeek_AC320(Len, (void*)FileInfo);
}

static int BigLitEndianCheck(void *mem, short data)
{
    int     ret = -1;
    int     Rlen = 0;
    char    buff[2];
    short   data1, data0, data2, data3;
    int     i;
    Ac320FileRead *FileInfo = (Ac320FileRead *)mem;

    if(FileInfo == NULL)
    {
        return -1;
    }

    Rlen = ShowAbsBits((void *)(buff), 2, FileInfo);
    if(Rlen != 2)
    {
        return -1;
    }

    for(i=0;i<0x100000;i++)//while(1)
    {
        data1 = buff[1] & 0xff;
        data0 = buff[0] & 0xff;
        data2 = (data1<<8) | data0;
        data3 = (data0<<8) | data1;
        if(data2 == data)
        {
            ret = 0;
            break;
        }
        else if(data3 == data)
        {
            ret = 1;
            FileInfo->BigENDFlag = 0;
            break;
        }

        FlushAbsBits(1, FileInfo);
        Rlen = ShowAbsBits((void *)(buff), 2, FileInfo);
        if(Rlen != 2)
        {
            return -1;
        }
    }

    return ret;
}

void Register_RW_APIs(Ac320FileRead* FileInfo)
{
    FileInfo->ShowAbsBits = ShowAbsBits;
    FileInfo->FlushAbsBits = FlushAbsBits;
    FileInfo->FREAD_AC320 = FREAD_AC320;
    FileInfo->FSeek_AC320 = FSeek_AC320;
    FileInfo->FSeek_AC320_Origin = FSeek_AC320_Origin;
    FileInfo->BigLitEndianCheck = BigLitEndianCheck;
    FileInfo->FlushPtsAtOnce = FlushPtsAtOnce;
}

static void* Wrapper_dlsym(const char* libname, void* handle, const char* symbol)
{
    (void)libname;
    return dlsym(handle, symbol);
}

static void* Wrapper_dlopen(const char* libname, int mode)
{
    //alib_logd("%s open, use dlopen!",libname);
    return dlopen(libname, mode);
}

static void Wrapper_dlclose(const char* libname, void* handle)
{
    //alib_logd("%s close, use dlclose!",libname);
    dlclose(handle);
    return;
}

static const char* Wrapper_dlerror(const char* libname)
{
    (void)libname;
    return dlerror();
}

static int InitCodec(AudioDecoderContextLib* pAudioDecodeLib, InitCodecInfo* initcodecinfo)
{
    if(!pAudioDecodeLib || !initcodecinfo)
    {
        alib_loge("InitCodec(%p) or pAudioDecodeLib NULL! broken down...", initcodecinfo);
        return -1;
    }
    pAudioDecodeLib->handlename = "none";

#if !defined(ADEC_CONFIG_COMPILE_STATIC_LIB) || (0==ADEC_CONFIG_COMPILE_STATIC_LIB)
    pAudioDecodeLib->libhandle = Wrapper_dlopen(initcodecinfo->handle, RTLD_NOW);
    if(pAudioDecodeLib->libhandle == NULL)
    {
        alib_loge("----Loading so fail");
        return -1;
    }
    else
    {
        pAudioDecodeLib->handlename = initcodecinfo->handle;
        alib_loge("----Loading so success:%s!",initcodecinfo->name);
    }
#endif
    pAudioDecodeLib->pCedarCodec = (CedarAudioCodec*)malloc(sizeof(CedarAudioCodec));
    if(!pAudioDecodeLib->pCedarCodec)
    {
        alib_loge("pAudioDecodeLib->pCedarCodec malloc fail! broken down...");
        if(pAudioDecodeLib->libhandle)
        {
            Wrapper_dlclose(pAudioDecodeLib->handlename, pAudioDecodeLib->libhandle);
            pAudioDecodeLib->libhandle = NULL;
        }
        return -1;
    }

    memset(pAudioDecodeLib->pCedarCodec, 0x00, sizeof(CedarAudioCodec));
    pAudioDecodeLib->pCedarCodec->name = initcodecinfo->name;
#if !defined(ADEC_CONFIG_COMPILE_STATIC_LIB) || (0==ADEC_CONFIG_COMPILE_STATIC_LIB)
    pAudioDecodeLib->pCedarCodec->init = Wrapper_dlsym(pAudioDecodeLib->handlename, pAudioDecodeLib->libhandle, initcodecinfo->init);
    pAudioDecodeLib->pCedarCodec->exit = Wrapper_dlsym(pAudioDecodeLib->handlename, pAudioDecodeLib->libhandle, initcodecinfo->exit);
#else
    if(0 == strcmp(initcodecinfo->init, STRING(ADEC_AAC_INIT_FUNC)))
    {
        DECLARE_ADEC_INIT_FUNC(ADEC_AAC_INIT_FUNC); //extern struct __AudioDEC_AC320 *AudioAACDecInit(void);
        DECLARE_ADEC_EXIT_FUNC(ADEC_AAC_EXIT_FUNC); //extern int AudioAACDecExit(struct __AudioDEC_AC320 *p);
        pAudioDecodeLib->pCedarCodec->init = &ADEC_AAC_INIT_FUNC; //&AudioAACDecInit;
        pAudioDecodeLib->pCedarCodec->exit = &ADEC_AAC_EXIT_FUNC; //&AudioAACDecExit;
    }
    else if(0 == strcmp(initcodecinfo->init, STRING(ADEC_WAV_INIT_FUNC)))
    {
        DECLARE_ADEC_INIT_FUNC(ADEC_WAV_INIT_FUNC); //extern struct __AudioDEC_AC320 *AudioWAVDecInit(void);
        DECLARE_ADEC_EXIT_FUNC(ADEC_WAV_EXIT_FUNC); //extern int AudioWAVDecExit(struct __AudioDEC_AC320 *p);
        pAudioDecodeLib->pCedarCodec->init = &ADEC_WAV_INIT_FUNC; //&AudioWAVDecInit;
        pAudioDecodeLib->pCedarCodec->exit = &ADEC_WAV_EXIT_FUNC; //&AudioWAVDecExit;
    }
    else if(0 == strcmp(initcodecinfo->init, STRING(ADEC_MP3_INIT_FUNC)))
    {
        DECLARE_ADEC_INIT_FUNC(ADEC_MP3_INIT_FUNC); //extern struct __AudioDEC_AC320 *AudioMP3DecInit(void);
        DECLARE_ADEC_EXIT_FUNC(ADEC_MP3_EXIT_FUNC); //extern int AudioMP3DecExit(struct __AudioDEC_AC320 *p);
        pAudioDecodeLib->pCedarCodec->init = &ADEC_MP3_INIT_FUNC; //&AudioMP3DecInit;
        pAudioDecodeLib->pCedarCodec->exit = &ADEC_MP3_EXIT_FUNC; //&AudioMP3DecExit;
    }
//    else if(0 == strcmp(initcodecinfo->init, STRING(ADEC_G711A_INIT_FUNC)))
//    {
//        DECLARE_ADEC_INIT_FUNC(ADEC_G711A_INIT_FUNC); //extern struct __AudioDEC_AC320 *AudioG711aDecInit(void);
//        DECLARE_ADEC_EXIT_FUNC(ADEC_G711A_EXIT_FUNC); //extern int AudioG711aDecExit(struct __AudioDEC_AC320 *p);
//        pAudioDecodeLib->pCedarCodec->init = &ADEC_G711A_INIT_FUNC; //&AudioG711aDecInit;
//        pAudioDecodeLib->pCedarCodec->exit = &ADEC_G711A_EXIT_FUNC; //&AudioG711aDecExit;
//    }
//    else if(0 == strcmp(initcodecinfo->init, STRING(ADEC_G711U_INIT_FUNC)))
//    {
//        DECLARE_ADEC_INIT_FUNC(ADEC_G711U_INIT_FUNC); //extern struct __AudioDEC_AC320 *AudioG711uDecInit(void);
//        DECLARE_ADEC_EXIT_FUNC(ADEC_G711U_EXIT_FUNC); //extern int AudioG711uDecExit(struct __AudioDEC_AC320 *p);
//        pAudioDecodeLib->pCedarCodec->init = &ADEC_G711U_INIT_FUNC; //&AudioG711uDecInit;
//        pAudioDecodeLib->pCedarCodec->exit = &ADEC_G711U_EXIT_FUNC; //&AudioG711uDecExit;
//    }
    else if(0 == strcmp(initcodecinfo->init, STRING(ADEC_G726_INIT_FUNC)))
    {
        DECLARE_ADEC_INIT_FUNC(ADEC_G726_INIT_FUNC); //extern struct __AudioDEC_AC320 *AudioG726DecInit(void);
        DECLARE_ADEC_EXIT_FUNC(ADEC_G726_EXIT_FUNC); //extern int AudioG726DecExit(struct __AudioDEC_AC320 *p);
        pAudioDecodeLib->pCedarCodec->init = &ADEC_G726_INIT_FUNC; //&AudioG726DecInit;
        pAudioDecodeLib->pCedarCodec->exit = &ADEC_G726_EXIT_FUNC; //&AudioG726DecExit;
    }
    else
    {
        alib_loge("fatal error! audio dec[%s] is not supported now.", initcodecinfo->name);
    }
#endif
    alib_logv("%s, %s, %p, %p", pAudioDecodeLib->handlename, initcodecinfo->init, pAudioDecodeLib->pCedarCodec->init, pAudioDecodeLib->pCedarCodec->exit);
    pAudioDecodeLib->pCedarCodec->flag = initcodecinfo->flag;
    if(!pAudioDecodeLib->pCedarCodec->init || !pAudioDecodeLib->pCedarCodec->exit)
    {
        if(pAudioDecodeLib->libhandle)
        {
            Wrapper_dlclose(pAudioDecodeLib->handlename, pAudioDecodeLib->libhandle);
            pAudioDecodeLib->libhandle = NULL;
        }
        if(pAudioDecodeLib->pCedarCodec)
        {
            free(pAudioDecodeLib->pCedarCodec);
            pAudioDecodeLib->pCedarCodec = NULL;
        }
        alib_loge("%s get some dlsym fail! broken down....", initcodecinfo->handle);
        return -1;
    }
    return 0;
}

static void ExitCodec(AudioDecoderContextLib* pAudioDecodeLib)
{
    if(!pAudioDecodeLib)
    {
        alib_loge("ExitCodec, pAudioDecodeLib is NULL!");
        return ;
    }
    if(pAudioDecodeLib->libhandle)
    {
        Wrapper_dlclose(pAudioDecodeLib->handlename, pAudioDecodeLib->libhandle);
        pAudioDecodeLib->libhandle = NULL;
        alib_logd("----dlclose so success!");
    }
    if(pAudioDecodeLib->pCedarCodec)
    {
        free(pAudioDecodeLib->pCedarCodec);
        pAudioDecodeLib->pCedarCodec = NULL;
    }
}

static int SetAudioBsHeader(AudioDecoderContextLib *pAudioDecodeLib, unsigned char *pExtraData, int nExtraDataLen)
{
    //AudioDecoderContextLib *pAudioDecodeLib  = (AudioDecoderContextLib *)pDecoder;
    astream_fifo_t* stream = pAudioDecodeLib->DecFileInfo.frmFifo;
    cdx_mutex_lock(&pAudioDecodeLib->mutex_audiodec_thread);

    memset(&stream->inFrames[stream->wtIdx],0,sizeof(aframe_info_t));

    stream->inFrames[stream->wtIdx].pts = 0; //TODO convert it to 64bit??
    stream->inFrames[stream->wtIdx].len = nExtraDataLen;
    stream->inFrames[stream->wtIdx].ptsValid = -1;
    stream->wtIdx++;
    if(stream->wtIdx >= AUDIO_BITSTREAM_BUFFER_MAX_FRAME_NUM)
    {
        stream->wtIdx =0;
    }
    stream->ValidchunkCnt++;

    memcpy(pAudioDecodeLib->DecFileInfo.bufWritingPtr, pExtraData, nExtraDataLen);
    pAudioDecodeLib->DecFileInfo.bufWritingPtr += nExtraDataLen;
    pAudioDecodeLib->DecFileInfo.BufValideLen -= nExtraDataLen;
    pAudioDecodeLib->DecFileInfo.BufLen += nExtraDataLen;
    pAudioDecodeLib->DecFileInfo.FileWritingOpsition += nExtraDataLen;

    cdx_mutex_unlock(&pAudioDecodeLib->mutex_audiodec_thread);
    return 0;
}

static const char* CodecIDtoFormatName(enum EAUDIOCODECFORMAT ecodec)
{
    switch(ecodec){
        case AUDIO_CODEC_FORMAT_MP1:
            return "mpeg layer-I";
        case AUDIO_CODEC_FORMAT_MP2:
            return "mpeg layer-II";
        case AUDIO_CODEC_FORMAT_MP3:
        case AUDIO_CODEC_FORMAT_MP3_PRO:
            return "mpeg layer-III";
        case AUDIO_CODEC_FORMAT_MPEG_AAC_LC:
            return "aac low-complexy";
        case AUDIO_CODEC_FORMAT_AC3:
            return "dolby audio";
        case AUDIO_CODEC_FORMAT_DTS:
            return "dts audio";
        case AUDIO_CODEC_FORMAT_LPCM_V:
        case AUDIO_CODEC_FORMAT_LPCM_A:
        case AUDIO_CODEC_FORMAT_ADPCM:
        case AUDIO_CODEC_FORMAT_PCM:
        case AUDIO_CODEC_FORMAT_PPCM:
            return "linear pcm wav audio";
        case AUDIO_CODEC_FORMAT_WMA_PRO:
        case AUDIO_CODEC_FORMAT_WMA_LOSS:
        case AUDIO_CODEC_FORMAT_WMA_STANDARD:
            return "wma audio";
        case AUDIO_CODEC_FORMAT_FLAC:
            return "flac audio";
        case AUDIO_CODEC_FORMAT_APE:
            return "ape audio";
        case AUDIO_CODEC_FORMAT_OGG:
            return "ogg audio";
        case AUDIO_CODEC_FORMAT_RAAC:
            return "raac audio";
        case AUDIO_CODEC_FORMAT_COOK:
            return "cook";
        case AUDIO_CODEC_FORMAT_SIPR:
            return "sipr";
        case AUDIO_CODEC_FORMAT_ATRC:
            return "atrac";
        case AUDIO_CODEC_FORMAT_AMR:
            return "amr audio";
        case AUDIO_CODEC_FORMAT_RA:
            return "ra audio";
        case AUDIO_CODEC_FORMAT_ALAC:
            return "apple lossless";
        case AUDIO_CODEC_FORMAT_DSD:
            return "dsd stream";
        case AUDIO_CODEC_FORMAT_OPUS:
            return "opus";
        default:
            return "unknow format";
    }
    return "null";
}

void AudioInternalCtl(int cmd, void* parm1, void* parm2)
{
    unsigned char *pExtraData;
    int ret;

    switch(cmd)
    {
        case AUDIO_INTERNAL_CMD_GET_APE_FILE_VERSION:
            {
                pExtraData = (unsigned char *)parm1;
                ret = 0;
                ret = pExtraData[1]&0xff;
                ret <<= 8;
                ret |= (pExtraData[0]&0xff);
            }
            memcpy(parm2, &ret, sizeof(int));
            break;
        default:
            alib_logd("Unknow cmd(%d)...", cmd);
            break;
    }
}

static int AC320DecInit(AudioDecoderContextLib *pAudioDecodeLib,BsInFor *pBsInFor, AudioStreamInfo *pAudioStreamInfo)
{
    unsigned char *pExtraData;
    int nExtraDataLen;

#ifdef __AD_CEDAR_DBG_WRITEOUT_BITSTREAM
#if ( __ANDROID__ != 1 )
    pFileBs = fopen("/mnt/hdd_1/ad_cedar_bs.bin","wb");
#else
    pFileBs = fopen("/data/camera/ad_cedar_bs.bin","wb");
#endif
    if(!pFileBs)
    {
        alib_logw("can't open pFileBs\n");
    }
#endif

#ifdef __AD_CEDAR_DBG_WRITEOUT_PCM_BUFFER
#if ( __ANDROID__ != 1 )
    pFilePcm = fopen("/mnt/hdd_1/ad_cedar_pcm.bin","wb");
#else
    pFilePcm = fopen("/data/camera/ad_cedar_pcm.bin","wb");
#endif
    if(!pFilePcm)
    {
        alib_logw("can't open pFileBs\n");
    }
#endif

    pExtraData = (unsigned char*)pAudioStreamInfo->pCodecSpecificData;
    nExtraDataLen  = pAudioStreamInfo->nCodecSpecificDataLen;
    #if 1
    alib_logd("*************pAudioStreamInfo start******************");
    alib_logd("eCodecFormat         :id(%d), name(%s)",   pAudioStreamInfo->eCodecFormat, CodecIDtoFormatName(pAudioStreamInfo->eCodecFormat));
    alib_logd("eSubCodecFormat      :%d",   pAudioStreamInfo->eSubCodecFormat        );
    alib_logd("nChannelNum          :%d",   pAudioStreamInfo->nChannelNum            );
    alib_logd("nBitsPerSample       :%d",   pAudioStreamInfo->nBitsPerSample == 0?16:pAudioStreamInfo->nBitsPerSample);
    alib_logd("nSampleRate          :%d",   pAudioStreamInfo->nSampleRate            );
    alib_logd("nAvgBitrate          :%d",   pAudioStreamInfo->nAvgBitrate            );
    alib_logd("nMaxBitRate          :%d",   pAudioStreamInfo->nMaxBitRate            );
    alib_logd("nFileSize            :%d",   pAudioStreamInfo->nFileSize              );
    alib_logd("eAudioBitstreamSource:%d",   pAudioStreamInfo->eAudioBitstreamSource  );
    alib_logd("eDataEncodeType      :%d",   pAudioStreamInfo->eDataEncodeType        );
    alib_logd("nCodecSpecificDataLen:%d",   pAudioStreamInfo->nCodecSpecificDataLen  );
    alib_logd("pCodecSpecificData   :%p",   pAudioStreamInfo->pCodecSpecificData     );
    alib_logd("nFlags               :%d",   pAudioStreamInfo->nFlags                 );
    alib_logd("nBlockAlign          :%d",   pAudioStreamInfo->nBlockAlign            );
    alib_logd("*************pAudioStreamInfo end  ******************");
    #endif


    pAudioDecodeLib->DecFileInfo.bufStart = (unsigned char *)malloc(AUDIO_BITSTREAM_BUFFER_SIZE);

    if(!pAudioDecodeLib->DecFileInfo.bufStart)
    {
        alib_loge("err bufStart malloc");
        return -1;
    }

    pAudioDecodeLib->DecFileInfo.frmFifo = (astream_fifo_t*)malloc(sizeof(astream_fifo_t));
    if(!pAudioDecodeLib->DecFileInfo.frmFifo)
    {
        alib_loge("err frmFifo malloc");
        return -1;
    }
    memset(pAudioDecodeLib->DecFileInfo.frmFifo, 0x00, sizeof(astream_fifo_t));
    pAudioDecodeLib->DecFileInfo.frmFifo->inFrames = (aframe_info_t *)malloc(AUDIO_BITSTREAM_BUFFER_MAX_FRAME_NUM * sizeof(aframe_info_t));

    if(!pAudioDecodeLib->DecFileInfo.frmFifo->inFrames)
    {
        alib_loge("err inFrames malloc");
        return -1;
    }

    memset(pAudioDecodeLib->DecFileInfo.frmFifo->inFrames, 0, AUDIO_BITSTREAM_BUFFER_MAX_FRAME_NUM * sizeof(aframe_info_t));
    pAudioDecodeLib->DecFileInfo.frmFifo->maxchunkNum = AUDIO_BITSTREAM_BUFFER_MAX_FRAME_NUM;
    pAudioDecodeLib->DecFileInfo.frmFifo->dLastValidPTS = -1;

    pAudioDecodeLib->DecFileInfo.BufToTalLen = AUDIO_BITSTREAM_BUFFER_SIZE;
    pAudioDecodeLib->DecFileInfo.bufWritingPtr = pAudioDecodeLib->DecFileInfo.bufStart;
    pAudioDecodeLib->DecFileInfo.BufValideLen = pAudioDecodeLib->DecFileInfo.BufToTalLen;
    pAudioDecodeLib->DecFileInfo.bufReadingPtr = pAudioDecodeLib->DecFileInfo.bufStart;
    pAudioDecodeLib->DecFileInfo.BufLen = 0;
    pAudioDecodeLib->DecFileInfo.BigENDFlag = 1;
    pAudioDecodeLib->DecFileInfo.FileLength = pAudioStreamInfo->nFileSize;
    if(pAudioStreamInfo->eAudioBitstreamSource == 0x2001)
    {
        pBsInFor->nDecodeMode = CDX_DECODER_MODE_RAWMUSIC;
    }
    if(pBsInFor->nDecodeMode != CDX_DECODER_MODE_RAWMUSIC)
    {
        pAudioDecodeLib->DecFileInfo.FileLength = 0x7fffffff;
        pAudioDecodeLib->pInternal.bWithVideo = 1;//vedio play flag
        if(pAudioDecodeLib->pCedarCodec->flag & AD_CEDAR_CTRL_FLAG_BUILD_RAHDR)
        {
            if(pExtraData && nExtraDataLen > 0)
            {
                SetAudioBsHeader(pAudioDecodeLib, pExtraData, nExtraDataLen);
            }
        }
        else if(pAudioDecodeLib->pCedarCodec->flag & AD_CEDAR_CTRL_FLAG_BUILD_RMVB_SIPR_HDR)
        {
            alib_logd("Sipr add rmvb head");
/*
               Head is something like this...
            cdx_uint32 ulSampleRate;
            cdx_uint32 ulActualRate;
            cdx_uint16 usBitsPerSample;
            cdx_uint16 usNumChannels;
            cdx_uint16 usAudioQuality;
            cdx_uint16 usFlavorIndex;
            cdx_uint32 ulBitsPerFrame;
            cdx_uint32 ulGranularity;
            cdx_uint32 ulOpaqueDataSize;
            cdx_uint8* pOpaqueData;
*/
            if(pExtraData && ( nExtraDataLen > 4 ) && IS_RMVB_INFO_HEADER(pExtraData))
            {
                SetAudioBsHeader(pAudioDecodeLib, pExtraData, nExtraDataLen);
            }
            else
            {//For our sipr lib must meet 32 byte rmvb head like uppon
                unsigned char Extra[32]= {0};
                unsigned char *pExtra = NULL;
                pExtra = &Extra[0];
                pExtra[3] = pExtra[2] = pExtra[1] = pExtra[0] = 0xff;
                pExtra = &Extra[4];
                *((unsigned int*)pExtra) = (unsigned int)pAudioStreamInfo->nSampleRate;
                pExtra = &Extra[8];
                *((unsigned int*)pExtra) = (unsigned int)pAudioStreamInfo->nSampleRate;
                pExtra = &Extra[12];
                *((unsigned short*)pExtra) = (unsigned short)pAudioStreamInfo->nBitsPerSample;
                pExtra = &Extra[14];
                *((unsigned short*)pExtra) = (unsigned short)pAudioStreamInfo->nChannelNum;
                pExtra = &Extra[16];
                *((unsigned short*)pExtra) = 100;
                SetAudioBsHeader(pAudioDecodeLib, Extra, 32);
            }
        }
        else if(pAudioDecodeLib->pCedarCodec->flag & AD_CEDAR_CTRL_FLAG_BUILD_WMAHDR)
        {
            if(pExtraData)
            {
                {
                    unsigned char pExtra[18]= {0};
                    //set 18bytes
                    //0,1,wFormatTag
                    pExtra[ 0]= pAudioStreamInfo->eSubCodecFormat & 0xff;
                    pExtra[ 1]= (pAudioStreamInfo->eSubCodecFormat>>8) & 0xff;
                    //2,3,nChannels
                    pExtra[ 2]= pAudioStreamInfo->nChannelNum & 0xff;
                    pExtra[ 3]= (pAudioStreamInfo->nChannelNum>>8) & 0xff;
                    //4567nSamplesPerSec
                    pExtra[ 4]= pAudioStreamInfo->nSampleRate & 0xff;
                    pExtra[ 5]= (pAudioStreamInfo->nSampleRate>>8) & 0xff;
                    pExtra[ 6]= (pAudioStreamInfo->nSampleRate>>16) & 0xff;
                    pExtra[ 7]= (pAudioStreamInfo->nSampleRate>>24) & 0xff;
                    //891011nAvgBytesPerSec
                    //alib_logd("pAudioStreamInfo->nAvgBitrate:0x%0x",pAudioStreamInfo->nAvgBitrate);
                    pExtra[ 8]= (pAudioStreamInfo->nAvgBitrate>>3)&0xff ;
                    pExtra[ 9]= (pAudioStreamInfo->nAvgBitrate>>11)&0xff ;
                    pExtra[10]= (pAudioStreamInfo->nAvgBitrate>>19)&0xff ;
                    pExtra[11]= (pAudioStreamInfo->nAvgBitrate>>27)&0xff ;
                    //1213nBlockAlign
                    pExtra[12]= pAudioStreamInfo->nBlockAlign & 0xff;
                    pExtra[13]= (pAudioStreamInfo->nBlockAlign>>8) & 0xff;
                    //1415wBitsPerSample
                    pExtra[14]= pAudioStreamInfo->nBitsPerSample & 0xff;
                    pExtra[15]= (pAudioStreamInfo->nBitsPerSample>>8) & 0xff;
                    //extradata_size
                    pExtra[16]= pAudioStreamInfo->nCodecSpecificDataLen & 0xff;
                    pExtra[17]= (pAudioStreamInfo->nCodecSpecificDataLen>>8) & 0xff;
                    SetAudioBsHeader(pAudioDecodeLib, pExtra, 18);
                    //set extradata
                    SetAudioBsHeader(pAudioDecodeLib, pExtraData, nExtraDataLen);
                }
            }
        }
        /*
            Maybe in RMVB, TS, PMP, aac has already headed. The parser need to set ADEC_DISABLE_AAC_PACKING 1 respectly
        */
        else if((pAudioDecodeLib->pCedarCodec->flag & AD_CEDAR_CTRL_FLAG_BUILD_AACHDR) && 
                        !(pAudioDecodeLib->nAudioInfoFlags & ADEC_DISABLE_AAC_PACKING))
        {
            if(pExtraData)
            {
                AdCedarBuildAACPacketHdr(pExtraData, nExtraDataLen, 0, pAudioDecodeLib->Streambuffer.CedarAbsPackHdr, &pAudioDecodeLib->Streambuffer.CedarAbsPackHdrLen, pAudioStreamInfo->nChannelNum, pAudioStreamInfo->nSampleRate);
            }

            //* add by chenxiaochuan. Some media files have packed aac header in stream already. Don't add header for it.
            if(nExtraDataLen < 2)
            {
                pAudioDecodeLib->Streambuffer.CedarAbsPackHdrLen = 0;
            }
            //* end.
        }
        else if(pAudioDecodeLib->pCedarCodec->flag & AD_CEDAR_CTRL_FLAG_BUILD_AMRHDR)
        {
            unsigned char samr_extradata[] = {0x23,0x21,0x41,0x4d,0x52,0x0a};
            unsigned char sawb_extradata[] = {0x23,0x21,0x41,0x4d,0x52,0x2d,0x57,0x42,0x0a};
            if(pAudioStreamInfo->eSubCodecFormat == AMR_FORMAT_NARROWBAND)
            { //TODO make codec format ok
                SetAudioBsHeader(pAudioDecodeLib, samr_extradata, 6);
            }
            else
            {
                SetAudioBsHeader(pAudioDecodeLib, sawb_extradata, 9);
            }
        }
        else if(pAudioDecodeLib->pCedarCodec->flag & AD_CEDAR_CTRL_FLAG_BUILD_PCMHDR)
        {
            if(pExtraData && nExtraDataLen)
            {
                alib_loge("audioCodec[%d-%d], has extraData %d bytes", pAudioStreamInfo->eCodecFormat, pAudioStreamInfo->eSubCodecFormat, nExtraDataLen);
                int uNSamplesPerBlock =  (unsigned int)pExtraData[0]|((unsigned int)pExtraData[1]<<8);
                pAudioStreamInfo->nBlockAlign = (uNSamplesPerBlock * pAudioStreamInfo->nChannelNum - pAudioStreamInfo->nChannelNum)/2 + 4 * pAudioStreamInfo->nChannelNum;
            }
            SetAudioBsHeader_PCM(pAudioStreamInfo,&pAudioDecodeLib->DecFileInfo);
        }
        else if(pAudioDecodeLib->pCedarCodec->flag & AD_CEDAR_CTRL_FLAG_BUILD_APEHDR)
        {
            if(pExtraData)
            {
                unsigned char pExtra[16]= {0};
                memcpy(pExtra,(char*)pExtraData , nExtraDataLen);
                //8,9wBitsPerSample
                pExtra[ 8]= pAudioStreamInfo->nBitsPerSample & 0xff;
                pExtra[ 9]= (pAudioStreamInfo->nBitsPerSample>>8) & 0xff;
                //10,11,nChannels
                pExtra[10]= pAudioStreamInfo->nChannelNum & 0xff;
                pExtra[11]= (pAudioStreamInfo->nChannelNum>>8) & 0xff;
                //12-15 nSamplesPerSec
                pExtra[12]= pAudioStreamInfo->nSampleRate & 0xff;
                pExtra[13]= (pAudioStreamInfo->nSampleRate>>8) & 0xff;
                pExtra[14]= (pAudioStreamInfo->nSampleRate>>16) & 0xff;
                pExtra[15]= (pAudioStreamInfo->nSampleRate>>24) & 0xff;

                SetAudioBsHeader(pAudioDecodeLib, pExtra, 16);
            }
        }
        else if(pAudioDecodeLib->pCedarCodec->flag & AD_CEDAR_CTRL_FLAG_BUILD_COMMONHDR)
        {
            if(pExtraData && nExtraDataLen > 0)
            {
                SetAudioBsHeader(pAudioDecodeLib, pExtraData, nExtraDataLen);
            }
        }
#ifdef __AD_CEDAR_DBG_WRITEOUT_BITSTREAM
        if(pFileBs)
            fwrite(pAudioDecodeLib->DecFileInfo.bufStart,1,pAudioDecodeLib->DecFileInfo.BufLen,pFileBs);
#endif
    }

    pAudioDecodeLib->pCedarAudioDec = pAudioDecodeLib->pCedarCodec->init();

    if(!pAudioDecodeLib->pCedarAudioDec)
    {
        return -1;
    }

    switch(pAudioStreamInfo->eCodecFormat)// dont touch
    {
        case AUDIO_CODEC_FORMAT_AC3:
        case AUDIO_CODEC_FORMAT_DTS:
        case AUDIO_CODEC_FORMAT_WMA_PRO:
        case AUDIO_CODEC_FORMAT_WMA_LOSS:
        case AUDIO_CODEC_FORMAT_WMA_STANDARD:
            if(pAudioDecodeLib->pCedarAudioDec->DecIoCtrl != NULL &&
               pAudioDecodeLib->pCedarAudioDec->DecIoCtrl(pAudioDecodeLib->pCedarAudioDec, 0, 0, NULL) >= 0)
            {
                break;
            }
            else
            {
                alib_logd("DecInit failed.");
                return -1;
            }
        default:
            break;
    }

    if(pAudioStreamInfo->eCodecFormat == AUDIO_CODEC_FORMAT_G726a)
    {
        pBsInFor->g726_enc_law = 1;
        pBsInFor->bitrate = pAudioStreamInfo->nAvgBitrate;
    }
    else if(pAudioStreamInfo->eCodecFormat == AUDIO_CODEC_FORMAT_G726u)
    {
        pBsInFor->g726_enc_law = 0;
        pBsInFor->bitrate = pAudioStreamInfo->nAvgBitrate;
    }
    pBsInFor->firstflag = 0;
    pAudioDecodeLib->pCedarAudioDec->FileReadInfo = &pAudioDecodeLib->DecFileInfo;
    pAudioDecodeLib->pCedarAudioDec->DecoderCommand = &pAudioDecodeLib->pInternal; // info used in decoder internally.
    pAudioDecodeLib->pCedarAudioDec->BsInformation = pBsInFor;

    Register_RW_APIs(pAudioDecodeLib->pCedarAudioDec->FileReadInfo);

    if(pAudioDecodeLib->pCedarAudioDec->DecInit(pAudioDecodeLib->pCedarAudioDec)<0)
    {
        alib_loge("err DecInit");
        return -1;
    }

    pAudioDecodeLib->pCedarAudioDec->DecoderCommand->ulMode= pAudioDecodeLib->pCedarAudioDec->BsInformation->ulMode;
    pAudioDecodeLib->pInternal.ulExitMean = 1;
    return 0;
}

static void AC320DecExit(AudioDecoderContextLib *pAudioDecodeLib)
{
#ifdef __AD_CEDAR_DBG_WRITEOUT_BITSTREAM
    if(pFileBs)
    {
        fclose(pFileBs);
        pFileBs = NULL;
    }
#endif

#ifdef __AD_CEDAR_DBG_WRITEOUT_PCM_BUFFER
    if(pFilePcm)
    {
        fclose(pFilePcm);
        pFilePcm = NULL;
    }
    #endif

    if(pAudioDecodeLib->DecFileInfo.frmFifo != NULL)
    {
        ALIB_SAFE_FREE(pAudioDecodeLib->DecFileInfo.frmFifo->inFrames);
    }
    ALIB_SAFE_FREE(pAudioDecodeLib->DecFileInfo.frmFifo);
    ALIB_SAFE_FREE(pAudioDecodeLib->DecFileInfo.bufStart);

    alib_logv("ad_cedar exit 1...");
    if(pAudioDecodeLib->pCedarAudioDec)
    {
        pAudioDecodeLib->pCedarAudioDec->DecExit(pAudioDecodeLib->pCedarAudioDec);
        pAudioDecodeLib->pCedarCodec->exit(pAudioDecodeLib->pCedarAudioDec);
        pAudioDecodeLib->pCedarAudioDec = NULL;
    }
    alib_logv("ad_cedar exit 2...");
    return;
}

/*********************************************************************************/

int ParseRequestAudioBitstreamBuffer(AudioDecoderLib* pDecoder,
               int           nRequireSize,
               unsigned char**        ppBuf,
               int*          pBufSize,
               unsigned char**        ppRingBuf,
               int*          pRingBufSize,
               int*          nOffset)
{
    //  |-------------------------|-----------------|
    // start                     curr              end
    //                            |--nLengthToEnd--|
    AudioDecoderContextLib *pAudioDecodeLib  = (AudioDecoderContextLib *)pDecoder;
    
    int nLengthToEnd = pAudioDecodeLib->DecFileInfo.bufStart + pAudioDecodeLib->DecFileInfo.BufToTalLen - 
                        pAudioDecodeLib->DecFileInfo.bufWritingPtr;
    int nWriteLenth = nRequireSize;
    astream_fifo_t* stream = pAudioDecodeLib->DecFileInfo.frmFifo;
    //alib_logv("***LINE:%d,FUNC:%s,size:%d,offset:%d",__LINE__,__FUNCTION__,nRequireSize,*nOffset);
    if( nWriteLenth > pAudioDecodeLib->DecFileInfo.BufValideLen || stream->ValidchunkCnt >= AUDIO_BITSTREAM_BUFFER_MAX_FRAME_NUM)
    {
        alib_logv("audio input bs buffer overflow: %d,%d,%d\n",nWriteLenth,pAudioDecodeLib->DecFileInfo.BufValideLen,stream->ValidchunkCnt);
        return -1;//OMX_ErrorOverflow;
    }
    //add for seek
    cdx_mutex_lock(&pAudioDecodeLib->mutex_audiodec_thread);
    if(pAudioDecodeLib->DecFileInfo.updataoffsetflag == 1)
    {
        pAudioDecodeLib->DecFileInfo.FileWritingOpsition = pAudioDecodeLib->DecFileInfo.need_offset;
        pAudioDecodeLib->DecFileInfo.updataoffsetflag = 2;
    }
    cdx_mutex_unlock(&pAudioDecodeLib->mutex_audiodec_thread);
    *nOffset = pAudioDecodeLib->DecFileInfo.FileWritingOpsition;

    if((pAudioDecodeLib->pCedarCodec->flag & AD_CEDAR_CTRL_FLAG_BUILD_AACHDR) && 
        !(pAudioDecodeLib->nAudioInfoFlags & ADEC_DISABLE_AAC_PACKING)) // reserve buffer space to store pkt header
    {
        if(pAudioDecodeLib->Streambuffer.CedarAbsPackHdrLen >= nLengthToEnd)
        { //back-round for pAudioDecData->CedarAbsPackHdr
            pAudioDecodeLib->Streambuffer.InputBsManage.mode = BSFILL_MODE_HDRWRAP;
            pAudioDecodeLib->Streambuffer.InputBsManage.pHoloStartAddr0 = pAudioDecodeLib->DecFileInfo.bufWritingPtr;
            pAudioDecodeLib->Streambuffer.InputBsManage.pHoloStartAddr1 = pAudioDecodeLib->DecFileInfo.bufStart;
            pAudioDecodeLib->Streambuffer.InputBsManage.nHoloLen0 = nLengthToEnd;
            pAudioDecodeLib->Streambuffer.InputBsManage.nHoloLen1 = pAudioDecodeLib->Streambuffer.CedarAbsPackHdrLen - nLengthToEnd;
            *ppBuf = pAudioDecodeLib->DecFileInfo.bufStart + pAudioDecodeLib->Streambuffer.CedarAbsPackHdrLen - nLengthToEnd;
            *pBufSize = nRequireSize;
            *ppRingBuf = NULL;
            *pRingBufSize = 0;
            alib_logv("audio bs hdr wrap nHoloLen0:%d nHoloLen1:%d addr0:%p addr1:%p pBuffer:%p",pAudioDecodeLib->Streambuffer.InputBsManage.nHoloLen0,
            pAudioDecodeLib->Streambuffer.InputBsManage.nHoloLen1,pAudioDecodeLib->Streambuffer.InputBsManage.pHoloStartAddr0,
            pAudioDecodeLib->Streambuffer.InputBsManage.pHoloStartAddr1,*ppBuf);
        }
        else
        {
            int nLengthToEnd2;
            pAudioDecodeLib->Streambuffer.InputBsManage.mode = BSFILL_MODE_BSWRAP;
            pAudioDecodeLib->Streambuffer.InputBsManage.pHoloStartAddr0 = pAudioDecodeLib->DecFileInfo.bufWritingPtr;
            pAudioDecodeLib->Streambuffer.InputBsManage.pHoloStartAddr1 = NULL;
            pAudioDecodeLib->Streambuffer.InputBsManage.nHoloLen0 = pAudioDecodeLib->Streambuffer.CedarAbsPackHdrLen;
            pAudioDecodeLib->Streambuffer.InputBsManage.nHoloLen1 = 0;
            *ppBuf = pAudioDecodeLib->DecFileInfo.bufWritingPtr + pAudioDecodeLib->Streambuffer.CedarAbsPackHdrLen; //holo for update fill
            nLengthToEnd2 = pAudioDecodeLib->DecFileInfo.bufStart + pAudioDecodeLib->DecFileInfo.BufToTalLen - 
                (unsigned char*)(*ppBuf);
            //yuanfangif(pBuffer->nTobeFillLen >= nLengthToEnd){ //back-round for streams
            if(nRequireSize >= nLengthToEnd2)
            { //back-round for streams
                *pBufSize = nLengthToEnd2;
                *ppRingBuf = pAudioDecodeLib->DecFileInfo.bufStart;
                *pRingBufSize = nRequireSize - nLengthToEnd2;
            }
            else
            { //normal case
                *pBufSize = nRequireSize;
                *ppRingBuf = NULL;
                *pRingBufSize = 0;
            }
        }
    }
    else
    {
        pAudioDecodeLib->Streambuffer.InputBsManage.mode = BSFILL_MODE_NORMAL;
        *ppBuf = pAudioDecodeLib->DecFileInfo.bufWritingPtr;
        if (nRequireSize >= nLengthToEnd)
        { //back-round for streams
            *pBufSize = nLengthToEnd;
            *ppRingBuf = pAudioDecodeLib->DecFileInfo.bufStart;
            *pRingBufSize = nRequireSize - nLengthToEnd;
        }
        else
        { //normal case
            *pBufSize = nRequireSize;
            *ppRingBuf = NULL;
            *pRingBufSize = 0;
        }
        //alib_logv("ad_cedar_req pBuffer->pBuffer:%p TobeFillLen:%d",pBuffer->pBuffer,pBuffer->nTobeFillLen);
    }
    return 0;//OMX_ErrorNone;
}


int ParseUpdateAudioBitstreamData(AudioDecoderLib* pDecoder,
                                          int nFilledLen,
                                          int64_t nTimeStamp,
                                          int nOffset)
{
    AudioDecoderContextLib *pAudioDecodeLib  = (AudioDecoderContextLib *)pDecoder;
    astream_fifo_t* stream = pAudioDecodeLib->DecFileInfo.frmFifo;
    int nWriteLenth = nFilledLen;
    int eError = 0;//OMX_ErrorNone;
    if(nFilledLen<=0)//maybe unvalid packet;
    {
        return eError;
    }

    cdx_mutex_lock(&pAudioDecodeLib->mutex_audiodec_thread);
    if(pAudioDecodeLib->DecFileInfo.updataoffsetflag == 1)
    {
        cdx_mutex_unlock(&pAudioDecodeLib->mutex_audiodec_thread);
        //sync witch decode driver main task, need send data again
        return eError;
    }
    if(pAudioDecodeLib->DecFileInfo.updataoffsetflag == 2)
    {
        if(nOffset != (int)pAudioDecodeLib->DecFileInfo.need_offset)
        {
            pAudioDecodeLib->DecFileInfo.updataoffsetflag = 1;
            cdx_mutex_unlock(&pAudioDecodeLib->mutex_audiodec_thread);
            return 0;
        }
        else
        {
            pAudioDecodeLib->DecFileInfo.updataoffsetflag = 0;
        }
    }

    if(pAudioDecodeLib->Streambuffer.InputBsManage.mode == BSFILL_MODE_BSWRAP)
    {
        AdCedarUpdateAACPacketHdr(pAudioDecodeLib->Streambuffer.CedarAbsPackHdr, pAudioDecodeLib->Streambuffer.CedarAbsPackHdrLen, nFilledLen);
        memcpy(pAudioDecodeLib->Streambuffer.InputBsManage.pHoloStartAddr0,pAudioDecodeLib->Streambuffer.CedarAbsPackHdr,pAudioDecodeLib->Streambuffer.InputBsManage.nHoloLen0);
        //alib_logw("audio bs wrap BSFILL_MODE_BSWRAP");
    }
    else if(pAudioDecodeLib->Streambuffer.InputBsManage.mode == BSFILL_MODE_HDRWRAP)
    {
        AdCedarUpdateAACPacketHdr(pAudioDecodeLib->Streambuffer.CedarAbsPackHdr, pAudioDecodeLib->Streambuffer.CedarAbsPackHdrLen, nFilledLen);
        //alib_logw("audio bs wrap BSFILL_MODE_HDRWRAP");
        memcpy(pAudioDecodeLib->Streambuffer.InputBsManage.pHoloStartAddr0,pAudioDecodeLib->Streambuffer.CedarAbsPackHdr,pAudioDecodeLib->Streambuffer.InputBsManage.nHoloLen0);
        memcpy(pAudioDecodeLib->Streambuffer.InputBsManage.pHoloStartAddr1,pAudioDecodeLib->Streambuffer.CedarAbsPackHdr + pAudioDecodeLib->Streambuffer.InputBsManage.nHoloLen0,pAudioDecodeLib->Streambuffer.InputBsManage.nHoloLen1);
    }
    //------------- update write information --------------
    nWriteLenth = nFilledLen + pAudioDecodeLib->Streambuffer.CedarAbsPackHdrLen;

    if(pAudioDecodeLib->DecFileInfo.bufWritingPtr + nWriteLenth >= 
        pAudioDecodeLib->DecFileInfo.bufStart + pAudioDecodeLib->DecFileInfo.BufToTalLen)
    {
        int nBufferLen = pAudioDecodeLib->DecFileInfo.bufStart + pAudioDecodeLib->DecFileInfo.BufToTalLen - 
                                                                    pAudioDecodeLib->DecFileInfo.bufWritingPtr;

#ifdef __AD_CEDAR_DBG_WRITEOUT_BITSTREAM
        if(pFileBs)
        {
            fwrite(pAudioDecodeLib->DecFileInfo.bufWritingPtr,1,nBufferLen,pFileBs);
            fwrite(pAudioDecodeLib->DecFileInfo.bufStart,1,nWriteLenth - nBufferLen,pFileBs);
        }
#endif
        pAudioDecodeLib->DecFileInfo.bufWritingPtr = pAudioDecodeLib->DecFileInfo.bufWritingPtr + nWriteLenth - 
                                                        pAudioDecodeLib->DecFileInfo.BufToTalLen;
    }
    else
    {
#ifdef __AD_CEDAR_DBG_WRITEOUT_BITSTREAM


        if(pFileBs){
#if 0
            char ckhs[4] = {'c', 'k', 'h', 's'};
            uint32_t len = nWriteLenth;
            fwrite(ckhs, 1, 4, pFileBs);
            fwrite(&len, 1, 4, pFileBs);
#endif
            fwrite(pAudioDecodeLib->DecFileInfo.bufWritingPtr,1,nWriteLenth,pFileBs);
            sync();
        }
#endif
        pAudioDecodeLib->DecFileInfo.bufWritingPtr += nWriteLenth;
    }

    pAudioDecodeLib->DecFileInfo.FileWritingOpsition += nWriteLenth;//fwrite file
    pAudioDecodeLib->DecFileInfo.BufLen += nWriteLenth;
    pAudioDecodeLib->DecFileInfo.BufValideLen -= nWriteLenth;
    //---------------------------------------------------------------
    memset(&stream->inFrames[stream->wtIdx],0,sizeof(aframe_info_t));
    //stream->inFrames[stream->wtIdx].startAddr = ;
    stream->inFrames[stream->wtIdx].pts = nTimeStamp ; //TODO convert it to 64bit??
    stream->inFrames[stream->wtIdx].len = nFilledLen + pAudioDecodeLib->Streambuffer.CedarAbsPackHdrLen;
    stream->inFrames[stream->wtIdx].ptsValid = (nTimeStamp != -1);

    if(stream->inFrames[stream->wtIdx].ptsValid)
    {
        if(stream->dLastValidPTS != nTimeStamp)
        {
           stream->dLastValidPTS = nTimeStamp;
        }
        else
        {
           stream->inFrames[stream->wtIdx].ptsValid  = 0;
        }
    }
    alib_logv("****parse pts:%lld,len:%d,BufLen:%d,wtIdx:%d",stream->inFrames[stream->wtIdx].pts,stream->inFrames[stream->wtIdx].len,pAudioDecodeLib->DecFileInfo.BufLen,stream->wtIdx);
    stream->wtIdx++;
    if(stream->wtIdx >= AUDIO_BITSTREAM_BUFFER_MAX_FRAME_NUM)
    {
        stream->wtIdx =0;
    }
    stream->ValidchunkCnt++;
    cdx_mutex_unlock(&pAudioDecodeLib->mutex_audiodec_thread);
    return eError;
}

int ParseAudioStreamDataSize(AudioDecoderLib* pDecoder)
{
    AudioDecoderContextLib *pAudioDecodeLib  = (AudioDecoderContextLib *)pDecoder;
    return pAudioDecodeLib->DecFileInfo.BufLen;
}


void BitstreamQueryQuality(AudioDecoderLib* pDecoder, int* pValidPercent, int* vbv)
{
    AudioDecoderContextLib *pAudioDecodeLib  = (AudioDecoderContextLib *)pDecoder;
    cdx_mutex_lock(&pAudioDecodeLib->mutex_audiodec_thread);
    *pValidPercent = pAudioDecodeLib->DecFileInfo.BufLen * 100 / pAudioDecodeLib->DecFileInfo.BufToTalLen;
    *vbv = pAudioDecodeLib->DecFileInfo.frmFifo->ValidchunkCnt;
    cdx_mutex_unlock(&pAudioDecodeLib->mutex_audiodec_thread);
    return;
}
void ParseBitstreamSeekSync(AudioDecoderLib* pDecoder, int64_t nSeekTime, int nGetAudioInfoFlag)
{
    AudioDecoderContextLib *pAudioDecodeLib        = (AudioDecoderContextLib *)pDecoder;
    if(!pAudioDecodeLib->pCedarAudioDec->Decinitedflag)
    {
        return ;//if no init finish,no seek flush extradata;
    }
#ifdef __AD_CEDAR_DBG_WRITEOUT_BITSTREAM
    if(pFileBs)
    {
        fclose(pFileBs);
        pFileBs = NULL;
    }
    pFileBs = fopen("/data/camera/ad_cedar_bs.bin","wb");
    if(!pFileBs)
    {
        alib_logw("can't open pFileBs\n");
    }
#endif
    if(pAudioDecodeLib->pCedarAudioDec->BsInformation->nDecodeMode != CDX_DECODER_MODE_RAWMUSIC)
    {
        astream_fifo_t* stream                     = pAudioDecodeLib->DecFileInfo.frmFifo;
        alib_logd("ad_cedar seek sync ......nSeekTime:%lld",nSeekTime);
        //sync with input buffer
        pAudioDecodeLib->DecFileInfo.bufWritingPtr = pAudioDecodeLib->DecFileInfo.bufStart;
        pAudioDecodeLib->DecFileInfo.BufValideLen  = pAudioDecodeLib->DecFileInfo.BufToTalLen;
        pAudioDecodeLib->DecFileInfo.bufReadingPtr = pAudioDecodeLib->DecFileInfo.bufStart;
        pAudioDecodeLib->DecFileInfo.BufLen        = 0;

        //sync with pts control
        stream->wtIdx = stream->rdIdx              = 0;
        stream->ValidchunkCnt                      = 0;
        stream->dLastValidPTS             = -1;
        pAudioDecodeLib->pInternal.ulFFREVFlag     = 0x88;
        pAudioDecodeLib->pInternal.ulFFREVStep     = nSeekTime / 1000000;
    }
    else
    {
        int64_t dSeekRelativeTime                  = nSeekTime - pAudioDecodeLib->pCedarAudioDec->BsInformation->NowPTSTime;
        pAudioDecodeLib->pInternal.ulFFREVFlag     = dSeekRelativeTime >= 0 ? 0x44 : 0x66;
        pAudioDecodeLib->pInternal.ulFFREVStep     = llabs(dSeekRelativeTime) / 1000000;
        alib_logd("seek relative time: %lld", dSeekRelativeTime);
    }

    if(nGetAudioInfoFlag)
    {
        pAudioDecodeLib->pInternal.ulFFREVFlag     = 0x00;
    }
    pAudioDecodeLib->pInternal.ulExitMean = 1;
}
int InitializeAudioDecodeLib(AudioDecoderLib*    pDecoder,
                           AudioStreamInfo* pAudioStreamInfo,
                           BsInFor *pBsInFor)
{
    int ret = 0;
    int idx = 0;

    LogVersionInfo();

    AudioDecoderContextLib *pAudioDecodeLib            = (AudioDecoderContextLib *)pDecoder;
    pAudioDecodeLib->DecFileInfo.tmpGlobalAudioDecData = (void*)pDecoder;
    pAudioDecodeLib->nAudioInfoFlags                  = pAudioStreamInfo->nFlags;
    printf("**** InitializeAudioDecodeLib codec_tpye:%d ****\n",pAudioStreamInfo->eCodecFormat);

    switch(pAudioStreamInfo->eCodecFormat)
    {
//        case AUDIO_CODEC_FORMAT_WMA_STANDARD:
//        case AUDIO_CODEC_FORMAT_WMA_LOSS:
//            ret = InitCodec(pAudioDecodeLib, InitCodesT[AUDIO_CODEC_INDEX_WMA]);
//            if(ret < 0)
//            {
//                ExitCodec(pAudioDecodeLib);
//                return -1;
//            }
//            break;
        case AUDIO_CODEC_FORMAT_MPEG_AAC_LATM:
        case AUDIO_CODEC_FORMAT_MPEG_AAC_LC:
        case AUDIO_CODEC_FORMAT_RAAC:
            if(AUDIO_CODEC_FORMAT_MPEG_AAC_LATM == pAudioStreamInfo->eCodecFormat)
            {
                pAudioDecodeLib->pInternal.n_foramet = 4;//AAC_FF_LATM
            }
            ret = InitCodec(pAudioDecodeLib, InitCodesT[AUDIO_CODEC_INDEX_AAC]);
            if(ret < 0)
            {
                ExitCodec(pAudioDecodeLib);
                return -1;
            }
            break;
        case AUDIO_CODEC_FORMAT_RA:
            ret = InitCodec(pAudioDecodeLib, InitCodesT[AUDIO_CODEC_INDEX_RA]);
            if(ret < 0)
            {
                ExitCodec(pAudioDecodeLib);
                return -1;
            }
            break;
        case AUDIO_CODEC_FORMAT_G729:
            ret = InitCodec(pAudioDecodeLib, InitCodesT[AUDIO_CODEC_INDEX_G729]);
            if(ret < 0)
            {
                ExitCodec(pAudioDecodeLib);
                return -1;
            }
            break;
        case AUDIO_CODEC_FORMAT_MP1:
        case AUDIO_CODEC_FORMAT_MP2:
        case AUDIO_CODEC_FORMAT_MP3:
            ret = InitCodec(pAudioDecodeLib, InitCodesT[AUDIO_CODEC_INDEX_MP3]);
            if(ret < 0)
            {
                ExitCodec(pAudioDecodeLib);
                return -1;
            }
            break;
        case AUDIO_CODEC_FORMAT_DSD:
            ret = InitCodec(pAudioDecodeLib, InitCodesT[AUDIO_CODEC_INDEX_DSD]);
            if(ret < 0)
            {
                ExitCodec(pAudioDecodeLib);
                return -1;
            }
            break;
        case AUDIO_CODEC_FORMAT_OPUS:
            ret = InitCodec(pAudioDecodeLib, InitCodesT[AUDIO_CODEC_INDEX_OPUS]);
            if(ret < 0)
            {
                ExitCodec(pAudioDecodeLib);
                return -1;
            }
            break;
        case AUDIO_CODEC_FORMAT_FLAC:
            ret = InitCodec(pAudioDecodeLib, InitCodesT[AUDIO_CODEC_INDEX_FLAC]);
            if(ret < 0)
            {
                ExitCodec(pAudioDecodeLib);
                return -1;
            }
            break;
        case AUDIO_CODEC_FORMAT_ALAC:
            ret = InitCodec(pAudioDecodeLib, InitCodesT[AUDIO_CODEC_INDEX_ALAC]);
            if(ret < 0)
            {
                ExitCodec(pAudioDecodeLib);
                return -1;
            }
            break;
        case AUDIO_CODEC_FORMAT_APE:
            ret = InitCodec(pAudioDecodeLib, InitCodesT[AUDIO_CODEC_INDEX_APE]);
            if(ret < 0)
            {
                ExitCodec(pAudioDecodeLib);
                return -1;
            }
            break;
        case AUDIO_CODEC_FORMAT_OGG:
            ret = InitCodec(pAudioDecodeLib, InitCodesT[AUDIO_CODEC_INDEX_OGG]);
            if(ret < 0)
            {
                ExitCodec(pAudioDecodeLib);
                return -1;
            }
            break;
        case AUDIO_CODEC_FORMAT_COOK:
            ret = InitCodec(pAudioDecodeLib, InitCodesT[AUDIO_CODEC_INDEX_COOK]);
            if(ret < 0)
            {
                ExitCodec(pAudioDecodeLib);
                return -1;
            }
            break;
        case AUDIO_CODEC_FORMAT_SIPR:
            ret = InitCodec(pAudioDecodeLib, InitCodesT[AUDIO_CODEC_INDEX_SIPR]);
            if(ret < 0)
            {
                ExitCodec(pAudioDecodeLib);
                return -1;
            }
            break;
        case AUDIO_CODEC_FORMAT_ATRC:
            ret = InitCodec(pAudioDecodeLib, InitCodesT[AUDIO_CODEC_INDEX_ATRC]);
            if(ret < 0)
            {
                ExitCodec(pAudioDecodeLib);
                return -1;
            }
            break;
        case AUDIO_CODEC_FORMAT_AMR:
            ret = InitCodec(pAudioDecodeLib, InitCodesT[AUDIO_CODEC_INDEX_AMR]);
            if(ret < 0)
            {
                ExitCodec(pAudioDecodeLib);
                return -1;
            }
            break;
        case AUDIO_CODEC_FORMAT_LPCM_V:
        case AUDIO_CODEC_FORMAT_LPCM_A:
        case AUDIO_CODEC_FORMAT_ADPCM:
        case AUDIO_CODEC_FORMAT_PCM:
        case AUDIO_CODEC_FORMAT_G711a:
        case AUDIO_CODEC_FORMAT_G711u:
            ret = InitCodec(pAudioDecodeLib, InitCodesT[AUDIO_CODEC_INDEX_WAV]);
            if(ret < 0)
            {
                ExitCodec(pAudioDecodeLib);
                return -1;
            }
            break;
//        case AUDIO_CODEC_FORMAT_G711a:
//        {
//            ret = InitCodec(pAudioDecodeLib, InitCodesT[AUDIO_CODEC_INDEX_G711a]);
//            if(ret < 0)
//            {
//                ExitCodec(pAudioDecodeLib);
//                return -1;
//            }
//            break;
//        }
//        case AUDIO_CODEC_FORMAT_G711u:
//        {
//            ret = InitCodec(pAudioDecodeLib, InitCodesT[AUDIO_CODEC_INDEX_G711u]);
//            if(ret < 0)
//            {
//                ExitCodec(pAudioDecodeLib);
//                return -1;
//            }
//            break;
//        }
        case AUDIO_CODEC_FORMAT_G726a:
        case AUDIO_CODEC_FORMAT_G726u:
        {
            ret = InitCodec(pAudioDecodeLib, InitCodesT[AUDIO_CODEC_INDEX_G726]);
            if(ret < 0)
            {
                ExitCodec(pAudioDecodeLib);
                return -1;
            }
            break;
        }
            
        default:
            return -1;
    }

    if(AC320DecInit(pAudioDecodeLib,pBsInFor,pAudioStreamInfo)<0)
    {
        alib_logw("cedar audio dec init error!");
        return -1;
    }
    return 0;
}

int DecodeAudioFrame(AudioDecoderLib* pDecoder,
                      char*        ppBuf,
                      int*          pBufSize)
{
    AudioDecoderContextLib *pAudioDecodeLib  = (AudioDecoderContextLib *)pDecoder;
    int ret = 0;

    ret = pAudioDecodeLib->pCedarAudioDec->DecFrame(pAudioDecodeLib->pCedarAudioDec, (alib_int8*)ppBuf, pBufSize);
#ifdef __AD_CEDAR_DBG_WRITEOUT_PCM_BUFFER
    if(pFilePcm)
       fwrite(ppBuf,1,*pBufSize,pFilePcm);
#endif
    if(pAudioDecodeLib->pInternal.ulFFREVFlag == 0x88)
    {
        alib_logv("pAudioDecodeLib->pInternal.ulFFREVFlag 0x88");
    }
    pAudioDecodeLib->pInternal.ulFFREVFlag = 0x00;
    return ret;
}
int DestroyAudioDecodeLib(AudioDecoderLib* pDecoder)
{
    AudioDecoderContextLib *pAudioDecodeLib  = (AudioDecoderContextLib *)pDecoder;
    if(!pAudioDecodeLib)
    {
        return -1;
    }
    AC320DecExit(pAudioDecodeLib);
    ExitCodec(pAudioDecodeLib);
    pthread_mutex_destroy(&pAudioDecodeLib->mutex_audiodec_thread);
    free(pAudioDecodeLib);
    pAudioDecodeLib = 0;
    return 0;
}

void SetAudiolibRawParam(AudioDecoderLib* pDecoder, int commond)
{
    AudioDecoderContextLib *pAudioDecodeLib  = (AudioDecoderContextLib *)pDecoder;
    pAudioDecodeLib->pCedarAudioDec->DecoderCommand->ulMode = commond;
}

void SetAudioDecEOS(AudioDecoderLib* pDecoder)
{
    AudioDecoderContextLib *pAudioDecodeLib  = (AudioDecoderContextLib *)pDecoder;
    pAudioDecodeLib->pInternal.ulExitMean = 0;
}

AudioDecoderLib* CreateAudioDecodeLib(void)
{
    AudioDecoderContextLib *pAudioDecodeLib = malloc(sizeof(AudioDecoderContextLib));
    if(!pAudioDecodeLib)
    {
        return 0;
    }
    memset(pAudioDecodeLib,0,sizeof(AudioDecoderContextLib));

    pthread_mutex_init(&pAudioDecodeLib->mutex_audiodec_thread, NULL);

    return (AudioDecoderLib*)pAudioDecodeLib;
}

#ifdef __cplusplus
}
#endif /* __cplusplus */
