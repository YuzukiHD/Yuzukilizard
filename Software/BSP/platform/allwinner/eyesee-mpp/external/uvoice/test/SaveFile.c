//#define LOG_NDEBUG 0
#define LOG_TAG "SaveWavFile"
#include <utils/plat_log.h>

#include <pthread.h>
#include <string.h>
#include <errno.h>

#include <confparser.h>
#include <SystemBase.h>
#include "SaveFile.h"

typedef enum
{
    SaveWavFile_Stop = 0,
    SaveWavFile_InputData,
} SaveWavFileMessage;

static void PcmDataAddWaveHeader(SaveWavFile *pContext)
{
    struct WaveHeader{
        int riff_id;
        int riff_sz;
        int riff_fmt;
        int fmt_id;
        int fmt_sz;
        short audio_fmt;
        short num_chn;
        int sample_rate;
        int byte_rate;
        short block_align;
        short bits_per_sample;
        int data_id;
        int data_sz;
    } header;

    memcpy(&header.riff_id, "RIFF", 4);
    header.riff_sz = pContext->mWritePcmSize + sizeof(struct WaveHeader) - 8;
    memcpy(&header.riff_fmt, "WAVE", 4);
    memcpy(&header.fmt_id, "fmt ", 4);
    header.fmt_sz = 16;
    header.audio_fmt = 1;       // s16le
    header.num_chn = pContext->mChannelCount;
    header.sample_rate = pContext->mSampleRate;
    header.byte_rate = pContext->mSampleRate * pContext->mChannelCount * pContext->mSampleBitsPerChannel/8;
    header.block_align = pContext->mChannelCount * pContext->mSampleBitsPerChannel/8;
    header.bits_per_sample = pContext->mSampleBitsPerChannel;
    memcpy(&header.data_id, "data", 4);
    header.data_sz = pContext->mWritePcmSize;

    fseek(pContext->mFpWavFile, 0, SEEK_SET);
    fwrite(&header, 1, sizeof(struct WaveHeader), pContext->mFpWavFile);
}

static int SaveWavFile_WriteLastData(SaveWavFile *pContext)
{
    if(pContext->mBufferValidLen > 0)
    {
        char *pReadPtr0, *pReadPtr1;
        int nReadLen0, nReadLen1;
        if(pContext->mpBufferReadPtr + pContext->mBufferValidLen > pContext->mpPcmBuffer + pContext->mBufferSize)
        {
            pReadPtr0 = pContext->mpBufferReadPtr;
            nReadLen0 = pContext->mpPcmBuffer + pContext->mBufferSize - pContext->mpBufferReadPtr;
            pReadPtr1 = pContext->mpPcmBuffer;
            nReadLen1 = pContext->mBufferValidLen - nReadLen0;
        }
        else
        {
            pReadPtr0 = pContext->mpBufferReadPtr;
            nReadLen0 = pContext->mBufferValidLen;
            pReadPtr1 = NULL;
            nReadLen1 = 0;
        }
        if(pReadPtr0 != NULL)
        {
            int nWriteLen = fwrite(pReadPtr0, 1, nReadLen0, pContext->mFpWavFile);
            if(nWriteLen != nReadLen0)
            {
                aloge("fatal error! fwrite[%d]bytes_0, need write[%d]bytes!", nWriteLen, nReadLen0);
            }
            pContext->mWritePcmSize += nReadLen0;
        }
        if(pReadPtr1 != NULL)
        {
            int nWriteLen = fwrite(pReadPtr1, 1, nReadLen1, pContext->mFpWavFile);
            if(nWriteLen != nReadLen1)
            {
                aloge("fatal error! fwrite[%d]bytes_1, need write[%d]bytes!", nWriteLen, nReadLen1);
            }
            pContext->mWritePcmSize += nReadLen1;
        }
        pContext->mpBufferReadPtr += (nReadLen0+nReadLen1);
        if(pContext->mpBufferReadPtr >= pContext->mpPcmBuffer+pContext->mBufferSize)
        {
            pContext->mpBufferReadPtr = pContext->mpPcmBuffer+nReadLen1;
        }
        pContext->mBufferValidLen -= (nReadLen0+nReadLen1);
        alogd("write last [%d]bytes to file, left[%d]bytes!", nReadLen0+nReadLen1, pContext->mBufferValidLen);
    }
    return 0;
}

static void* SaveWavFileThread(void *pThreadData)
{
    SaveWavFile *pContext = (SaveWavFile*)pThreadData;
    message_t stMsg;

    while(1)
    {
    PROCESS_MESSAGE:
        if(get_message(&pContext->mMessageQueue, &stMsg) == 0) 
        {
            alogv("SaveWavFile Thread get message cmd:%d", stMsg.command);
            if (SaveWavFile_Stop == stMsg.command)
            {
                //write last data to file.
                pthread_mutex_lock(&pContext->mBufferLock);
                SaveWavFile_WriteLastData(pContext);
                if(pContext->mbWavFlag)
                {
                    PcmDataAddWaveHeader(pContext);
                }
                pContext->mbFinishFile = 1;
                if(pContext->mbWaitFreeBufReady)
                {
                    pContext->mbWaitFreeBufReady = 0;
                    pthread_cond_signal(&pContext->mCondFreeBufReady);
                }
                pthread_mutex_unlock(&pContext->mBufferLock);
                // Kill thread
                goto _exit;
            }
            else if(SaveWavFile_InputData == stMsg.command)
            {
                //alogd("data input");
            }
            else
            {
                aloge("fatal error! unknown cmd[0x%x]", stMsg.command);
            }
            //precede to process message
            goto PROCESS_MESSAGE;
        }
        pthread_mutex_lock(&pContext->mBufferLock);
        if(pContext->mBufferValidLen > 0)
        {
            char *pReadPtr0, *pReadPtr1;
            int nReadLen0, nReadLen1;
            if(pContext->mpBufferReadPtr + pContext->mBufferValidLen > pContext->mpPcmBuffer + pContext->mBufferSize)
            {
                pReadPtr0 = pContext->mpBufferReadPtr;
                nReadLen0 = pContext->mpPcmBuffer + pContext->mBufferSize - pContext->mpBufferReadPtr;
                pReadPtr1 = pContext->mpPcmBuffer;
                nReadLen1 = pContext->mBufferValidLen - nReadLen0;
            }
            else
            {
                pReadPtr0 = pContext->mpBufferReadPtr;
                nReadLen0 = pContext->mBufferValidLen;
                pReadPtr1 = NULL;
                nReadLen1 = 0;
            }
            pthread_mutex_unlock(&pContext->mBufferLock);
            //fwrite must out of lock scope, to avoid mainThread waiting to write buffer!
            if(pReadPtr0 != NULL)
            {
                int nWriteLen = fwrite(pReadPtr0, 1, nReadLen0, pContext->mFpWavFile);
                if(nWriteLen != nReadLen0)
                {
                    aloge("fatal error! fwrite[%d]bytes_0, need write[%d]bytes!", nWriteLen, nReadLen0);
                }
                pContext->mWritePcmSize += nReadLen0;
            }
            if(pReadPtr1 != NULL)
            {
                int nWriteLen = fwrite(pReadPtr1, 1, nReadLen1, pContext->mFpWavFile);
                if(nWriteLen != nReadLen1)
                {
                    aloge("fatal error! fwrite[%d]bytes_1, need write[%d]bytes!", nWriteLen, nReadLen1);
                }
                pContext->mWritePcmSize += nReadLen1;
            }
            pthread_mutex_lock(&pContext->mBufferLock);
            //note: pContext->mBufferValidLen may be changed by mainThread!
            pContext->mpBufferReadPtr += (nReadLen0+nReadLen1);
            if(pContext->mpBufferReadPtr >= pContext->mpPcmBuffer+pContext->mBufferSize)
            {
                pContext->mpBufferReadPtr = pContext->mpPcmBuffer+nReadLen1;
            }
            pContext->mBufferValidLen -= (nReadLen0+nReadLen1);
            
            if(pContext->mbWaitFreeBufReady)
            {
                pContext->mbWaitFreeBufReady = 0;
                pthread_cond_signal(&pContext->mCondFreeBufReady);
            }
            pthread_mutex_unlock(&pContext->mBufferLock);
        }
        else
        {
            pContext->mbWaitInputData = 1;
            pthread_mutex_unlock(&pContext->mBufferLock);
            TMessage_WaitQueueNotEmpty(&pContext->mMessageQueue, 0);
            goto PROCESS_MESSAGE;
        }
    }
_exit:
    return (void*)0;
}

static int SaveWavFile_SendPcmData(void *pThiz, char *pData, int nSize, int nTimeOut)
{
    int result = 0;
    int bExit = 0;
    SaveWavFile *pContext = (SaveWavFile*)pThiz;
    
    pthread_mutex_lock(&pContext->mBufferLock);
    if(pContext->mbFinishFile)
    {
        alogd("wav file has finished! Don't accept data now!");
        pthread_mutex_unlock(&pContext->mBufferLock);
        return -1;
    }
    while(nSize > pContext->mBufferSize - pContext->mBufferValidLen)
    {
        if(0 == nTimeOut)
        {
            alogd("buffer full? return!");
            result = -1;
            bExit = 1;
            break;
        }
        else if(nTimeOut < 0)
        {
            alogd("buffer full? wait!");
            pContext->mbWaitFreeBufReady = 1;
            pthread_cond_wait(&pContext->mCondFreeBufReady, &pContext->mBufferLock);
            pContext->mbWaitFreeBufReady = 0;
            continue;
        }
        else
        {
            alogd("buffer full? wait [%d]ms!", nTimeOut);
            pContext->mbWaitFreeBufReady = 1;
            int ret = pthread_cond_wait_timeout(&pContext->mCondFreeBufReady, &pContext->mBufferLock, nTimeOut);
            pContext->mbWaitFreeBufReady = 0;
            if(ETIMEDOUT == ret)
            {
                alogd("wait free buffer timeout[%d]ms, ret[%d]", nTimeOut, ret);
                result = ETIMEDOUT;
                bExit = 1;
                break;
            }
            else if(0 == ret)
            {
                continue;
            }
            else
            {
                aloge("fatal error! pthread cond wait timeout ret[%d]", ret);
                result = -1;
                bExit = 1;
                break;
            }
        }
    }
    if(bExit)
    {
        pthread_mutex_unlock(&pContext->mBufferLock);
        return result;
    }

    if(nSize <= pContext->mBufferSize - pContext->mBufferValidLen)
    {
        if(pContext->mpBufferWritePtr >= pContext->mpBufferReadPtr)
        {
            int nFreeLen0 = pContext->mpPcmBuffer + pContext->mBufferSize - pContext->mpBufferWritePtr;
            int nFreeLen1 = pContext->mpBufferReadPtr - pContext->mpPcmBuffer;
            int nDataLen0 = 0;
            int nDataLen1 = 0;
            if(nFreeLen0 >= nSize)
            {
                nDataLen0 = nSize;
                nDataLen1 = 0;
            }
            else
            {
                nDataLen0 = nFreeLen0;
                nDataLen1 = nSize - nFreeLen0;
            }
            memcpy(pContext->mpBufferWritePtr, pData, nDataLen0);
            pContext->mpBufferWritePtr += nDataLen0;
            if(pContext->mpBufferWritePtr >= pContext->mpPcmBuffer + pContext->mBufferSize)
            {
                if(pContext->mpBufferWritePtr != pContext->mpPcmBuffer + pContext->mBufferSize)
                {
                    aloge("fatal error! buffer ptr wrong [%p]!=[%p]+[%d]!", pContext->mpBufferWritePtr, pContext->mpPcmBuffer, pContext->mBufferSize);
                }
                pContext->mpBufferWritePtr = pContext->mpPcmBuffer;
            }
            if(nDataLen1 > 0)
            {
                memcpy(pContext->mpPcmBuffer, pData+nDataLen0, nDataLen1);
                pContext->mpBufferWritePtr += nDataLen1;
            }
        }
        else
        {
            memcpy(pContext->mpBufferWritePtr, pData, nSize);
            pContext->mpBufferWritePtr += nSize;
            if(pContext->mpBufferWritePtr >= pContext->mpPcmBuffer + pContext->mBufferSize)
            {
                aloge("fatal error! buffer ptr wrong [%p]!=[%p]+[%d]!", pContext->mpBufferWritePtr, pContext->mpPcmBuffer, pContext->mBufferSize);
            }
        }
        pContext->mBufferValidLen += nSize;
        pContext->mPcmSize += nSize;
        if(pContext->mbWaitInputData!=0)
        {
            pContext->mbWaitInputData = 0;
            message_t msg;
            msg.command = SaveWavFile_InputData;
            put_message(&pContext->mMessageQueue, &msg);
        }
    }
    else
    {
        aloge("fatal error! impossible! check code!");
        result = -1;
    }
    pthread_mutex_unlock(&pContext->mBufferLock);
    return result;
}

SaveWavFile* constructSaveWavFile(char *pFilePath, int nBufferSize, int bWavFlag, 
                unsigned int nSampleRate, unsigned int nChannelCount, unsigned int nSampleBitsPerChannel)
{
    int ret;
    SaveWavFile *pContext = (SaveWavFile*)malloc(sizeof(SaveWavFile));
    memset(pContext, 0, sizeof(SaveWavFile));
    strncpy(pContext->mFilePath, pFilePath, SAVE_FILE_MAX_FILE_PATH_SIZE);
    pContext->mSampleRate = nSampleRate;
    pContext->mChannelCount = nChannelCount;
    pContext->mSampleBitsPerChannel = nSampleBitsPerChannel;
    ret = pthread_mutex_init(&pContext->mBufferLock, NULL);
    if(ret != 0)
    {
        aloge("fatal error! pthread mutex init fail[0x%x]", ret);
    }
    pthread_condattr_t  condAttr;
    pthread_condattr_init(&condAttr);
    pthread_condattr_setclock(&condAttr, CLOCK_MONOTONIC);
    ret = pthread_cond_init(&pContext->mCondFreeBufReady, &condAttr);
    if(ret != 0)
    {
        aloge("fatal error! pthread cond init fail[0x%x]", ret);
    }
    pContext->mpPcmBuffer = (char*)malloc(nBufferSize);
    if(NULL == pContext->mpPcmBuffer)
    {
        aloge("fatal error! malloc [%d]bytes failed", nBufferSize);
    }
    memset(pContext->mpPcmBuffer, 0, pContext->mBufferSize);
    pContext->mpBufferReadPtr = pContext->mpPcmBuffer;
    pContext->mpBufferWritePtr = pContext->mpPcmBuffer;
    pContext->mBufferValidLen = 0;
    pContext->mBufferSize = nBufferSize;
    pContext->mbWavFlag = bWavFlag;
    pContext->mFpWavFile = fopen(pContext->mFilePath, "wb");
    if(pContext->mFpWavFile != NULL)
    {
        alogd("open file: %s", pContext->mFilePath);
        if(pContext->mbWavFlag)
        {
            fseek(pContext->mFpWavFile, 44, SEEK_SET);  // 44: size(WavHeader)
        }
    }
    else
    {
        aloge("fatal error! can't open pcm file[%s]", pContext->mFilePath);
    }
    ret = message_create(&pContext->mMessageQueue);
    if(ret != 0)
    {
        aloge("fatal error! message create fail[0x%x]", ret);
    }
    //create writeFileThread.
    ret = pthread_create(&pContext->mWriteFileThreadId, NULL, SaveWavFileThread, pContext);
    if(ret != 0)
    {
        aloge("fatal error! create thread fail![%d], threadId[0x%x]", ret, pContext->mWriteFileThreadId);
    }
    else
    {
        alogd("create SaveWavFile Thread[0x%x]", pContext->mWriteFileThreadId);
    }
    pContext->SendPcmData = SaveWavFile_SendPcmData;
    return pContext;
}

void destructSaveWavFile(SaveWavFile *pContext)
{
    message_t msg;
    msg.command = SaveWavFile_Stop;
    put_message(&pContext->mMessageQueue, &msg);
    void *pRet = NULL;
    pthread_join(pContext->mWriteFileThreadId, (void**)&pRet);
    alogd("join writeFileThreadId[%#x], ret[0x%x]", pContext->mWriteFileThreadId, pRet);
    pContext->mWriteFileThreadId = -1;

    if(pContext->mpPcmBuffer)
    {
        free(pContext->mpPcmBuffer);
        pContext->mpPcmBuffer = NULL;
    }
    pthread_mutex_destroy(&pContext->mBufferLock);
    pthread_cond_destroy(&pContext->mCondFreeBufReady);

    if(pContext->mFpWavFile)
    {
        fclose(pContext->mFpWavFile);
        pContext->mFpWavFile = NULL;
    }
    message_destroy(&pContext->mMessageQueue);
}

