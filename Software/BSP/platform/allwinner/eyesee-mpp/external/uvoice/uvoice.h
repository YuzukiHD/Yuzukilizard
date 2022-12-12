#ifndef _UVOICE_H_
#define _UVOICE_H_

typedef enum {
    uvoiceCmd_XiaozhiStartRecord = 0,
    uvoiceCmd_XiaozhiStopRecord = 1,
    uvoiceCmd_XiaozhiCapture = 2,
    uvoiceCmd_XiaozhiContinueCapture = 3,
    uvoiceCmd_XiaozhiShutdown = 4,
} uvoiceCommand;
typedef struct uvoiceResult {
    //uvoice corp define these member.
    uvoiceCommand eCmd;
} uvoiceResult;

typedef struct uvoiceCallbackInfo {
    void *cookie;
    int (*callback)(void *cookie, uvoiceResult *pResultData);
} uvoiceCallbackInfo;

typedef struct
{
    unsigned int mSampleRate;
    unsigned int mChannels;
    unsigned int mSampleBitsPerChannel;
}PcmConfig;

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */
int uvoice_RegisterCallback(void *puvoice, uvoiceCallbackInfo *pCallback);

void* constructUvoiceInstance(PcmConfig *pConfig);
void destructUvoiceInstance(void *puvoice);

/**
 * must send data of 160ms, which is uvoice corp's rule.
 */
int uvoice_SendData(void *puvoice, char *pData, int nSize);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */


#endif  /* _UVOICE_H_ */

