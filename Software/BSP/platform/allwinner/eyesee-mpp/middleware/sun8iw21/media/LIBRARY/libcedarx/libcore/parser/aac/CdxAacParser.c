/*
* Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : CdxAacParser.c
* Description : AAC Parser
* History :
*
*/
#include <CdxTypes.h>
#include <CdxParser.h>
#include <CdxStream.h>
#include <CdxMemory.h>
#include <CdxAacParser.h>

#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "aac_parser"
#endif

#define LATM_SIZE_MASK 0x001FFF
/**************************************************************************************
 * Function:    SetBitstreamPointer
 *
 * Description: initialize bitstream reader
 *
 * Inputs:      pointer to BitStreamInfo struct
 *              number of bytes in bitstream
 *              pointer to byte-aligned buffer of data to read from
 *
 * Outputs:     initialized bitstream info struct
 *
 * Return:      none
 **************************************************************************************/
static void SetBitstreamPointer(BitStreamInfo *pBsi, int nBytes, unsigned char *pBuf)
{
   /* init bitstream */
   pBsi->pBytes = pBuf;
   pBsi->nCache = 0;   /* 4-byte unsigned int */
   pBsi->nCachedBits = 0; /* i.e. zero bits in cache */
   pBsi->nBytes = nBytes;
}

/**************************************************************************************
 * Function:    RefillBitstreamCache
 *
 * Description: read new data from bitstream buffer into 32-bit cache
 *
 * Inputs:      pointer to initialized BitStreamInfo struct
 *
 * Outputs:     updated bitstream info struct
 *
 * Return:      none
 *
 * Notes:       only call when nCache is completely drained (resets bitOffset to 0)
 *              always loads 4 new bytes except when pBsi->nBytes < 4 (end of buffer)
 *              stores data as big-endian in cache, regardless of machine endian-ness
 **************************************************************************************/
static __inline void RefillBitstreamCache(BitStreamInfo *pBsi)
{
   int nBytes = pBsi->nBytes;

   /* optimize for common case, independent of machine endian-ness */
   if (nBytes >= 4)
   {
        pBsi->nCache  = (*pBsi->pBytes++) << 24;
        pBsi->nCache |= (*pBsi->pBytes++) << 16;
        pBsi->nCache |= (*pBsi->pBytes++) <<  8;
        pBsi->nCache |= (*pBsi->pBytes++);
        pBsi->nCachedBits = 32;
        pBsi->nBytes -= 4;
    }
    else
    {
        pBsi->nCache = 0;
        while (nBytes--)
        {
            pBsi->nCache |= (*pBsi->pBytes++);
            pBsi->nCache <<= 8;
        }
        pBsi->nCache <<= ((3 - pBsi->nBytes)*8);
        pBsi->nCachedBits = 8*pBsi->nBytes;
        pBsi->nBytes = 0;
   }
}
/**************************************************************************************
 * Function:    AacGetBits
 *
 * Description: get bits from bitstream, advance bitstream pointer
 *
 * Inputs:      pointer to initialized BitStreamInfo struct
 *              number of bits to get from bitstream
 *
 * Outputs:     updated bitstream info struct
 *
 * Return:      the next nBits bits of data from bitstream buffer
 *
 * Notes:       nBits must be in range [0, 31], nBits outside this range masked by 0x1f
 *              for speed, does not indicate error if you overrun bit buffer
 *              if nBits == 0, returns 0
 **************************************************************************************/

 static unsigned int AacGetBits(BitStreamInfo *pBsi, int nBits)
{
    unsigned int data, lowBits;

    /* nBits mod 32 to avoid unpredictable results like >> by negative amount */
    nBits &= 0x1f;
    data = pBsi->nCache >> (31 - nBits);   /* unsigned >> so zero-extend */
    data >>= 1;               /* do as >> 31, >> 1 so that nBits = 0 works okay (returns 0) */
    pBsi->nCache <<= nBits;          /* left-justify cache */
    pBsi->nCachedBits -= nBits;       /* how many bits have we drawn from the cache so far */

    /* if we cross an int boundary, refill the cache */
    if (pBsi->nCachedBits < 0) {
        lowBits = -pBsi->nCachedBits;
        RefillBitstreamCache(pBsi);
        data |= pBsi->nCache >> (32 - lowBits);    /* get the low-order bits */

        pBsi->nCachedBits -= lowBits;     /* how many bits have we drawn from the cache so far */
        pBsi->nCache <<= lowBits;      /* left-justify cache */
    }

    return data;
}
/**************************************************************************************
 * Function:    AACFindSyncWord
 *
 * Description: locate the next byte-alinged sync word in the raw AAC stream
 *
 * Inputs:      buffer to search for sync word
 *              max number of bytes to search in buffer
 *
 * Outputs:     none
 *
 * Return:      impl->uFirstFrmOffset to first sync word (bytes from start of pBuf)
 *              -1 if sync not found after searching nBytes
 **************************************************************************************/

static int AACFindSyncWord(unsigned char *pBuf, int nBytes)
{
    int i;

    /* find byte-aligned syncword (12 bits = 0xFFF) */
    for (i = 0; i < nBytes - 1; i++)
    {
        if ( (pBuf[i+0] & AAC_SYNCWORDH) == AAC_SYNCWORDH
                && (pBuf[i+1] & AAC_SYNCWORDL) == AAC_SYNCWORDL )
            return i;
    }

    return -1;
}

static int AACFindSyncWord_LATM(unsigned char *pBuf, int nBytes)
{
    int i;

    /* find byte-aligned syncword (12 bits = 0xFFF) */
    for (i = 0; i < nBytes - 1; i++) {
        if ( (pBuf[i+0] & AAC_SYNCWORDH) == AAC_SYNCWORDL_H
                && (pBuf[i+1] & AAC_SYNCWORDL_LATM) == AAC_SYNCWORDL_LATM )
            return i;
    }

    return -1;
}

static int AACFindSyncWord_before(unsigned char *pBuf, int nBytes)
{
    int i;

    /* find byte-aligned syncword (12 bits = 0xFFF) */
    for (i = nBytes-2; i > 0; i--) {
        if ( (pBuf[i+0] & AAC_SYNCWORDH) == AAC_SYNCWORDH
                && (pBuf[i+1] & AAC_SYNCWORDL) == AAC_SYNCWORDL )
            return i;
    }

    return -1;
}

static int AACFindSyncWord_before_LATM(unsigned char *pBuf, int nBytes)
{
    int i;

    /* find byte-aligned syncword (12 bits = 0xFFF) */
    for (i = nBytes-2; i > 0; i--) {
        if ( (pBuf[i+0] & AAC_SYNCWORDH) == AAC_SYNCWORDL_H
                && (pBuf[i+1] & AAC_SYNCWORDL_LATM) == AAC_SYNCWORDL_LATM )
            return i;
    }

    return -1;
}

static int FillReadBuffer(AacParserImplS *impl, unsigned char *readBuf,
                          unsigned char *readPtr, int bufSize, int bytesLeft)
{
    int nRead;

    /* move last, small chunk from end of buffer to start, then fill with new data */
    memmove(readBuf, readPtr, bytesLeft);
    impl->dFileOffset += readPtr - readBuf;
    //nRead = fread(readBuf + bytesLeft, 1, bufSize - bytesLeft, infile);
    nRead = CdxStreamRead(impl->stream, (void *)(readBuf + bytesLeft),bufSize - bytesLeft);
    /* zero-pad to avoid finding false sync word after last frame (from old data in readBuf) */
    if (nRead < bufSize - bytesLeft)
        memset(readBuf + bytesLeft + nRead, 0, bufSize - bytesLeft - nRead);

    return nRead;
}

static void ResetReadBuffer(AacParserImplS *impl)
{
    impl->bytesLeft = 0;
    impl->readPtr = impl->readBuf;
    impl->dFileOffset = impl->uFirstFrmOffset;
    memset(impl->readBuf, 0x00, 2*1024*6*6);
}

static int GetBeforeFrame(AacParserImplS *impl )
{
    int retVal = -1;
    ADTSHeaderT            fhADTS;
    if((impl->readPtr-impl->readBuf)>8)
    {
        impl->readPtr -=7;
        impl->bytesLeft +=7;
    }
    else
    {
        int ret = impl->readPtr-impl->readBuf+impl->bytesLeft+2* 1024;
        if(ret>impl->dFileOffset-impl->uFirstFrmOffset)
        {
            ret = impl->dFileOffset - impl->uFirstFrmOffset
                    - (impl->readPtr-impl->readBuf+impl->bytesLeft);
            if(ret<=0)
                return 0;
            if(CdxStreamSeek(impl->stream,impl->uFirstFrmOffset, SEEK_SET))
            {
                return -1;
            }
            retVal = CdxStreamRead(impl->stream, (void *)(impl->readBuf), ret);
            if(retVal != ret)
            {
                return -1;
            }
            impl->bytesLeft = 0;
            impl->readPtr = impl->readBuf + ret;
            impl->dFileOffset = impl->uFirstFrmOffset;
        }
        else
        {
            if(CdxStreamSeek(impl->stream,
                -(int)(impl->readPtr-impl->readBuf+impl->bytesLeft+2* 1024), SEEK_CUR))
            {
                return -1;
            }

            retVal = CdxStreamRead(impl->stream, (void *)(impl->readBuf), 1024 * 2);
            if(retVal != 1024*2)
            {
                return -1;
            }
            impl->bytesLeft = 0;
            impl->readPtr = impl->readBuf + 2*1024;
            impl->dFileOffset -= 2*1024;
        }
    }
    while(retVal==-1)
    {
        if(impl->ulformat == AAC_FF_ADTS)
        {
            retVal = AACFindSyncWord_before(impl->readBuf,(int)(impl->readPtr-impl->readBuf));
            if(retVal==-1)
            {
                int ret = impl->readPtr-impl->readBuf+impl->bytesLeft+2* 1024;
                if(ret>impl->dFileOffset-impl->uFirstFrmOffset)
                {
                    ret = impl->dFileOffset-impl->uFirstFrmOffset
                            - (impl->readPtr-impl->readBuf+impl->bytesLeft);
                    if(ret<=0)
                        return 0;
                    if(CdxStreamSeek(impl->stream,impl->uFirstFrmOffset, SEEK_SET))
                    {
                        return -1;
                    }
                    retVal = CdxStreamRead(impl->stream, (void *)(impl->readBuf), ret);
                    if(retVal != ret)
                    {
                        return -1;
                    }
                    impl->bytesLeft = 0;
                    impl->readPtr = impl->readBuf + ret;
                    impl->dFileOffset = impl->uFirstFrmOffset;
                }
                else
                {
                    if(CdxStreamSeek(impl->stream,
                            -(int)(impl->readPtr-impl->readBuf+impl->bytesLeft+2* 1024), SEEK_CUR))
                    {
                        return -1;
                    }

                    retVal = CdxStreamRead(impl->stream, (void *)(impl->readBuf), 1024 * 2);
                    if(retVal != 1024*2)
                    {
                        return -1;
                    }
                    impl->bytesLeft = 0;
                    impl->readPtr = impl->readBuf + 2*1024;
                    impl->dFileOffset -= 2*1024;
                }
            }
            else
            {
                impl->bytesLeft += impl->readPtr - impl->readBuf - retVal;
                impl->readPtr = impl->readBuf + retVal;

                fhADTS.ucLayer =            (impl->readPtr[1]>>1)&0x03;//AacGetBits(&pBsi, 2);
                //fhADTS.ucProtectBit =       AacGetBits(&pBsi, 1);
                fhADTS.ucProfile =          (impl->readPtr[2]>>6)&0x03;//AacGetBits(&pBsi, 2);
                fhADTS.ucSampRateIdx =      (impl->readPtr[2]>>2)&0x0f;//AacGetBits(&pBsi, 4);
                //fhADTS.ucPivateBit =       AacGetBits(&pBsi, 1);
                fhADTS.ucChannelConfig =    ((impl->readPtr[2]<<2)&0x04)
                                             |((impl->readPtr[3]>>6)&0x03);//AacGetBits(&pBsi, 3);
                fhADTS.nFrameLength = ((int)(impl->readPtr[3]&0x3)<<11)
                                       |((int)(impl->readPtr[4]&0xff)<<3)
                                       |((impl->readPtr[5]>>5)&0x07);
                /* check validity of header */
                if(fhADTS.ucLayer != 0 || /*fhADTS.ucProfile != 1 ||*/
                    fhADTS.ucSampRateIdx >= 12 || fhADTS.ucChannelConfig >= 8
                    ||(fhADTS.nFrameLength>2*1024*2)||(fhADTS.nFrameLength>impl->bytesLeft))
                    retVal = -1;
            }
        }
        else//for latm
        {
            retVal = AACFindSyncWord_before_LATM(impl->readBuf,(int)(impl->readPtr-impl->readBuf));
            if(retVal==-1)
            {

                int ret = impl->readPtr-impl->readBuf+impl->bytesLeft+2* 1024;
                if(ret>impl->dFileOffset-impl->uFirstFrmOffset)
                {
                    ret = impl->dFileOffset-impl->uFirstFrmOffset
                          - (impl->readPtr-impl->readBuf+impl->bytesLeft);
                    if(ret<=0)
                        return -2;
                    if(CdxStreamSeek(impl->stream,impl->uFirstFrmOffset, SEEK_SET))
                    {
                        return -1;
                    }
                    retVal = CdxStreamRead(impl->stream, (void *)(impl->readBuf), ret);
                    if(retVal != ret)
                    {
                        return -1;
                    }
                    impl->bytesLeft = 0;
                    impl->readPtr = impl->readBuf + ret;
                    impl->dFileOffset = impl->uFirstFrmOffset;
                }
                else
                {
                    if(CdxStreamSeek(impl->stream,
                            -(int)(impl->readPtr-impl->readBuf+impl->bytesLeft+2* 1024), SEEK_CUR))
                    {
                        return -1;
                    }

                    retVal = CdxStreamRead(impl->stream, (void *)(impl->readBuf), 1024 * 2);
                    if(retVal != 1024*2)
                    {
                        return -1;
                    }
                    impl->bytesLeft = 0;
                    impl->readPtr = impl->readBuf + 2*1024;
                    impl->dFileOffset -= 2*1024;
                }
            }
            else
            {
                impl->bytesLeft += impl->readPtr - impl->readBuf - retVal;
                impl->readPtr = impl->readBuf + retVal;
            }
        }
    }
    return 0;
}

static int GetNextFrame(AacParserImplS *impl )
{
    int nBytes;
    int retVal=-1;
    int nRead = 0;
    ADTSHeaderT            fhADTS;
    //if (bytesLeft < ulchannels * 1024 * ulchannels +3 /*&& !eofReached*/) {
    if (impl->bytesLeft < 2 * 1024 * 2 && !impl->eofReached) {
        nRead = FillReadBuffer(impl,impl->readBuf, impl->readPtr,
                               impl->bytesLeft + 1024 * 2 * 2, impl->bytesLeft);
        impl->bytesLeft += nRead;
        impl->readPtr = impl->readBuf;
        if(nRead == 0)
        {
            return 0;

        }
        else if (nRead != 1024*2*2)
        {
            impl->eofReached = 1;
            //return -1;
        }

    }
    if(impl->ulformat == AAC_FF_ADTS)
    {
        nBytes = AACFindSyncWord(impl->readPtr,impl->bytesLeft);
        if(nBytes<0)
        {
            return -1;
        }

        impl->readPtr +=nBytes;
        impl->bytesLeft -=nBytes;
        fhADTS.nFrameLength = ((int)(impl->readPtr[3]&0x3)<<11)
                               |((int)(impl->readPtr[4]&0xff)<<3)|((impl->readPtr[5]>>5)&0x07);
        //nFrameLength error
        if((fhADTS.nFrameLength>2*1024*2)||(fhADTS.nFrameLength>impl->bytesLeft))
        {
            fhADTS.nFrameLength = 1;
        }
        impl->readPtr +=fhADTS.nFrameLength;
        impl->bytesLeft -=fhADTS.nFrameLength;
        while(retVal<0)
        {
            retVal = 0;
            //    if (bytesLeft < ulchannels * 1024 * ulchannels/*&& !eofReached*/) {
            if (impl->bytesLeft < 2 * 1024 * 2&& !impl->eofReached) {
                nRead = FillReadBuffer(impl,impl->readBuf, impl->readPtr,
                                       impl->bytesLeft + 1024* 2 * 2, impl->bytesLeft);
                impl->bytesLeft += nRead;
                impl->readPtr = impl->readBuf;
                /*if(bytesLeft==0)
                {
                return -1;
                }*/
                //if (nRead == 0)
                if(nRead == 0)
                {
                    return 0;
                }
                else if (nRead != 1024*2*2)
                {
                    impl->eofReached = 1;
                    //return -1;
                }
            }
            nBytes = AACFindSyncWord(impl->readPtr,impl->bytesLeft);
            if(nBytes<0)
            {
                return -1;
            }
            impl->readPtr +=nBytes;
            impl->bytesLeft -=nBytes;

            fhADTS.ucLayer =            (impl->readPtr[1]>>1)&0x03;//AacGetBits(&pBsi, 2);
            //fhADTS.ucProtectBit =       AacGetBits(&pBsi, 1);
            fhADTS.ucProfile =          (impl->readPtr[2]>>6)&0x03;//AacGetBits(&pBsi, 2);
            fhADTS.ucSampRateIdx =      (impl->readPtr[2]>>2)&0x0f;//AacGetBits(&pBsi, 4);
            //fhADTS.ucPivateBit =       AacGetBits(&pBsi, 1);
            fhADTS.ucChannelConfig =    ((impl->readPtr[2]<<2)&0x04)
                                         |((impl->readPtr[3]>>6)&0x03);//AacGetBits(&pBsi, 3);
            // check validity of header
            if (fhADTS.ucLayer != 0 ||
            fhADTS.ucSampRateIdx >= 12 || fhADTS.ucChannelConfig >= 8)
            {
                impl->bytesLeft -=1;
                impl->readPtr +=1;
                retVal = -1;
            }
        }
    }
    else//latm
    {
        nBytes = AACFindSyncWord_LATM(impl->readPtr,impl->bytesLeft);
        if(nBytes<0)
        {
            return -1;
        }
        impl->readPtr +=nBytes;
        impl->bytesLeft -=nBytes;
        fhADTS.nFrameLength = ((int)(impl->readPtr[1]&0x1F)<<8)|((int)(impl->readPtr[2]&0xff));
        //nFrameLength error
        if((fhADTS.nFrameLength>2*1024*2)||(fhADTS.nFrameLength>impl->bytesLeft))
        {
            fhADTS.nFrameLength = 0;
        }
        fhADTS.nFrameLength += 3;
        impl->readPtr +=fhADTS.nFrameLength;
        impl->bytesLeft -=fhADTS.nFrameLength;
        while(retVal<0)
        {
            retVal = 0;
            //    if (bytesLeft < ulchannels * 1024 * ulchannels/*&& !eofReached*/) {
            if (impl->bytesLeft < 2 * 1024 * 2&& !impl->eofReached) {
                nRead = FillReadBuffer(impl,impl->readBuf, impl->readPtr,
                                       impl->bytesLeft + 1024* 2 * 2, impl->bytesLeft);
                impl->bytesLeft += nRead;
                impl->readPtr = impl->readBuf;
                /*if(bytesLeft==0)
                {
                return -1;
                }*/
                //if (nRead == 0)
                if(nRead == 0)
                {
                    return 0;
                }
                else if (nRead != 1024*2*2)
                {
                    impl->eofReached = 1;
                    //return -1;
                }
            }
            nBytes = AACFindSyncWord_LATM(impl->readPtr,impl->bytesLeft);
            if(nBytes<0)
            {
                return -1;
            }
            impl->readPtr +=nBytes;
            impl->bytesLeft -=nBytes;
        }
    }
    return 0;
}

static int GetFrame(AacParserImplS *impl)
{
    cdx_int32 nBytes;
    cdx_int32 retVal    = -1;
    cdx_int32 nRead     = 0;
    cdx_int32 nSyncLen  = 0;
    cdx_int32 bytesLeft = impl->bytesLeft;
    cdx_uint8 *readPtr  = impl->readPtr;
    ADTSHeaderT            fhADTS;

    if(impl->ulformat == AAC_FF_ADTS)
    {
        while(retVal<0)
        {
            retVal = 0;
            //    if (bytesLeft < ulchannels * 1024 * ulchannels/*&& !eofReached*/) {
            if (bytesLeft < 2 * 1024 * 2&& !impl->eofReached) {
                nRead = FillReadBuffer(impl,impl->readBuf, impl->readPtr,
                                       impl->bytesLeft + 1024* 2 * 2, impl->bytesLeft);
                impl->bytesLeft += nRead;
                impl->readPtr = impl->readBuf;
                bytesLeft += nRead;
                readPtr = impl->readPtr + nSyncLen;
//                if(nRead == 0)
//                {
//                    return -1;
//                }
                if (nRead != 1024*2*2)
                {
                    impl->eofReached = 1;
                    //return -1;
                }
            }
            nBytes = AACFindSyncWord(readPtr,bytesLeft);
            if(nBytes<0)
            {
                CDX_LOGE("AACFindSyncWord error");
                return -1;
            }
            nSyncLen  += nBytes;
            readPtr   += nBytes;
            bytesLeft -= nBytes;

            fhADTS.ucLayer =            (readPtr[1]>>1)&0x03;//AacGetBits(&pBsi, 2);
            //fhADTS.ucProtectBit =       AacGetBits(&pBsi, 1);
            fhADTS.ucProfile =          (readPtr[2]>>6)&0x03;//AacGetBits(&pBsi, 2);
            fhADTS.ucSampRateIdx =      (readPtr[2]>>2)&0x0f;//AacGetBits(&pBsi, 4);
            //fhADTS.ucPivateBit =       AacGetBits(&pBsi, 1);
            //AacGetBits(&pBsi, 3);
            fhADTS.ucChannelConfig =    ((readPtr[2]<<2)&0x04)|((readPtr[3]>>6)&0x03);
            fhADTS.nFrameLength = ((int)(readPtr[3]&0x3)<<11)
                                   |((int)(readPtr[4]&0xff)<<3)|((readPtr[5]>>5)&0x07);
            // check validity of header
            //nFrameLength error
            if ((fhADTS.ucLayer != 0 || fhADTS.ucSampRateIdx >= 12 || fhADTS.ucChannelConfig >= 8)||
                  ((fhADTS.nFrameLength>2*1024*2)||(fhADTS.nFrameLength>bytesLeft)))
            {
                bytesLeft -=1;
                readPtr +=1;
                retVal = -1;
                nSyncLen +=1;
                if(nSyncLen > READLEN)//maybe sync too long
                {
                    CDX_LOGW("SyncWord :%d",nBytes);
                    impl->readPtr += nSyncLen;
                    impl->bytesLeft -= nSyncLen;
                    nSyncLen = 0;
                }
            }
            else
            {
                break;
            }
        }
    }
    else//latm
    {
        while(retVal<0)
        {
            retVal = 0;
            //    if (bytesLeft < ulchannels * 1024 * ulchannels/*&& !eofReached*/) {
            if (bytesLeft < 2 * 1024 * 2&& !impl->eofReached) {
                nRead = FillReadBuffer(impl,impl->readBuf, impl->readPtr,
                                       impl->bytesLeft + 1024* 2 * 2, impl->bytesLeft);
                impl->bytesLeft += nRead;
                impl->readPtr = impl->readBuf;
                bytesLeft += nRead;
                readPtr = impl->readPtr + nSyncLen;
                if (nRead != 1024*2*2)
                {
                    impl->eofReached = 1;
                    //return -1;
                }
            }
            nBytes = AACFindSyncWord_LATM(readPtr,bytesLeft);
            if(nBytes<0)
            {
                CDX_LOGE("LATMFindSyncWord error");
                return -1;
            }
            readPtr +=nBytes;
            bytesLeft -=nBytes;
            nSyncLen  += nBytes;
            fhADTS.nFrameLength = ((int)(readPtr[1]&0x1F)<<8)|((int)(readPtr[2]&0xff));
            //nFrameLength error
            if((fhADTS.nFrameLength>2*1024*2)||(fhADTS.nFrameLength>bytesLeft))
            {
                fhADTS.nFrameLength = 0;
                readPtr += 2;
                bytesLeft -= 2;
                nSyncLen +=2;
                retVal = -1;
                if(nSyncLen > READLEN)//maybe sync too long
                {
                    CDX_LOGW("SyncWord :%d",nBytes);
                    impl->readPtr += nSyncLen;
                    impl->bytesLeft -= nSyncLen;
                    nSyncLen = 0;
                }
            }
            else
            {
                 break;
            }
        }
    }
    return  nSyncLen + fhADTS.nFrameLength;
}

/**************************************************************************************
 * Function:    DecodeProgramConfigElement
 *
 * Description: decode one pPce
 *
 * Inputs:      BitStreamInfo struct pointing to start of pPce (14496-3, table 4.4.2)
 *
 * Outputs:     filled-in ProgConfigElementT struct
 *              updated BitStreamInfo struct
 *
 * Return:      0 if successful, error code (< 0) if error
 *
 * Notes:       #define KEEP_PCE_COMMENTS to save the comment field of the pPce
 *                (otherwise we just skip it in the bitstream, to save memory)
 **************************************************************************************/
static int DecodeProgramConfigElement(ProgConfigElementT *pPce, BitStreamInfo *pBsi)
{
  int i;

  pPce->ucElemInstTag =   AacGetBits(pBsi, 4);
  pPce->ucProfile =       AacGetBits(pBsi, 2);
  pPce->ucSampRateIdx =   AacGetBits(pBsi, 4);
  pPce->ucNumFCE =        AacGetBits(pBsi, 4);
  pPce->ucNumSCE =        AacGetBits(pBsi, 4);
  pPce->ucNumBCE =        AacGetBits(pBsi, 4);
  pPce->ucNumLCE =        AacGetBits(pBsi, 2);
  pPce->ucNumADE =        AacGetBits(pBsi, 3);
  pPce->ucNumCCE =        AacGetBits(pBsi, 4);

  pPce->ucMonoMixdown = AacGetBits(pBsi, 1) << 4;  /* present flag */
  if (pPce->ucMonoMixdown)
    pPce->ucMonoMixdown |= AacGetBits(pBsi, 4);  /* element number */

  pPce->ucStereoMixdown = AacGetBits(pBsi, 1) << 4;  /* present flag */
  if (pPce->ucStereoMixdown)
    pPce->ucStereoMixdown  |= AacGetBits(pBsi, 4); /* element number */

  pPce->ucMatrixMixdown = AacGetBits(pBsi, 1) << 4;  /* present flag */
  if (pPce->ucMatrixMixdown) {
    pPce->ucMatrixMixdown  |= AacGetBits(pBsi, 2) << 1;  /* index */
    pPce->ucMatrixMixdown  |= AacGetBits(pBsi, 1);     /* pseudo-surround enable */
  }

  for (i = 0; i < pPce->ucNumFCE; i++) {
    pPce->ucFce[i]  = AacGetBits(pBsi, 1) << 4;  /* is_cpe flag */
    pPce->ucFce[i] |= AacGetBits(pBsi, 4);     /* tag select */
  }

  for (i = 0; i < pPce->ucNumSCE; i++) {
    pPce->ucSce[i]  = AacGetBits(pBsi, 1) << 4;  /* is_cpe flag */
    pPce->ucSce[i] |= AacGetBits(pBsi, 4);     /* tag select */
  }

  for (i = 0; i < pPce->ucNumBCE; i++) {
    pPce->ucBce[i]  = AacGetBits(pBsi, 1) << 4;  /* is_cpe flag */
    pPce->ucBce[i] |= AacGetBits(pBsi, 4);     /* tag select */
  }

  for (i = 0; i < pPce->ucNumLCE; i++)
    pPce->ucLce[i] = AacGetBits(pBsi, 4);      /* tag select */

  for (i = 0; i < pPce->ucNumADE; i++)
    pPce->ucAde[i] = AacGetBits(pBsi, 4);      /* tag select */

  for (i = 0; i < pPce->ucNumCCE; i++) {
    pPce->ucCce[i]  = AacGetBits(pBsi, 1) << 4;  /* independent/dependent flag */
    pPce->ucCce[i] |= AacGetBits(pBsi, 4);     /* tag select */
  }

    i = pBsi->nCachedBits&0x07;
  if(i!=0)
  {
    AacGetBits(pBsi, i);
  }
  //ByteAlignBitstream(pBsi);

#ifdef KEEP_PCE_COMMENTS
  pPce->ucCommentBytes = AacGetBits(pBsi, 8);
  for (i = 0; i < pPce->ucCommentBytes; i++)
    pPce->ucCommentField[i] = AacGetBits(pBsi, 8);
#else
  /* eat comment bytes and throw away */
  i = AacGetBits(pBsi, 8);
  while (i--)
    AacGetBits(pBsi, 8);
#endif

  return 0;
}
/**************************************************************************************
 * Function:    GetNumChannelsADIF
 *
 * Description: get number of channels from program config elements in an ADIF file
 *
 * Inputs:      array of filled-in program config element structures
 *              number of pPce's
 *
 * Outputs:     none
 *
 * Return:      total number of channels in file
 *              -1 if error (invalid number of pPce's or unsupported mode)
 **************************************************************************************/

static int GetNumChannelsADIF(ProgConfigElementT *fhPCE, int nPCE)
{
  int i, j, nChans;

  if (nPCE < 1 || nPCE > 16)
    return -1;

  nChans = 0;
  for (i = 0; i < nPCE; i++) {
    /* for now: only support LC, no channel coupling */
    if (/*fhPCE[i].ucProfile != AAC_PROFILE_LC ||*/ fhPCE[i].ucNumCCE > 0)
      return -1;

    /* add up number of channels in all channel elements (assume all single-channel) */
        nChans += fhPCE[i].ucNumFCE;
        nChans += fhPCE[i].ucNumSCE;
        nChans += fhPCE[i].ucNumBCE;
        nChans += fhPCE[i].ucNumLCE;

    /* add one more for every element which is a channel pair */
        for (j = 0; j < fhPCE[i].ucNumFCE; j++) {
            if (CHAN_ELEM_IS_CPE(fhPCE[i].ucFce[j]))
                nChans++;
        }
        for (j = 0; j < fhPCE[i].ucNumSCE; j++) {
            if (CHAN_ELEM_IS_CPE(fhPCE[i].ucSce[j]))
                nChans++;
        }
        for (j = 0; j < fhPCE[i].ucNumBCE; j++) {
            if (CHAN_ELEM_IS_CPE(fhPCE[i].ucBce[j]))
                nChans++;
        }

  }

  return nChans;
}

/**************************************************************************************
 * Function:    GetSampleRateIdxADIF
 *
 * Description: get sampling rate index from program config elements in an ADIF file
 *
 * Inputs:      array of filled-in program config element structures
 *              number of pPce's
 *
 * Outputs:     none
 *
 * Return:      sample rate of file
 *              -1 if error (invalid number of pPce's or sample rate mismatch)
 **************************************************************************************/
static int GetSampleRateIdxADIF(ProgConfigElementT *fhPCE, int nPCE)
{
    int i, idx;

    if (nPCE < 1 || nPCE > AAC_MAX_NUM_PCE_ADIF)
        return -1;

    /* make sure all pPce's have the same sample rate */
    idx = fhPCE[0].ucSampRateIdx;
    for (i = 1; i < nPCE; i++) {
        if (fhPCE[i].ucSampRateIdx != idx)
            return -1;
    }

    return idx;
}

static int UnpackADIFHeader(AacParserImplS *impl,unsigned char *ptr,int len)
{
    int ret;
    int i;
    int ulChannels,ulSampleRate;
    ADIFHeaderT fhADIF;
    BitStreamInfo pBsi;
    ProgConfigElementT pPce[16];
    SetBitstreamPointer(&pBsi, len-4, (unsigned char*)(ptr+4));
    if(AacGetBits(&pBsi, 1))
    {
        for (i = 0; i < 9; i++)
            fhADIF.ucCopyID[i] = AacGetBits(&pBsi, 8);
    }
    fhADIF.ucOrigCopy = AacGetBits(&pBsi, 1);
    fhADIF.ucHome =     AacGetBits(&pBsi, 1);
    fhADIF.ucBsType =   AacGetBits(&pBsi, 1);
    fhADIF.nBitRate =  AacGetBits(&pBsi, 23);
    fhADIF.ucNumPCE =   AacGetBits(&pBsi, 4) + 1; /* add 1 (so range = [1, 16]) */
    if (fhADIF.ucBsType == 0)
    fhADIF.nBufferFull = AacGetBits(&pBsi, 20);
    /* parse all program config elements */
    for (i = 0; i < fhADIF.ucNumPCE; i++)
        DecodeProgramConfigElement(pPce + i, &pBsi);

    /* byte align */
    //ByteAlignBitstream(&pBsi);
    ret = pBsi.nCachedBits&0x07;
    if(ret!=0)AacGetBits(&pBsi, ret);

    /* update codec info */
    //  AIF->ulChannels = GetNumChannelsADIF(pPce, fhADIF.ucNumPCE);
    //  AIF->ulSampleRate = GetSampleRateIdxADIF(pPce, fhADIF.ucNumPCE);
    ulChannels = GetNumChannelsADIF(pPce, fhADIF.ucNumPCE);
    ulSampleRate = GetSampleRateIdxADIF(pPce, fhADIF.ucNumPCE);

    /* check validity of header */
    //if (AIF->ulChannels < 0 || AIF->ulSampleRate < 0 || AIF->ulSampleRate >= 12)
    if (ulChannels < 0 || ulSampleRate < 0 || ulSampleRate >= 12)
    {
        CDX_LOGE("ERROR: ulChannels:%d,ulSampleRate[0-11]:%d ",ulChannels,ulSampleRate);
        return ERROR;
    }
    impl->ulChannels  = ulChannels;
    impl->ulSampleRate = sampRateTab[ulSampleRate];
    if(fhADIF.nBitRate!=0)
    {
        impl->ulBitRate =fhADIF.nBitRate;
        impl->ulDuration = (int)(((double)(impl->dFileSize-impl->uFirstFrmOffset)
                                 * (double)(8))*1000 /((double)impl->ulBitRate) );
    }
    return SUCCESS;
}

static cdx_int32 AacProbe(cdx_uint8 *ptr)
{
    cdx_int32 offset = 0;
    /*We give up the judgement of id3 tag, instead of moving it to id3 parser*/
    if((((ptr[offset]&0xff)==0xff)&&((ptr[offset + 1]&0xf0)==0xf0)
        &&(((ptr[offset + 1] & (0x3<<1)) == 0x0<<1)
        ||((ptr[offset + 1] & (0x3<<1)) == 0x3<<1))&&((ptr[offset + 2] & (0xf<<2)) < (12<<2))
        &&(((ptr[offset + 2]&0x01)|(ptr[offset + 3]&(0x03<<6)))!=0))
        ||(((ptr[offset]&0xff)==0x56)&&((ptr[offset + 1]&0xe0)==0xe0))
        ||(((ptr[offset]&0xff)=='A')&&((ptr[offset + 1]&0xff)=='D')
        &&((ptr[offset + 2]&0xff)=='I')&&((ptr[offset + 3]&0xff)=='F')))
    {
        if(((ptr[offset]&0xff)==0x56)&&((ptr[offset + 1]&0xe0)==0xe0))
        {
            int latm_size = 0;
            int state = ((ptr[offset]&0xff) << 16) | ((ptr[offset+1]&0xff) << 8) |
                    ((ptr[offset+2]&0xff));
            latm_size = state & LATM_SIZE_MASK;
            CDX_LOGD("latm_size : %d", latm_size);
            /*
                For our aac parser at least have 4096 buffer size to avoid out of range
                The most important, a aac frame has only 1024 samples, that is the size
                cannot exceed 1024*2*2 = 4096bytes if lossless, though it's very impossiable
             */
            if(latm_size > 4090)
            {
                return CDX_FALSE;
            }
            if(((ptr[offset + latm_size]&0xff)==0x56)&&((ptr[offset + latm_size + 1]&0xe0)==0xe0))
            {
                CDX_LOGD("This is realy latm...");
            }
            else
            {
                CDX_LOGE("This is not latm...");
                return CDX_FALSE;
            }
        }
        return CDX_TRUE;
    }
    //CDX_LOGD("Cannot Sync to a AAC header!!!");
    return CDX_FALSE;
}

static int AacInit(CdxParserT* parameter)
{
    cdx_int32 ret = 0;
    int needOffset = 0;
    int readlength = 0;
    int readlength_copy = 0;
    int i = 0;
    int nBytes = 0;
    int retVal = -1;
    int frameOn = 0;
    int frameOn_copy = 0;
    ADTSHeaderT            fhADTS;
    struct AacParserImplS *impl          = NULL;

    impl = (AacParserImplS *)parameter;
    impl->dFileSize = CdxStreamSize(impl->stream);
    impl->bSeekable = CdxStreamSeekAble(impl->stream);

    ret = CdxStreamRead(impl->stream, (void *)impl->readBuf, READLEN);
    if(ret != READLEN)
    {
        CDX_LOGE("CdxStreamRead error");
        goto AAC_error;
    }
    while(needOffset + 7 < READLEN)
    {
        if(AacProbe(impl->readBuf + needOffset))
            break;
        needOffset++;
    }

    CDX_LOGD("First frame needOffset : %d", needOffset);

    if(needOffset + 7 >= READLEN)
    {
        CDX_LOGE("Too Many Rubbish data!");
        goto AAC_error;
    }
    else if(needOffset > 0)
    {
        FillReadBuffer(impl, impl->readBuf,impl->readBuf + needOffset,
                        READLEN, READLEN - needOffset);
    }
    readlength += needOffset;

    if (IS_ADIF(impl->readBuf))
    {
        /* unpack ADIF header */

        impl->ulformat = AAC_FF_ADIF;
        ret = UnpackADIFHeader(impl,impl->readBuf,READLEN);
        if(ret==ERROR)
        {
            CDX_LOGE("UnpackADIFHeader error");
            goto AAC_error;
        }
    }
    else
    {
        {
            //BsInfo      *BSINFO = (BsInfo *)AIF->ulBSINFO;
            unsigned char *pBuf = impl->readBuf;
            for (i = 0; i < READLEN-1; i++)
            {
                if ( (pBuf[i+0] & AAC_SYNCWORDH) == AAC_SYNCWORDH
                        && (pBuf[i+1] & AAC_SYNCWORDL) == AAC_SYNCWORDL )
                {
                    impl->ulformat = AAC_FF_ADTS;
                    break;
                }
                if ( (pBuf[i+0] & AAC_SYNCWORDH) == AAC_SYNCWORDL_H
                        && (pBuf[i+1] & AAC_SYNCWORDL_LATM) == AAC_SYNCWORDL_LATM )
                {
                    impl->ulformat = AAC_FF_LATM;
                    break;
                }
            }
            if(i != 0)
            {
                CDX_LOGW("Before we begin to judge ADTS or LATM, we shift %d bytes. It is strange,\
                        'cause we has done it before!!!!", i);
                goto AAC_error;
            }
        }

        nBytes = 0;
        if(impl->ulformat == AAC_FF_ADTS)
        {
            unsigned char *pBuf = impl->readBuf;

            for(i=0;(i<ERRORFAMENUM )&&(retVal<0);i++)
            {
                retVal = 0;

//This code fragment is useless
//But cannot delete it now.
#if 0
                if(2*nBytes>READLEN)
                {
                    if(CdxStreamSeek(impl->stream,readlength,SEEK_SET))
                    {
                        CDX_LOGE("CdxStreamSeek error");
                        goto AAC_error;
                    }
                    ret = CdxStreamRead(impl->stream, (void *)impl->readBuf, READLEN);
                    if(ret != READLEN)
                    {
                        CDX_LOGE("CdxStreamRead error");
                        goto AAC_error;
                    }
                    nBytes = 0;
                }

                nBytes = AACFindSyncWord(impl->readBuf,READLEN - nBytes);//maybe 4*1024;
                if(nBytes<0)
                {
                    CDX_LOGE("AACFindSyncWord error");
                    goto AAC_error;
                }
                readlength += nBytes;
                if(impl->dFileSize > 0 && readlength >= impl->dFileSize)
                {
                    readlength =impl->dFileSize;
                    CDX_LOGE("dFileSize error");
                    goto AAC_error;
                }
                if(nBytes+7>READLEN)
                {
                    if(CdxStreamSeek(impl->stream,readlength,SEEK_SET))
                    {
                        CDX_LOGE("CdxStreamSeek error");
                        goto AAC_error;
                    }
                    ret = CdxStreamRead(impl->stream, (void *)impl->readBuf, READLEN);
                    if(ret != READLEN)
                    {
                        CDX_LOGE("CdxStreamRead error");
                        goto AAC_error;
                    }
                    nBytes = 0;
                }
#endif
                //AacGetBits(&pBsi, 2);
                fhADTS.ucLayer =            (impl->readBuf[nBytes + 1]>>1)&0x03;
                //fhADTS.ucProtectBit =       AacGetBits(&pBsi, 1);
                //AacGetBits(&pBsi, 2);
                fhADTS.ucProfile =          (impl->readBuf[nBytes + 2]>>6)&0x03;
                //AacGetBits(&pBsi, 4);
                fhADTS.ucSampRateIdx =      (impl->readBuf[nBytes + 2]>>2)&0x0f;
                //fhADTS.ucPivateBit =       AacGetBits(&pBsi, 1);
                //AacGetBits(&pBsi, 3);
                fhADTS.ucChannelConfig =    ((impl->readBuf[nBytes + 2]<<2)&0x04)
                                             |((impl->readBuf[nBytes + 3]>>6)&0x03);
                /* check validity of header */
                fhADTS.nFrameLength = ((int)(impl->readBuf[nBytes + 3]&0x3)<<11)
                                       |((int)(impl->readBuf[nBytes + 4]&0xff)<<3)
                                       |((impl->readBuf[nBytes + 5]>>5)&0x07);
                if (((fhADTS.ucLayer != 0 )&&(fhADTS.ucLayer != 3 ))/*|| fhADTS.ucProfile != 1*/ ||
                      fhADTS.ucSampRateIdx >= 12 || fhADTS.ucChannelConfig >= 8
                      ||fhADTS.nFrameLength>READLEN)
                {
                    nBytes +=1 ;
                    readlength +=1;
                    retVal = -1;
                }
                else
                {
                    unsigned char cBuf[2];
                    if(fhADTS.nFrameLength+2>READLEN-nBytes)
                    {
                        if(impl->bSeekable)
                        {
                            if(CdxStreamSeek(impl->stream,readlength +
                                             fhADTS.nFrameLength,SEEK_SET))
                            {
                                CDX_LOGE("CdxStreamSeek error");
                                goto AAC_error;
                            }
                            ret = CdxStreamRead(impl->stream, (void *)cBuf, 2);
                        }
                        else
                        {
                            cdx_uint8 tempbuf[1024] = {0};
                            cdx_int32 readlen = nBytes + fhADTS.nFrameLength - READLEN;
                            while(1)
                            {
                                if(readlen == -1)
                                {
                                    cBuf[0] = impl->readBuf[nBytes + fhADTS.nFrameLength];
                                    ret = CdxStreamRead(impl->stream, (void *)(&(cBuf[1])), 1);
                                    if(ret != 1)
                                    {
                                        CDX_LOGE("CdxStreamRead error");
                                        goto AAC_error;
                                    }
                                    break;
                                }
                                if(readlen <= 1024)
                                {
                                    CdxStreamRead(impl->stream, (void *)tempbuf, readlen);
                                    ret = CdxStreamRead(impl->stream, (void *)cBuf, 2);
                                    if(ret != 2)
                                    {
                                        CDX_LOGE("CdxStreamRead error");
                                        goto AAC_error;
                                    }
                                    break;
                                }
                                CdxStreamRead(impl->stream, (void *)tempbuf, 1024);
                                readlen -= 1024;
                            }
                        }
                        pBuf =  cBuf;
                    }
                    else
                    {
                        pBuf = impl->readBuf + nBytes + fhADTS.nFrameLength;
                    }
                    if ( (pBuf[0] & AAC_SYNCWORDH) == AAC_SYNCWORDH
                            && (pBuf[1] & AAC_SYNCWORDL) == AAC_SYNCWORDL )
                    {
                        retVal = 0;
                        break;
                    }
                    else
                    {
                        nBytes +=1 ;
                        readlength +=1;
                        retVal = -1;
                    }
                }
                if(readlength-needOffset>2*1024)
                {
                    CDX_LOGE("readlength error");
                    goto AAC_error;
                }
            }
            if(i==ERRORFAMENUM)
            {
                CDX_LOGE("ERRORFAMENUM error");
                goto AAC_error;
            }
            impl->ulSampleRate = sampRateTab[fhADTS.ucSampRateIdx];
        }
        ////////////////////////////////////////////////////////////////
        else//AAC_format = AAC_FF_LATM
        {
            unsigned char *pBuf = impl->readBuf;
            int nLength = 0;
            for(i=0;(i<ERRORFAMENUM )&&(retVal<0);i++)
            //while(retVal<0)
            {
                retVal = 0;
//This code fragment is useless
//But cannot delete it now.
#if 0
                if(2*nBytes>READLEN)
                {
                    if(CdxStreamSeek(impl->stream,readlength,SEEK_SET))
                    {
                        goto AAC_error;
                    }
                    ret = CdxStreamRead(impl->stream, (void *)impl->readBuf, READLEN);
                    if(ret != READLEN)
                    {
                        goto AAC_error;
                    }
                    nBytes = 0;
                }
                nBytes = AACFindSyncWord_LATM(impl->readBuf,READLEN - nBytes);
                if(nBytes<0)goto AAC_error;
                readlength += nBytes;
                if(readlength>=impl->dFileSize)
                {
                    readlength =impl->dFileSize;
                    goto AAC_error;
                }
                if(nBytes+7>READLEN)
                {
                    if(CdxStreamSeek(impl->stream,readlength,SEEK_SET))
                    {
                        goto AAC_error;
                    }
                    ret = CdxStreamRead(impl->stream, (void *)impl->readBuf, READLEN);
                    if(ret != READLEN)
                    {
                        goto AAC_error;
                    }
                    nBytes = 0;
                }
#endif
                /****************************************************************/
                nLength = (((int)impl->readBuf[nBytes + 1]&0x1f)<<8)
                           |((int)impl->readBuf[nBytes + 2]&0xff);
                fhADTS.nFrameLength = nLength + 3;
                if((impl->readBuf[nBytes]&0xC0)||(fhADTS.nFrameLength>READLEN))
                {
                        nBytes +=1 ;
                    readlength +=1;
                    retVal = -1;
                }
                else
                {
                    unsigned char cBuf[2];
                    if(fhADTS.nFrameLength+2>READLEN-nBytes)
                    {
                        if(impl->bSeekable)
                        {
                            if(CdxStreamSeek(impl->stream,readlength +
                               fhADTS.nFrameLength,SEEK_SET))
                            {
                                goto AAC_error;
                            }
                            ret = CdxStreamRead(impl->stream, (void *)cBuf, 2);
                        }
                        else
                        {
                            cdx_uint8 tempbuf[1024] = {0};
                            cdx_int32 readlen = nBytes + fhADTS.nFrameLength - READLEN;
                            while(1)
                            {
                                if(readlen == -1)
                                {
                                    cBuf[0] = impl->readBuf[nBytes + fhADTS.nFrameLength];
                                    ret = CdxStreamRead(impl->stream, (void *)(&(cBuf[1])), 1);
                                    if(ret != 1)
                                    {
                                        CDX_LOGE("CdxStreamRead error");
                                        goto AAC_error;
                                    }
                                    break;
                                }
                                if(readlen <= 1024)
                                {
                                    CdxStreamRead(impl->stream, (void *)tempbuf, readlen);
                                    ret = CdxStreamRead(impl->stream, (void *)cBuf, 2);
                                    if(ret != 2)
                                    {
                                        CDX_LOGE("CdxStreamRead error");
                                        goto AAC_error;
                                    }
                                    break;
                                }
                                CdxStreamRead(impl->stream, (void *)tempbuf, 1024);
                                readlen -= 1024;
                            }

                        }
                        pBuf =  cBuf;
                    }
                    else
                    {
                        pBuf = impl->readBuf + nBytes + fhADTS.nFrameLength;
                    }
                    if ( (pBuf[0] & AAC_SYNCWORDH) == AAC_SYNCWORDL_H
                            && (pBuf[1] & AAC_SYNCWORDL_LATM) == AAC_SYNCWORDL_LATM )
                    {
                        retVal = 0;
                        //useSameStreamMux == 0 && audioMuxVersion==0
                        //AacGetBits(&pBsi,3); //3 uimsbf
                        fhADTS.ucLayer =  impl->readBuf[nBytes + 1 + 3]&0x07;
                        //AacGetBits(&pBsi,5); //5 bslbf
                        //audioObjectType = (GetInfo_ShowBs(2,1,AIF)>>3)&0x1F;
                        fhADTS.ucSampRateIdx = 2*((impl->readBuf[nBytes + 2 + 3])&0x07)
                                               + ((impl->readBuf[nBytes + 3+3]>>7)&0x01);
                        //AacGetBits(&pBsi,4); //4 bslbf
                        if ( fhADTS.ucSampRateIdx==0xf )
                        {
                            //AacGetBits(&pBsi,24); //24 uimsbf
                            impl->ulSampleRate = ((impl->readBuf[nBytes +3+3]&0x07F)<<17)
                                                   | (impl->readBuf[nBytes +4+3] << 9)
                                                   | (impl->readBuf[nBytes +5+3] << 1)
                                                   | ((impl->readBuf[nBytes +6+3] >> 7)&0x01);
                            //AacGetBits(&pBsi,4);
                            fhADTS.ucChannelConfig = ((impl->readBuf[nBytes +6+3] >> 3)&0x0F);
                        }
                        else
                        {
                            impl->ulSampleRate = sampRateTab[fhADTS.ucSampRateIdx];
                            fhADTS.ucChannelConfig = ((impl->readBuf[nBytes +3+3] >> 3)&0x0F);
                        }
                        break;
                    }
                    else
                    {
                        nBytes +=1 ;
                        readlength +=1;
                        retVal = -1;
                    }
                }
            }
            if(i==ERRORFAMENUM)
            {
                goto AAC_error;
            }
        }

        impl->ulChannels        = channelMapTab[fhADTS.ucChannelConfig];

        impl->uFirstFrmOffset = readlength;
        impl->dFileOffset = readlength;
        impl->readPtr = impl->readBuf;
        impl->bytesLeft = 0;
        ret =0;
        if(impl->dFileSize > 0 && impl->bSeekable)
        {
            if(CdxStreamSeek(impl->stream,impl->uFirstFrmOffset,SEEK_SET))
            {
                CDX_LOGE("CdxStreamSeek error");
                goto AAC_error;
            }

            #ifndef TRYALL

            ret = 10*impl->ulSampleRate /1024;
            for(i=0;i<ret;i++)
            {
                retVal = GetNextFrame(impl);
                frameOn++;
                if(retVal==-1)break;
            }
            readlength =impl->dFileOffset + (cdx_int32)((uintptr_t)impl->readPtr
                        - (uintptr_t)impl->readBuf);
            if(i==ret)
            {
                readlength_copy =readlength;
                frameOn_copy=frameOn;
                ret = AuInfTime*impl->ulSampleRate /1024;
                for(i=0;i<ret;i++)
                {
                    retVal = GetNextFrame(impl);
                    frameOn++;
                    if(retVal==-1)break;
                }
            }
            readlength =impl->dFileOffset
                    + (cdx_int32)((uintptr_t)impl->readPtr - (uintptr_t)impl->readBuf);
            if(i==ret)
            {
                readlength =readlength - readlength_copy + impl->uFirstFrmOffset ;
                frameOn -= frameOn_copy;
            }
            #else

            while(ret>=0)
            {
                ret = GetNextFrame(impl);
                frameOn++;
            }
            readlength = impl->dFileSize;
            #endif

            impl->ulBitRate  = (int)((double)((double)(readlength-impl->uFirstFrmOffset)*(double)8
                                    *(double)impl->ulSampleRate)/(double)((double)frameOn*1024));
            impl->ulDuration = (int)((double)((double)(impl->dFileSize-impl->uFirstFrmOffset)*8000)
                                    /(double)impl->ulBitRate);
            //AAC_format = AAC_FF_ADTS;
            if(impl->ulBitRate>impl->ulSampleRate*impl->ulChannels*16)
            {
                CDX_LOGE("aac ulBitRate error.rate:%d,fs:%d,ch:%d",impl->ulBitRate,
                      impl->ulSampleRate,impl->ulChannels);
                goto AAC_error;//for aac not nBitRate lag
            }

            if(CdxStreamSeek(impl->stream,needOffset,SEEK_SET))
            {
                CDX_LOGE("CdxStreamSeek error");
                goto AAC_error;
            }
            //Reset the readBuf...
            ResetReadBuffer(impl);
        }
    }
    impl->eofReached = 0;
    impl->bytesLeft = 0;
    impl->mErrno = PSR_OK;
    pthread_cond_signal(&impl->cond);
    CDX_LOGW("AAC ulDuration:%lld",impl->ulDuration);
    return 0;
AAC_error:
    CDX_LOGE("AacOpenThread fail!!!");
    impl->mErrno = PSR_OPEN_FAIL;
    pthread_cond_signal(&impl->cond);
    return -1;
}

static cdx_int32 __AacParserControl(CdxParserT *parser, cdx_int32 cmd, void *param)
{
    (void)param;
    cdx_int64 streamSeekPos = 0, timeUs = 0;
    cdx_int32 ret = 0,nFrames = 0;
    struct AacParserImplS *impl = NULL;
    impl = CdxContainerOf(parser, struct AacParserImplS, base);
    switch (cmd)
    {
    case CDX_PSR_CMD_DISABLE_AUDIO:
    case CDX_PSR_CMD_DISABLE_VIDEO:
    case CDX_PSR_CMD_SWITCH_AUDIO:
        break;
    case CDX_PSR_CMD_SET_FORCESTOP:
        CdxStreamForceStop(impl->stream);
        break;
    case CDX_PSR_CMD_CLR_FORCESTOP:
        CdxStreamClrForceStop(impl->stream);
        break;
    case CDX_PSR_CMD_REPLACE_STREAM:
        CDX_LOGD("replace stream!!!");
        if(impl->stream)
        {
            CdxStreamClose(impl->stream);
        }
        impl->stream = (CdxStreamT*)param;
        impl->eofReached = 0;
        impl->mErrno = PSR_OK;
        break;
    case CDX_PSR_CMD_STREAM_SEEK:
        if(!impl->stream)
        {
            CDX_LOGE("mAACParser->cdxStream == NULL, can not stream control");
            return -1;
        }
        if(!CdxStreamSeekAble(impl->stream))
        {
            CDX_LOGV("CdxStreamSeekAble == 0");
            return 0;
        }
        streamSeekPos = *(cdx_int64 *)param;
        ret = CdxStreamSeek(impl->stream, streamSeekPos, SEEK_SET);
        if(ret < 0)
        {
            CDX_LOGE("CdxStreamSeek fail");
        }
        memset(impl->readBuf,0x00,2*1024*6*6);
        impl->readPtr = impl->readBuf;
        impl->bytesLeft = 0;
        ret = GetNextFrame(impl);
        if(ret<0)
        {
            CDX_LOGE("Controll GetNextFrame error");
            return CDX_FAILURE;
        }
        impl->eofReached = 0;
        nFrames = (timeUs/1000000) * impl->ulSampleRate /1024;
        impl->nFrames = nFrames;

        break;
    default :
        CDX_LOGW("not implement...(%d)", cmd);
        break;
    }
    impl->nFlags = cmd;

    return CDX_SUCCESS;
}

static cdx_int32 __AacParserPrefetch(CdxParserT *parser, CdxPacketT *pkt)
{
    struct AacParserImplS *impl = NULL;
    int ret = 0;
    int nReadLen = 0;
    impl = CdxContainerOf(parser, struct AacParserImplS, base);
    pkt->type = CDX_MEDIA_AUDIO;
    if(impl->ulformat == AAC_FF_ADIF)
    {
        nReadLen = AAC_MAINBUF_SIZE;
        pkt->pts = -1;
        ret = CdxStreamRead(impl->stream, (void *)impl->readBuf, AAC_MAINBUF_SIZE);
        if(ret < 0)
        {
            CDX_LOGE("CdxStreamRead fail");
            impl->mErrno = PSR_IO_ERR;
            return CDX_FAILURE;
        }
        else if(ret == 0)
        {
           CDX_LOGD("CdxStream EOS");
           impl->mErrno = PSR_EOS;
           return CDX_FAILURE;
        }
        if(ret != AAC_MAINBUF_SIZE)
        {
           CDX_LOGD("CdxStream EOS");
           impl->mErrno = PSR_EOS;
        }

        impl->readPtr = impl->readBuf;
        impl->bytesLeft = ret;
        pkt->length = ret;
    }
    else if(impl->ulformat == AAC_FF_ADTS)
    {
        ret = GetFrame(impl);
        if(ret<0)
        {
            if(impl->eofReached)
            {
                CDX_LOGD("CdxStream EOS");
                impl->mErrno = PSR_EOS;
            }
            pkt->length = impl->bytesLeft;
            pkt->pts = -1;
            CDX_LOGW("maybe sync err");
        }
        else
        {
            pkt->length =  ret;
            pkt->pts = (cdx_int64)impl->nFrames* 1024 *1000000/impl->ulSampleRate;
        }
    }

    CDX_LOGV("****len:%d,",pkt->length);
    if(pkt->length == 0)
    {
       CDX_LOGD("CdxStream EOS");
       impl->mErrno = PSR_EOS;
       return CDX_FAILURE;
    }

    pkt->flags |= (FIRST_PART|LAST_PART);
    //pkt->pts = (cdx_int64)impl->frames*impl->frame_samples *1000000
    //             /impl->aac_format.nSamplesPerSec;//-1;

    return CDX_SUCCESS;
}

static cdx_int32 __AacParserRead(CdxParserT *parser, CdxPacketT *pkt)
{
    struct AacParserImplS *impl = NULL;
    CdxStreamT *cdxStream = NULL;

    impl = CdxContainerOf(parser, struct AacParserImplS, base);
    cdxStream = impl->stream;

    if(pkt->length <= pkt->buflen)
    {
        memcpy(pkt->buf, impl->readPtr, pkt->length);
    }
    else
    {
        memcpy(pkt->buf, impl->readPtr, pkt->buflen);
        memcpy(pkt->ringBuf, impl->readPtr + pkt->buflen, pkt->length - pkt->buflen);
    }

    impl->readPtr   += pkt->length;
    impl->bytesLeft -= pkt->length;
    CDX_LOGV("****len:%d,",pkt->length);
    if(pkt->pts != -1)
    {
        impl->nFrames++;
    }
    if(pkt->length == 0)
    {
       CDX_LOGD("CdxStream EOS");
       impl->mErrno = PSR_EOS;
       return CDX_FAILURE;
    }

    // TODO: fill pkt
    return CDX_SUCCESS;
}

static cdx_int32 __AacParserGetMediaInfo(CdxParserT *parser, CdxMediaInfoT *mediaInfo)
{
    struct AacParserImplS *impl;
    struct CdxProgramS *cdxProgram = NULL;

    impl = CdxContainerOf(parser, struct AacParserImplS, base);
    memset(mediaInfo, 0x00, sizeof(*mediaInfo));

    if(impl->mErrno != PSR_OK)
    {
        CDX_LOGW("audio parse status no PSR_OK");
        return CDX_FAILURE;
    }

    mediaInfo->programNum = 1;
    mediaInfo->programIndex = 0;
    cdxProgram = &mediaInfo->program[0];
    memset(cdxProgram, 0, sizeof(struct CdxProgramS));
    cdxProgram->id = 0;
    cdxProgram->audioNum = 1;
    cdxProgram->videoNum = 0;//
    cdxProgram->subtitleNum = 0;
    cdxProgram->audioIndex = 0;
    cdxProgram->videoIndex = 0;
    cdxProgram->subtitleIndex = 0;

    if(impl->bSeekable)
    {
        if((impl->ulformat != AAC_FF_ADTS)&&(impl->ulformat != AAC_FF_LATM))
        {
            impl->bSeekable = 0;
        }
    }
    mediaInfo->bSeekable = impl->bSeekable;
    mediaInfo->fileSize = impl->dFileSize;

    cdxProgram->duration = impl->ulDuration;
    cdxProgram->audio[0].eCodecFormat    = AUDIO_CODEC_FORMAT_RAAC;
    cdxProgram->audio[0].eSubCodecFormat = 0;//impl->WavFormat.wFormatag | impl->nBigEndian;
    cdxProgram->audio[0].nChannelNum     = impl->ulChannels;
    cdxProgram->audio[0].nBitsPerSample  = 16;
    cdxProgram->audio[0].nSampleRate     = impl->ulSampleRate;
    cdxProgram->audio[0].nAvgBitrate     = impl->ulBitRate;
    //cdxProgram->audio[0].nMaxBitRate;
    //cdxProgram->audio[0].nFlags
    cdxProgram->audio[0].nBlockAlign     = 0;//impl->WavFormat.nBlockAlign;
    //CDX_LOGD("eSubCodecFormat:0x%04x,ch:%d,fs:%d",cdxProgram->audio[0].eSubCodecFormat,
    //            cdxProgram->audio[0].nChannelNum,cdxProgram->audio[0].nSampleRate);
    //CDX_LOGD("AAC ulDuration:%d",cdxProgram->duration);

    return CDX_SUCCESS;
}

static cdx_int32 __AacParserSeekTo(CdxParserT *parser, cdx_int64 timeUs, SeekModeType seekModeType)
{
    CDX_UNUSE(seekModeType);
    struct AacParserImplS *impl = NULL;
    cdx_int32 nFrames = 0;
    cdx_int32 ret = 0;
    int i = 0;
    impl = CdxContainerOf(parser, struct AacParserImplS, base);

    if(!impl->bSeekable)
    {
        CDX_LOGD("bSeekable = 0");
        return CDX_FAILURE;
    }
    nFrames = (timeUs/1000000) * impl->ulSampleRate /1024;
    if(nFrames > impl->nFrames)//ff
    {
        int skipframeN = nFrames - impl->nFrames;
        for(i=0;i<skipframeN;i++)
        {
            ret = GetNextFrame(impl);
            if(impl->eofReached == 1)
            {
                skipframeN = i;
                break;
            }
            if(ret<0)
            {
                CDX_LOGE("GetNextFrame error");
                return CDX_FAILURE;
            }
        }
        impl->nFrames +=skipframeN;

    }
    else if(nFrames < impl->nFrames)
    {
        int skipframeN = impl->nFrames - nFrames;
        for(i=0;i<skipframeN;i++)
        {
            ret = GetBeforeFrame(impl);
            if(ret<0)
            {
                CDX_LOGE("GetBeforeFrame error");
                return CDX_FAILURE;
            }
         }
        impl->nFrames -=skipframeN;
        impl->eofReached = 0;

    }
    // TODO: not implement now...
    pthread_cond_signal(&impl->cond);
    CDX_LOGI("TODO, seek to now...");
    return CDX_SUCCESS;
}

static cdx_uint32 __AacParserAttribute(CdxParserT *parser)
{
    struct AacParserImplS *impl = NULL;

    impl = CdxContainerOf(parser, struct AacParserImplS, base);
    return 0;
}


static cdx_int32 __AacParserGetStatus(CdxParserT *parser)
{
    struct AacParserImplS *impl = NULL;

    impl = CdxContainerOf(parser, struct AacParserImplS, base);
#if 0
    if (CdxStreamEos(impl->stream))
    {
        CDX_LOGE("file PSR_EOS! ");
        return PSR_EOS;
    }
#endif
    return impl->mErrno;
}

static cdx_int32 __AacParserClose(CdxParserT *parser)
{
    struct AacParserImplS *impl = NULL;

    impl = CdxContainerOf(parser, struct AacParserImplS, base);
    CdxStreamClose(impl->stream);
    pthread_cond_destroy(&impl->cond);
    CdxFree(impl);
    return CDX_SUCCESS;
}

static struct CdxParserOpsS aacParserOps =
{
    .control      = __AacParserControl,
    .prefetch     = __AacParserPrefetch,
    .read         = __AacParserRead,
    .getMediaInfo = __AacParserGetMediaInfo,
    .seekTo       = __AacParserSeekTo,
    .attribute    = __AacParserAttribute,
    .getStatus    = __AacParserGetStatus,
    .close        = __AacParserClose,
    .init         = AacInit
};

static cdx_uint32 __AacParserProbe(CdxStreamProbeDataT *probeData)
{
    CDX_CHECK(probeData);
    if(probeData->len < 7)
    {
        CDX_LOGE("Probe data is not enough.");
        return 0;
    }

    int max_frames = 0, first_frames = 0;
    int fsize, frames;
    cdx_uint8 *buf0 = (cdx_uint8 *)probeData->buf;
    cdx_uint8 *buf1;
    cdx_uint8 *buf;
    const cdx_uint8 *end = buf0 + probeData->len - 7;

    for (buf = buf0; buf < end; buf = buf1 + 1)
    {
        buf1 = buf;

        for (frames = 0; buf1 < end; frames++)
        {
            if (!AacProbe(buf1))
            {
                if (buf != buf0)
                {
                    // Found something that isn't an ADTS/LATM/ADIF header, starting
                    // from a position other than the start of the buffer.
                    // Discard the count we've accumulated so far since it
                    // probably was a false positive.
                    frames = 0;
                }
                break;
            }
            fsize = (GetBE32(buf1 + 3) >> 13) & 0x1FFF;
            if (fsize < 7)
                break;
            fsize = fsize < end - buf1 ? fsize : end - buf1;
            buf1 += fsize;
        }
        max_frames = max_frames > frames ? max_frames : frames;
        if (buf == buf0)
            first_frames = frames;
    }

    int score = 0;
    if (first_frames >= 3)
        score = 100;
    else if (max_frames > 100)
        score = 50;
    else if (max_frames >= 3)
        score = 25;
    else if (max_frames >= 1)
        score = 1;

    CDX_LOGD("aac probe score %d", score);

    if(score < 25)//unreliable... kill it
    {
        score = 0;
    }
    return score;
}

static CdxParserT *__AacParserOpen(CdxStreamT *stream, cdx_uint32 flags)
{
    (void)flags;
    //cdx_int32 ret = 0;
    struct AacParserImplS *impl;
    impl = CdxMalloc(sizeof(*impl));

    memset(impl, 0x00, sizeof(*impl));
    impl->stream = stream;
    impl->base.ops = &aacParserOps;
    pthread_cond_init(&impl->cond, NULL);
    //ret = pthread_create(&impl->openTid, NULL, AacOpenThread, (void*)impl);
    //CDX_FORCE_CHECK(!ret);
    impl->mErrno = PSR_INVALID;
    return &impl->base;
}

struct CdxParserCreatorS aacParserCtor =
{
    .probe = __AacParserProbe,
    .create  = __AacParserOpen
};
