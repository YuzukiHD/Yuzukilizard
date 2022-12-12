/******************************************************************************
  Copyright (C), 2001-2018, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : VideoDec_InputThread.h
  Version       : Initial Draft
  Author        : Allwinner PDC-PD5 Team
  Created       : 2018/12/06
  Last Modified :
  Description   : mpp component implement
  Function List :
  History       :
******************************************************************************/
#ifndef _VIDEODEC_INPUTTHREAD_H_
#define _VIDEODEC_INPUTTHREAD_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct VIDEODEC_INPUT_DATA
{
    COMP_STATETYPE state;
    pthread_mutex_t mStateLock;
    pthread_cond_t mStateCond;
    
    int nRequestLen; //Set when not enough buffer for component A

    struct list_head mIdleVbsList;  //DMXPKT_NODE_T. used in COMP_BufferSupplyOutput, COMP_BufferSupplyInput
    struct list_head mReadyVbsList; //used in COMP_BufferSupplyOutput
    struct list_head mUsingVbsList; //used in COMP_BufferSupplyInput
    struct list_head mUsedVbsList;  //not used now.
    int mNodeNum;
    pthread_mutex_t mVbsListLock;
    volatile BOOL mWaitVbsValidFlag;    //for COMP_BufferSupplyInput
    BOOL mbWaitVbsFullFlag;             //for COMP_BufferSupplyInput
    pthread_cond_t mVbsFullCond;        //for COMP_BufferSupplyInput

    BOOL mbWaitUsedVbsFlag;  //for COMP_BufferSupplyOutput    
    
    void *pVideoDecData; //struct VIDEODECDATATYPE*
    message_queue_t cmd_queue;
    pthread_t thread_id;
}VIDEODEC_INPUT_DATA;

ERRORTYPE VideoDec_InputDataInit(VIDEODEC_INPUT_DATA *pInputData, void *pVideoDecData/*VIDEODECDATATYPE*/);
ERRORTYPE VideoDec_InputDataDestroy(VIDEODEC_INPUT_DATA *pInputData);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif  /* _VIDEODEC_INPUTTHREAD_H_ */

