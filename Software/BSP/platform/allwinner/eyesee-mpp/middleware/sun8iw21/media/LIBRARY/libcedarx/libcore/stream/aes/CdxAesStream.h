/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxAesStream.h
 * Description : AesStream
 * History :
 *
 */

#ifndef AES_STREAM_H
#define AES_STREAM_H
#include <pthread.h>
#include <CdxTypes.h>
#include <CdxStream.h>
#include <CdxAtomic.h>
#include <openssl/aes.h>
#include <stdlib.h>
#include <ctype.h>

const char AES_128_KEY[33] = "7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f";
const char AES_128_IV[33] = "7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f";

inline void text2Value(const char *in, cdx_uint8 *out)
{
    int i;
    cdx_uint8 high, low;
    for (i = 0; i < 16; i++)
    {
        high = toupper(in[2 * i]);
        low = toupper(in[2 * i + 1]);

        if (!isxdigit(high) || !isxdigit(low))
            abort();

        if (high >= '0' && high <= '9')
            high -= '0';
        else
            high += 10 - 'A';

        if (low >= '0' && low <= '9')
            low -= '0';
        else
            low += 10 - 'A';

        out[i] = high * 16 + low;
    }
}

void getAes128Key(cdx_uint8 *buf)
{
    text2Value(AES_128_KEY, buf);
}

void getAes128Iv(cdx_uint8 *buf)
{
    text2Value(AES_128_IV, buf);
}

enum CdxStreamStatus
{
    STREAM_IDLE,
    STREAM_CONNECTING,
    STREAM_SEEKING,
    STREAM_READING,
};

typedef struct {
    CdxStreamT base;
    enum CdxStreamStatus status;
    cdx_int32 ioState;
    pthread_mutex_t lock;
    pthread_cond_t cond;

    int forceStop;
    CdxStreamProbeDataT probeData;
    pthread_t threadId;/*UdpDownloadThread*/

    cdx_uint8 *bigBuf;
    cdx_uint32 bufSize;

    CdxDataSourceT dataSource;
    CdxStreamT *mediaFile;
    cdx_int64 mediaFileSize;
    cdx_char *mediaUri;
    cdx_uint8 iv[16];
    cdx_uint8 originalIv[16];
    cdx_uint8 key[16];
    AES_KEY aesKey;
    enum PaddingType paddingType;

    int downloadComplete;
    cdx_uint32 remainingSize;
    cdx_uint32 remainingOffset;
    cdx_uint8 remaining[16];

}CdxAesStream;

#endif
