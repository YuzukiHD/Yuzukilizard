#ifndef __MPP_HELPER_H__
#define __MPP_HELPER_H__

#include <stdio.h>
#include <plat_type.h>

// TODO we will remove type of venc
#include <mpi_venc.h>
// end TODO

#define _CHECK_RET( ret )  do {\
    if( SUCCESS != ret ) {\
        printf("Error: %s: %s at %d\n", __FILE__, __FUNCTION__, __LINE__);\
        return ret;\
    }\
} while(0)

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

ERRORTYPE MPP_HELPER_INIT();
ERRORTYPE MPP_HELPER_UNINIT();

typedef int (*preview_callback_t)(unsigned char* yBuffer, unsigned char* uBuffer, unsigned char* vBuffer);
ERRORTYPE MPP_HELPER_START_PREVIEW();
ERRORTYPE MPP_HELPER_STOP_PREVIEW();
BOOL MPP_HELPER_DO_PREVIEW(preview_callback_t preview_callback);

typedef int (*venc_record_callback_t)(unsigned char *addr, unsigned int len);
typedef int (*venc_motion_callback_t)(VencMotionSearchResult motionResult);
ERRORTYPE MPP_HELPER_START_RECORD(venc_record_callback_t record_callback);
ERRORTYPE MPP_HELPER_STOP_RECORD();
ERRORTYPE MPP_HELPER_DO_RECORD(venc_record_callback_t record_callback, venc_motion_callback_t motion_callback);

typedef int (*npu_callback_t)(unsigned char* yBuffer, unsigned char* uvBuffer, void *phuman_run);
ERRORTYPE MPP_HELPER_START_NPU(int width, int height, int fps);
ERRORTYPE MPP_HELPER_STOP_NPU();
BOOL MPP_HELPER_DO_NPU(void *phuman_run, npu_callback_t npu_callback);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __MPP_HELPER_H__ */
