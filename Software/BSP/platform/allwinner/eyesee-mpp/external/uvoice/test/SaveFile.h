#ifndef _SAVEFILE_H_
#define _SAVEFILE_H_

#include <pthread.h>

#include <plat_type.h>
#include <tsemaphore.h>
#include <tmessage.h>

#define SAVE_FILE_MAX_FILE_PATH_SIZE (256)

typedef struct
{
    char mFilePath[SAVE_FILE_MAX_FILE_PATH_SIZE];
    unsigned int mSampleRate;
    unsigned int mChannelCount;
    unsigned int mSampleBitsPerChannel;

    pthread_mutex_t mBufferLock;
    pthread_cond_t mCondFreeBufReady;
    int mbWaitFreeBufReady; //mainThread set it
    int mbWaitInputData;    //writeFileThread set it.
    char *mpPcmBuffer;
    char *mpBufferReadPtr;
    char *mpBufferWritePtr;
    int mBufferValidLen;
    int mBufferSize;    //unit:byte

    unsigned int mPcmSize;   //unit: byte
    FILE *mFpWavFile;
    int mbWavFlag;  //1:save wav file; 0:save pcm file.
    unsigned int mWritePcmSize;   //unit: byte

    pthread_t mWriteFileThreadId;
    message_queue_t  mMessageQueue;
    int mbFinishFile;

    int (*SendPcmData)(void *pThiz, char *pData, int nSize, int nTimeOut);  //unit:ms
}SaveWavFile;

SaveWavFile* constructSaveWavFile(char *pFilePath, int nBufferSize, int bWavFlag, 
                unsigned int nSampleRate, unsigned int nChannelCount, unsigned int nSampleBitsPerChannel);
void destructSaveWavFile(SaveWavFile *pThiz);

#endif  /* _SAVEFILE_H_ */

