#ifndef __AI_SERVICE_H__
#define __AI_SERVICE_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define NN_NBG_MAX_FILE_PATH_SIZE   (256)

#define NN_HUMAN_SRC_WIDTH          (320)
#define NN_HUMAN_SRC_HEIGHT         (192)
#define NN_FACE_SRC_WIDTH           (480)
#define NN_FACE_SRC_HEIGHT          (270)

#define NN_CHN_NUM_MAX              (2)

typedef struct ai_service_info_t
{
    int ch_idx;
    int nbg_type;
    int isp;
    int vipp;
    int viChn;
    int src_width;
    int src_height;
    int pixel_format;
    int vi_buf_num;
    int src_frame_rate;
    char model_file[NN_NBG_MAX_FILE_PATH_SIZE];
    int draw_orl_enable;
    int draw_orl_vipp;
    int draw_orl_src_width;
    int draw_orl_src_height;
    int region_hdl_base;
} ai_service_info_t;

typedef struct ai_service_attr_t
{
    ai_service_info_t ch_info[NN_CHN_NUM_MAX];
} ai_service_attr_t;

int ai_service_start(ai_service_attr_t *pattr);
int ai_service_stop(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __AI_SERVICE_H__ */
