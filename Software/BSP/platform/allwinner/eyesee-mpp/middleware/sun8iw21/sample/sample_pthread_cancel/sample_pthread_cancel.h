#ifndef _SAMPLE_PTHREAD_CANCEL_H_
#define _SAMPLE_PTHREAD_CANCEL_H_

#include <pthread.h>

typedef struct SamplePthreadCancelContext
{
    pthread_t mDetectInputThreadId;
}SamplePthreadCancelContext;
SamplePthreadCancelContext* constructSamplePthreadCancelContext();
void destructSamplePthreadCancelContext(SamplePthreadCancelContext *pContext);

#endif  /* _SAMPLE_PTHREAD_CANCEL_H_ */

