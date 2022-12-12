#ifndef __AISERVICE_DETECT_H__
#define __AISERVICE_DETECT_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stdbool.h>
#include <pthread.h>
#include "awnn_det.h"
#include "aiservice.h"

// 服务初始化
int aw_service_init();
// 服务反初始化
int aw_service_uninit();
// 服务开始
int aw_service_start(ai_service_attr_t *pattr);
// 服务结束
int aw_service_stop();

typedef struct
{
    float   avg_fps_body;
    float   fps_body;
    float   avg_fps_face;
    float   fps_face;
}statistics_t;

typedef struct
{
    int region_hdl_base;
    int old_num_of_boxes;
}region_info_t;

typedef struct
{
    pthread_t            detect_thread[NN_CHN_NUM_MAX];
    Awnn_Det_Handle      ai_det_handle;
    int                  aiservice_exit;
    int                  vichannel_started;
    int                  g2d_fd;
    ai_service_attr_t    attr;
    int                  use_g2d_flag;
    unsigned char       *pout_yuv;
    unsigned char       *pin_yuv;
    unsigned char       *face_yuv;
    int                 human_src_width;
    int                 human_src_height;
    int                 face_src_width;
    int                 face_src_height;
    region_info_t       region_info[NN_CHN_NUM_MAX];
} aialgo_context_t;

aialgo_context_t *get_aicontext(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __AISERVICE_DETECT_H__ */
