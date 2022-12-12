/*******************************************************************************
--                                                                            --
--                    CedarX Multimedia Framework                             --
--                                                                            --
--          the Multimedia Framework for Linux/Android System                 --
--                                                                            --
--       This software is confidential and proprietary and may be used        --
--        only as expressly authorized by a licensing agreement from          --
--                         Softwinner Products.                               --
--                                                                            --
--                   (C) COPYRIGHT 2011 SOFTWINNER PRODUCTS                   --
--                            ALL RIGHTS RESERVED                             --
--                                                                            --
--                 The entire notice above must be reproduced                 --
--                  on all copies and should not be removed.                  --
--                                                                            --
*******************************************************************************/
//#define LOG_NDEBUG 0
#define LOG_TAG "tmessage"
#include <utils/plat_log.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "tmessage.h"
#include "SystemBase.h"
#include <cdx_list.h>

static int TMessageDeepCopyMessage(message_t *pDesMsg, message_t *pSrcMsg)
{
    pDesMsg->command = pSrcMsg->command;
    pDesMsg->para0 = pSrcMsg->para0;
    pDesMsg->para1 = pSrcMsg->para1;
    pDesMsg->mpData = NULL;
    pDesMsg->mDataSize = 0;
    if(pSrcMsg->mpData && pSrcMsg->mDataSize>=0)
    {
        pDesMsg->mpData = malloc(pSrcMsg->mDataSize);
        if(pDesMsg->mpData)
        {
            pDesMsg->mDataSize = pSrcMsg->mDataSize;
            memcpy(pDesMsg->mpData, pSrcMsg->mpData, pSrcMsg->mDataSize);
        }
        else
        {
            aloge(" fatal error! malloc MessageData fail!");
            return -1;
        }
    }
    pDesMsg->pReply = pSrcMsg->pReply;
    return 0;
}

static int TMessageSetMessage(message_t *pDesMsg, message_t *pSrcMsg)
{
    pDesMsg->command = pSrcMsg->command;
    pDesMsg->para0 = pSrcMsg->para0;
    pDesMsg->para1 = pSrcMsg->para1;
    pDesMsg->mpData = pSrcMsg->mpData;
    pDesMsg->mDataSize = pSrcMsg->mDataSize;
    pDesMsg->pReply = pSrcMsg->pReply;
    return 0;
}

static int TMessageIncreaseIdleMessageList(message_queue_t* pThiz)
{
    int ret = 0;
    message_t   *pMsg;
    int i;
    for(i=0;i<MAX_MESSAGE_ELEMENTS;i++)
    {
        pMsg = (message_t*)malloc(sizeof(message_t));
        if(NULL == pMsg)
        {
            aloge(" fatal error! malloc fail");
            ret = -1;
            break;
        }
        list_add_tail(&pMsg->mList, &pThiz->mIdleMessageList);
    }
    return ret;
}

MessageReply *ConstructMessageReply()
{
    MessageReply *pReply = (MessageReply*)malloc(sizeof(MessageReply));
    if(NULL == pReply)
    {
        aloge("fatal error! malloc fail.");
    }
    memset(pReply, 0, sizeof(MessageReply));
    int ret = cdx_sem_init(&pReply->ReplySem, 0);
    if(ret != 0)
    {
        aloge("fatal error! cdx sem ini fail:%d", ret);
    }
    return pReply;
}
void DestructMessageReply(MessageReply *pReply)
{
    if(pReply)
    {
        if(pReply->pReplyExtra)
        {
            free(pReply->pReplyExtra);
            pReply->pReplyExtra = NULL;
            pReply->nReplyExtraSize = 0;
        }
        cdx_sem_deinit(&pReply->ReplySem);
    }
    else
    {
        aloge("fatal error! reply is null");
    }
}

int InitMessage(message_t *pMsg)
{
    memset(pMsg, 0, sizeof(message_t));
    return 0;
}

int message_create(message_queue_t* msg_queue)
{
	//int 		i;
    int         ret;
  
	alogv("msg create\n");
    
	ret = pthread_mutex_init(&msg_queue->mutex, NULL);
	if (ret!=0)
	{
		return -1;
	}
    pthread_condattr_t  condAttr;
    pthread_condattr_init(&condAttr);
//#if defined(__LP64__)
    pthread_condattr_setclock(&condAttr, CLOCK_MONOTONIC);
//#endif
    ret = pthread_cond_init(&msg_queue->mCondMessageQueueChanged, &condAttr);
    if(ret!=0)
    {
        aloge("[%s] fatal error! pthread cond init fail", strrchr(__FILE__, '/')+1);
        goto _err0;
    }
    msg_queue->mWaitMessageFlag = 0;
    //INIT_LIST_HEAD(&msg_queue->mMessageBufList);
    INIT_LIST_HEAD(&msg_queue->mIdleMessageList);
    INIT_LIST_HEAD(&msg_queue->mReadyMessageList);
    if(0!=TMessageIncreaseIdleMessageList(msg_queue))
    {
        goto _err1;
    }
	msg_queue->message_count = 0;
    
	return 0;
_err1:
    pthread_cond_destroy(&msg_queue->mCondMessageQueueChanged);
_err0:
    pthread_mutex_destroy(&msg_queue->mutex);
    return -1;
}


void message_destroy(message_queue_t* msg_queue)
{
	pthread_mutex_lock(&msg_queue->mutex);
    if(!list_empty(&msg_queue->mReadyMessageList))
    {
        message_t *pEntry, *pTmp;
        list_for_each_entry_safe(pEntry, pTmp, &msg_queue->mReadyMessageList, mList)
        {
            alogd(" msg destroy: cmd[%x]mpData[%p]size[%d]", pEntry->command, pEntry->mpData, pEntry->mDataSize);
            if(pEntry->mpData)
            {
                free(pEntry->mpData);
                pEntry->mpData = NULL;
            }
            pEntry->mDataSize = 0;
            list_move_tail(&pEntry->mList, &msg_queue->mIdleMessageList);
            msg_queue->message_count--;
        }
    }
    if(msg_queue->message_count != 0)
    {
        aloge(" fatal error! msg count[%d]!=0", msg_queue->message_count);
    }
    int cnt = 0;
    if(!list_empty(&msg_queue->mIdleMessageList))
    {
        message_t   *pEntry, *pTmp;
        list_for_each_entry_safe(pEntry, pTmp, &msg_queue->mIdleMessageList, mList)
        {
            list_del(&pEntry->mList);
            free(pEntry);
            cnt++;
        }
    }
    INIT_LIST_HEAD(&msg_queue->mIdleMessageList);
    INIT_LIST_HEAD(&msg_queue->mReadyMessageList);
	pthread_mutex_unlock(&msg_queue->mutex);
    pthread_cond_destroy(&msg_queue->mCondMessageQueueChanged);
	pthread_mutex_destroy(&msg_queue->mutex);
}


void flush_message(message_queue_t* msg_queue)
{
	pthread_mutex_lock(&msg_queue->mutex);
    if(!list_empty(&msg_queue->mReadyMessageList))
    {
        message_t   *pEntry, *pTmp;
        list_for_each_entry_safe(pEntry, pTmp, &msg_queue->mReadyMessageList, mList)
        {
            alogd(" msg destroy: cmd[%x]mpData[%p]size[%d]", pEntry->command, pEntry->mpData, pEntry->mDataSize);
            if(pEntry->mpData)
            {
                free(pEntry->mpData);
                pEntry->mpData = NULL;
            }
            pEntry->mDataSize = 0;
            list_move_tail(&pEntry->mList, &msg_queue->mIdleMessageList);
            msg_queue->message_count--;
        }
    }
    if(msg_queue->message_count != 0)
    {
        aloge(" fatal error! msg count[%d]!=0", msg_queue->message_count);
    }
	pthread_mutex_unlock(&msg_queue->mutex);
}

/*******************************************************************************
Function name: put_message
Description: 
    Do not accept mpData when call this function.
Parameters: 
    
Return: 
    
Time: 2015/3/6
*******************************************************************************/
int put_message(message_queue_t* msg_queue, message_t *msg_in)
{
    message_t message;
    memset(&message, 0, sizeof(message_t));
    message.command = msg_in->command;
    message.para0 = msg_in->para0;
    message.para1 = msg_in->para1;
    message.mpData = NULL;
    message.mDataSize = 0;
    return putMessageWithData(msg_queue, &message);
}

int get_message(message_queue_t* msg_queue, message_t *msg_out)
{
	pthread_mutex_lock(&msg_queue->mutex);

    if(list_empty(&msg_queue->mReadyMessageList))
    {
        pthread_mutex_unlock(&msg_queue->mutex);
        return -1;
    }
    message_t   *pMessageEntry = list_first_entry(&msg_queue->mReadyMessageList, message_t, mList);
    TMessageSetMessage(msg_out, pMessageEntry);
    list_move_tail(&pMessageEntry->mList, &msg_queue->mIdleMessageList);
    msg_queue->message_count--;

    pthread_mutex_unlock(&msg_queue->mutex);
	return 0;
}

int putMessageWithData(message_queue_t* msg_queue, message_t *msg_in)
{
    int ret = 0;
	pthread_mutex_lock(&msg_queue->mutex);
    if(list_empty(&msg_queue->mIdleMessageList))
    {
        alogw(" idleMessageList are all used, malloc more!");
        //dumpCallStack("TMsg");
        if(0!=TMessageIncreaseIdleMessageList(msg_queue))
        {
            pthread_mutex_unlock(&msg_queue->mutex);
            return -1;
        }
    }
    message_t   *pMessageEntry = list_first_entry(&msg_queue->mIdleMessageList, message_t, mList);
    if(0==TMessageDeepCopyMessage(pMessageEntry, msg_in))
    {
        list_move_tail(&pMessageEntry->mList, &msg_queue->mReadyMessageList);
        msg_queue->message_count++;
        alogv(" new msg command[%d], para[%d][%d] pData[%p]size[%d]", 
            pMessageEntry->command, pMessageEntry->para0, pMessageEntry->para1, pMessageEntry->mpData, pMessageEntry->mDataSize);
        if(msg_queue->mWaitMessageFlag)
        {
            pthread_cond_signal(&msg_queue->mCondMessageQueueChanged);
        }
    }
    else
    {
        ret = -1;
    }
    pthread_mutex_unlock(&msg_queue->mutex);
	return ret;
}

int get_message_count(message_queue_t* msg_queue)
{
	int message_count;

	pthread_mutex_lock(&msg_queue->mutex);
	message_count = msg_queue->message_count;
	pthread_mutex_unlock(&msg_queue->mutex);

	return message_count;
}

int TMessage_WaitQueueNotEmpty(message_queue_t* msg_queue, unsigned int timeout)
{
    int message_count;

    pthread_mutex_lock(&msg_queue->mutex);
    msg_queue->mWaitMessageFlag = 1;
    if(timeout <= 0)
    {
        while(list_empty(&msg_queue->mReadyMessageList))
        {
            pthread_cond_wait(&msg_queue->mCondMessageQueueChanged, &msg_queue->mutex);
        }
    }
    else
    {
        if(list_empty(&msg_queue->mReadyMessageList))
        {
            int ret = pthread_cond_wait_timeout(&msg_queue->mCondMessageQueueChanged, &msg_queue->mutex, timeout);
            if(ETIMEDOUT == ret)
            {
                //alogd(" pthread cond timeout np timeout[%d]", ret);
            }
            else if(0 == ret)
            {
            }
            else
            {
                aloge(" fatal error! pthread cond timeout np[%d]", ret);
            }
        }
    }
    msg_queue->mWaitMessageFlag = 0;
    message_count = msg_queue->message_count;
    pthread_mutex_unlock(&msg_queue->mutex);

    return message_count;
}

