#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <pthread.h>
#include <stdlib.h>
#include <assert.h>
#include <utils/plat_log.h>
#include <utils/plat_math.h>
#include "aiservice.h"
#include "aiservice_detect.h"
#include "aiservice_mpp_helper.h"
#include "aiservice_hw_scale.h"

#define MILLION        1000000L

static aialgo_context_t *gp_ai_service_context = NULL;

static long get_time_in_us(void)
{
    struct timeval time;
    gettimeofday(&time, NULL);
    return (time.tv_usec + time.tv_sec * MILLION);
}

aialgo_context_t *get_aicontext(void)
{
    if (gp_ai_service_context)
    {
        return gp_ai_service_context;
    }
    else
    {
        aloge("fatal error! please execute aw_service_init first?");
        return NULL;
    }
}

int aw_service_init(void)
{
    aialgo_context_t *pctx = (aialgo_context_t *)malloc(sizeof(aialgo_context_t));
    if (NULL == pctx)
    {
        aloge("sys Init failed!");
        return -1;
    }
    gp_ai_service_context = pctx;
    memset(pctx, 0x00, sizeof(aialgo_context_t));

    if (mpp_helper_init())
    {
        aloge("sys Init failed!");
        return -1;
    }

    return 0;
}

int aw_service_uninit(void)
{
    /* exit mpp systerm */
    if (mpp_helper_uninit())
    {
        aloge("sys exit failed!");
    }

    aialgo_context_t *pctx = get_aicontext();
    if (pctx)
    {
        free(pctx);
        pctx = NULL;
    }
    gp_ai_service_context = NULL;

    return 0;
}

static int npu_detect_callback_body(unsigned char *pBuffer, int ch_idx, void *pRunParam)
{
    BBoxResults_t *res = NULL;
    unsigned char *body_input_buf[2] = { NULL, NULL };
    aialgo_context_t *pctx = get_aicontext();
    int ret = 0;

    int size = 0;
    if (pctx->use_g2d_flag)
    {
        unsigned int npu_det_pic_width = ALIGN(pctx->human_src_width, 32);
        unsigned int npu_det_pic_height = ALIGN(pctx->human_src_height, 32);
        size = npu_det_pic_width * npu_det_pic_height;
    }
    else
    {
        size = pctx->human_src_width * pctx->human_src_height;
    }

    // alogd("0x%x, 0x%x, 0x%x, 0x%x.", pBuffer[0], pBuffer[1], pBuffer[2], pBuffer[3]);
    body_input_buf[0] = pBuffer;
    body_input_buf[1] = pBuffer + size;

    assert(pctx->ai_det_handle != NULL);
    if(pctx->ai_det_handle == NULL)
    {
        aloge("fata error, ai det handle should not be null.");
        ret = -1;
    }
    else
    {
        ((Awnn_Run_t *)pRunParam)->input_buffers = body_input_buf;
        res = awnn_detect_run(pctx->ai_det_handle, (Awnn_Run_t *)pRunParam);
        if (res)
        {
            if (pctx->attr.ch_info[ch_idx].draw_orl_enable)
            {
                paint_object_detect_region_body(res, ch_idx);
            }
            for (int j = 0; j < res->valid_cnt; j ++)
            {
                alogv("ch_idx=%d, cls %d, prob %f, rect [%d, %d, %d, %d]\n", ch_idx, res->boxes[j].label, res->boxes[j].score,
                      res->boxes[j].xmin, res->boxes[j].ymin, res->boxes[j].xmax, res->boxes[j].ymax);
            }
            ret = res->valid_cnt;
        }
        else
        {
            aloge("fatal error, res==NULL should not happen for object detection.");
        }
    }

    return ret;
}

static void *npu_worker_thread_body(void *para)
{
    unsigned char *yuv_buffer_body = NULL;
    aialgo_context_t *pctx = get_aicontext();
    ai_service_info_t *pchinfo = (ai_service_info_t *)para;
    if (!pctx || !pchinfo)
    {
        aloge("fatal error! invalid input params! %p,%p", pctx, pchinfo);
        return NULL;
    }

    double start_time = get_time_in_us();
    double before     = get_time_in_us();
    int frame_counter = 0;
    statistics_t         statis;
    memset(&statis, 0, sizeof(statistics_t));

    int size = 0;
    if (pctx->use_g2d_flag)
    {
        int npu_det_pic_width = ALIGN(pctx->human_src_width, 32);
        int npu_det_pic_height = ALIGN(pctx->human_src_height, 32);
        size = npu_det_pic_width * npu_det_pic_height * 3 / 2;
    }
    else
    {
        size = pctx->human_src_width * pctx->human_src_width * 3 / 2;
    }
    yuv_buffer_body = malloc(size);
    if(yuv_buffer_body == NULL)
    {
        aloge("fatal error, malloc yuv body buffer failure.");
        return NULL;
    }
    memset(yuv_buffer_body, 0x00, size);

    Awnn_Run_t run_param;
    awnn_create_run(&run_param, AWNN_HUMAN_DET);

    while (!pctx->aiservice_exit)
    {
        // do the actual job of body detection.
        mpp_helper_do_npu(yuv_buffer_body, pchinfo->ch_idx, (void *)&run_param, npu_detect_callback_body);

        double after = get_time_in_us();
        float curr = MILLION / (after - before);
        before = after;

        statis.fps_body = statis.fps_body * 0.9 + curr * 0.1;
        frame_counter ++;
        float spent_time = (get_time_in_us() - start_time) / MILLION;

        //statistic one time each 3 seconds.
        if (spent_time >= 3.0f)
        {
            statis.avg_fps_body = frame_counter / spent_time;
            frame_counter = 0;
            start_time = get_time_in_us();
            alogd("ch_idx=%d, avg body fps %f, body fps %f.", pchinfo->ch_idx, statis.avg_fps_body, statis.fps_body);
        }
        else if(spent_time < 0)
        {
            //i dont know why, but is happend.
            start_time = get_time_in_us();
        }
        usleep(30*1000);
    }

    awnn_destroy_run(&run_param);

    if (yuv_buffer_body)
    {
        free(yuv_buffer_body);
        yuv_buffer_body = NULL;
    }

    return NULL;
}

static int npu_detect_callback_face(unsigned char *pBuffer, int ch_idx, void *pRunParam)
{
    BBoxResults_t *res=NULL;
    unsigned char *face_input_buf[2] = { NULL, NULL };
    aialgo_context_t *pctx = get_aicontext();

    assert(pctx->ai_det_handle != NULL);
    if(pctx->ai_det_handle == NULL)
    {
        aloge("fata error, ai det handle should not be null.");
        return -1;
    }

    if (pctx->use_g2d_flag)
    {
        unsigned int vi_npu_pic_width = pctx->human_src_width;
        unsigned int vi_npu_pic_height = pctx->human_src_height;
        unsigned int npu_det_pic_width = ALIGN(pctx->human_src_width, 32);
        unsigned int npu_det_pic_height = ALIGN(pctx->human_src_height, 32);
        int scale_target_width = pctx->face_src_width;
        int scale_target_height = pctx->face_src_height;
        int scale_target_width_face = pctx->face_src_width;
        int scale_target_heigh_face = pctx->face_src_width;

        if(pctx->pout_yuv == NULL)
        {
            int size = scale_target_width * scale_target_height * 3 / 2;
            pctx->pout_yuv = malloc(size);
            if (!pctx->pout_yuv)
            {
                aloge("malloc poutyuv is null.");
                return 0;
            }
            memset(pctx->pout_yuv, 0x00, size);
        }

        int face_yuv_len = scale_target_width_face * scale_target_heigh_face * 3 / 2;
        if(pctx->face_yuv == NULL)
        {
            pctx->face_yuv = malloc(face_yuv_len);
            if (!pctx->face_yuv)
            {
                aloge("malloc poutyuv is null.");
                return 0;
            }
            memset(pctx->face_yuv, 0x00, face_yuv_len);
        }

        if(pctx->pin_yuv == NULL)
        {
            int size = vi_npu_pic_width * vi_npu_pic_height  * 3 / 2;
            pctx->pin_yuv = malloc(size);
            if (!pctx->pin_yuv)
            {
                aloge("malloc poutyuv is null.");
                return 0;
            }
            memset(pctx->pin_yuv, 0x00, size);
        }

        memcpy(pctx->pin_yuv, pBuffer, vi_npu_pic_width * vi_npu_pic_height);
        memcpy(pctx->pin_yuv + vi_npu_pic_width * vi_npu_pic_height, pBuffer + npu_det_pic_width * npu_det_pic_height, vi_npu_pic_width * vi_npu_pic_height * 1 / 2);

        //g2d hardware scale.
        scale_picture_nv12_to_nv12_byg2d(pctx->pin_yuv, vi_npu_pic_width, vi_npu_pic_height, pctx->pout_yuv, scale_target_width, scale_target_height, pctx->g2d_fd);

        memcpy(pctx->face_yuv, pctx->pout_yuv, scale_target_width * scale_target_height);
        memcpy(pctx->face_yuv + face_yuv_len * 2 / 3, pctx->pout_yuv + scale_target_width * scale_target_height, scale_target_width * scale_target_height * 1 / 2);

        face_input_buf[0] = pctx->face_yuv;
        face_input_buf[1] = pctx->face_yuv + face_yuv_len * 2 / 3;
    }
    else
    {
        face_input_buf[0] = pBuffer;
        face_input_buf[1] = pBuffer + pctx->face_src_width * pctx->face_src_height;
    }

    ((Awnn_Run_t *)pRunParam)->input_buffers = face_input_buf;
    res = awnn_detect_run(pctx->ai_det_handle, (Awnn_Run_t *)pRunParam);
    if(res)
    {
        if (pctx->attr.ch_info[ch_idx].draw_orl_enable)
        {
            paint_object_detect_region_face(res, ch_idx);
        }
        for (int j = 0; j < res->valid_cnt; j++)
        {
            alogv("ch_idx=%d, num %d landmark [%d, %d] [%d, %d] [%d, %d] [%d, %d] [%d, %d].", ch_idx, res->valid_cnt, res->boxes[j].landmark_x[0],res->boxes[j].landmark_x[0], \
                res->boxes[j].landmark_x[1],res->boxes[j].landmark_x[1], res->boxes[j].landmark_x[2],res->boxes[j].landmark_x[2], \
                res->boxes[j].landmark_x[3],res->boxes[j].landmark_x[3], res->boxes[j].landmark_x[4],res->boxes[j].landmark_x[4]);
        }
    }

    return 0;
}

static void *npu_worker_thread_face(void *para)
{
    unsigned char *yuv_buffer_face = NULL;
    aialgo_context_t *pctx = get_aicontext();
    ai_service_info_t *pchinfo = (ai_service_info_t *)para;
    if (!pctx || !pchinfo)
    {
        aloge("fatal error! invalid input params! %p,%p", pctx, pchinfo);
        return NULL;
    }

    double start_time = get_time_in_us();
    double before     = get_time_in_us();
    int frame_counter = 0;
    statistics_t         statis;
    memset(&statis, 0, sizeof(statistics_t));

    if (pctx->use_g2d_flag)
    {
        pctx->g2d_fd = open("/dev/g2d", O_RDWR, 0);
        if(pctx->g2d_fd < 0)
        {
            aloge("open g2d device failed!");
            return NULL;
        }

        if(g2d_mem_open() != 0)
        {
            aloge("init ion device failed!");
            close(pctx->g2d_fd);
            pctx->g2d_fd = -1;

            return NULL;
        }

        scale_create_buffer();
    }

    int size = 0;
    if (pctx->use_g2d_flag)
    {
        int npu_det_pic_width = ALIGN(pctx->human_src_width, 32);
        int npu_det_pic_height = ALIGN(pctx->human_src_height, 32);
        size = npu_det_pic_width * npu_det_pic_height * 3 / 2;
    }
    else
    {
        size = pctx->face_src_width * pctx->face_src_width * 3 / 2;
    }
    yuv_buffer_face = malloc(size);
    if(yuv_buffer_face == NULL)
    {
        aloge("fatal error, malloc yuv face buffer failure.");
        return NULL;
    }
    memset(yuv_buffer_face, 0x00, size);

    Awnn_Run_t run_param;
    awnn_create_run(&run_param, AWNN_FACE_DET);

    while (!pctx->aiservice_exit)
    {
        mpp_helper_do_npu(yuv_buffer_face, pchinfo->ch_idx, (void *)&run_param, npu_detect_callback_face);

        double after = get_time_in_us();
        float curr = MILLION / (after - before);
        before = after;

        statis.fps_face = statis.fps_face * 0.9 + curr * 0.1;
        frame_counter ++;
        float spent_time = (get_time_in_us() - start_time) / MILLION;

        //statistic one time each 3 seconds.
        if (spent_time >= 3.0f)
        {
            statis.avg_fps_face = frame_counter / spent_time;
            frame_counter = 0;
            start_time = get_time_in_us();
            alogd("ch_idx=%d, avg face fps %f, face fps %f.", pchinfo->ch_idx, statis.avg_fps_face, statis.fps_face);
        }
        else if(spent_time < 0)
        {
            //i dont know why, but is happend.
            start_time = get_time_in_us();
        }
    }

    awnn_destroy_run(&run_param);

    if(yuv_buffer_face)
    {
        free(yuv_buffer_face);
        yuv_buffer_face = NULL;
    }

    if (pctx->use_g2d_flag)
    {
        scale_destory_buffer();

        close(pctx->g2d_fd);
        pctx->g2d_fd = -1;

        g2d_mem_close();
    }

    return NULL;
}

int aw_service_start(ai_service_attr_t *pattr)
{
    int err = 0;
    char *human_nb = NULL;
    char *face_nb = NULL;

    aialgo_context_t *pctx = get_aicontext();

    memcpy(&pctx->attr, pattr, sizeof(ai_service_attr_t));

    pctx->aiservice_exit = 0;

    for (int i = 0; i < NN_CHN_NUM_MAX; i++)
    {
        if (AWNN_HUMAN_DET == pctx->attr.ch_info[i].nbg_type)
        {
            human_nb = pctx->attr.ch_info[i].model_file;
            pctx->human_src_width = pctx->attr.ch_info[i].src_width;
            pctx->human_src_height = pctx->attr.ch_info[i].src_height;
        }
        else if (AWNN_FACE_DET == pctx->attr.ch_info[i].nbg_type)
        {
            face_nb = pctx->attr.ch_info[i].model_file;
            pctx->face_src_width = pctx->attr.ch_info[i].src_width;
            pctx->face_src_height = pctx->attr.ch_info[i].src_height;
        }
    }
    alogd("human src_width:%d, src_height:%d, face src_width:%d, src_height:%d",
        pctx->human_src_width, pctx->human_src_height, pctx->face_src_width, pctx->face_src_height);

    if (NULL == human_nb && NULL == face_nb)
    {
        alogw("both human and face are disabled!");
        return 0;
    }

    // 人形和人脸的数据来自同一个vipp，需要使用G2D转换
    if (human_nb && face_nb && pctx->attr.ch_info[0].vipp == pctx->attr.ch_info[1].vipp)
    {
        pctx->use_g2d_flag = 1;
        alogd("both human and face are same vipp%d, use g2d scale.", pctx->attr.ch_info[0].vipp);
    }
    else
    {
        pctx->use_g2d_flag = 0;
    }

    Awnn_Init_t init_param;
    memset(&init_param, 0, sizeof(Awnn_Init_t));
    init_param.human_nb = human_nb;
    init_param.face_nb = face_nb;
    awnn_detect_init(&pctx->ai_det_handle, &init_param);
    if(pctx->ai_det_handle == NULL)
    {
        aloge("fatal error, ai model handle failure.");
        return -1;
    }
    alogd("ai_det_handle=%p, human model=%s, face model=%s", pctx->ai_det_handle, human_nb, face_nb);

    for (int i = 0; i < NN_CHN_NUM_MAX; i++)
    {
        if (AWNN_HUMAN_DET == pctx->attr.ch_info[i].nbg_type ||
            AWNN_FACE_DET == pctx->attr.ch_info[i].nbg_type)
        {
            pctx->region_info[i].region_hdl_base = pctx->attr.ch_info[i].region_hdl_base;
            mpp_helper_start_npu(pctx->attr.ch_info[i].isp,
                                 pctx->attr.ch_info[i].vipp,
                                 pctx->attr.ch_info[i].vi_buf_num,
                                 pctx->attr.ch_info[i].src_width,
                                 pctx->attr.ch_info[i].src_height,
                                 pctx->attr.ch_info[i].src_frame_rate,
                                 pctx->attr.ch_info[i].pixel_format);
            if (pctx->use_g2d_flag)
            {
                break;
            }
        }
    }
    pctx->vichannel_started = 1;

    for (int i = 0; i < NN_CHN_NUM_MAX; i++)
    {
        if (AWNN_HUMAN_DET == pctx->attr.ch_info[i].nbg_type)
        {
            err = pthread_create(&pctx->detect_thread[i], NULL, npu_worker_thread_body, &pctx->attr.ch_info[i]);
            if (err != 0)
            {
                aloge("fatal error, create body ch%d detect worker thread failed.", i);
                return -1;
            }
            alogd("create body ch%d detect worker thread success.", i);
        }
        if (AWNN_FACE_DET == pctx->attr.ch_info[i].nbg_type)
        {
            err = pthread_create(&pctx->detect_thread[i], NULL, npu_worker_thread_face, &pctx->attr.ch_info[i]);
            if (err != 0)
            {
                aloge("fatal error, create face ch%d detect worker thread failed.", i);
                return -1;
            }
            alogd("create face ch%d detect worker thread success.", i);
        }
    }

    alogd("success");

    return err;
}

int aw_service_stop(void)
{
    unsigned long value;

    aialgo_context_t *pctx = get_aicontext();

    pctx->aiservice_exit = 1;

    for (int i = 0; i < NN_CHN_NUM_MAX; i++)
    {
        if (pctx->detect_thread[i])
        {
            pthread_join(pctx->detect_thread[i], (void **)&value);
            pctx->detect_thread[i] = NULL;
        }
    }

    if (pctx->vichannel_started)
    {
        for (int i = 0; i < NN_CHN_NUM_MAX; i++)
        {
            if (AWNN_HUMAN_DET == pctx->attr.ch_info[i].nbg_type ||
                AWNN_FACE_DET == pctx->attr.ch_info[i].nbg_type)
            {
                mpp_helper_stop_npu(pctx->attr.ch_info[i].isp, pctx->attr.ch_info[i].vipp);
                if (pctx->use_g2d_flag)
                {
                    break;
                }
            }
        }
    }
    pctx->vichannel_started = 0;

    awnn_detect_exit(pctx->ai_det_handle);
    pctx->ai_det_handle = NULL;

    alogd("success");

    return 0;
}
