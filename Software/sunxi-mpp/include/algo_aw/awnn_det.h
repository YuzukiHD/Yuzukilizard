#ifndef __AWNN_DET_H__
#define __AWNN_DET_H__
#ifdef __cplusplus
       extern "C" {
#endif

typedef enum{
	AWNN_HUMAN_DET,
	AWNN_FACE_DET,
	AWNN_TYPE_MAX
} AWNN_TYPE;

typedef void* Awnn_Det_Handle;
typedef struct 
{
    int xmin; // 左侧坐标
    int ymin; // 顶部坐标
    int xmax; // 右侧坐标
    int ymax; // 底部坐标
    int label; // 标签：人形:0，非人形>0。
    float score; // 概率值
    int landmark_x[5]; // 五官坐标x，分别对应左眼、右眼、鼻子、嘴左侧，嘴右侧
    int landmark_y[5]; // 五官坐标y，分别对应左眼、右眼、鼻子、嘴左侧，嘴右侧
}BBoxRect_t;

typedef struct
{
    int valid_cnt;
    int cap;
    BBoxRect_t *boxes;
}BBoxResults_t;

typedef struct
{
    char *human_nb;
    char *face_nb;
} Awnn_Init_t;

typedef struct
{
    AWNN_TYPE type;
    unsigned char **input_buffers;
    float thresh; // 灵敏度，范围[0,1]，值越小越灵敏
    BBoxResults_t *res;
} Awnn_Run_t;

void awnn_detect_init(Awnn_Det_Handle *handle, Awnn_Init_t *initT);
void awnn_create_run(Awnn_Run_t *runT, AWNN_TYPE type);
BBoxResults_t *awnn_detect_run(Awnn_Det_Handle handle, Awnn_Run_t *runT);
void awnn_destroy_run(Awnn_Run_t *runT);
void awnn_detect_exit(Awnn_Det_Handle handle);
       
#ifdef __cplusplus
}
#endif

#endif