#ifndef __AISERVICE_MPP_HELPER_H__
#define __AISERVICE_MPP_HELPER_H__

#include <stdio.h>
#include <plat_type.h>
#include <media/mm_comm_region.h>
#include <media/mpi_region.h>
#include "awnn_det.h"

#define _CHECK_RET( ret )  do { \
        if( SUCCESS != ret ) { \
            aloge("Error no %d.\n", ret); \
            return ret; \
        } \
    } while(0)

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef int (*npu_callback_t)(unsigned char *pBuffer, int ch_idx, void *pRunParam);

ERRORTYPE mpp_helper_init(void);
ERRORTYPE mpp_helper_uninit(void);
ERRORTYPE mpp_helper_start_npu(int isp, int vipp, int vipp_buf_num, int width, int height, int fps, int format);
ERRORTYPE mpp_helper_stop_npu(int isp, int vipp);
BOOL mpp_helper_do_npu(unsigned char *pBuffer, int ch_idx, void *pRunParam, npu_callback_t npu_callback);
int mpp_helper_rgn_create(void);
int mpp_helper_rgn_destroy(void);
int mpp_helper_rgn_update(int i, RGN_CHN_ATTR_S *rgnChnAttr);
void paint_object_detect_region_body(BBoxResults_t *res, int ch_idx);
void paint_object_detect_region_body2(BBoxResults_t *res, int ch_idx);

void paint_object_detect_region_face(BBoxResults_t *res, int ch_idx);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __AISERVICE_MPP_HELPER_H__ */
