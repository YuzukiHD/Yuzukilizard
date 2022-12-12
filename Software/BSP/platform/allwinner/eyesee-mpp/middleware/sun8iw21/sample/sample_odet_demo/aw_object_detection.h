#ifndef AW_OBJECT_DETECTION_H
#define AW_OBJECT_DETECTION_H

#define defXMI_AI_RESULT_NUM 20

#ifdef __cplusplus
extern "C"
{
#endif //__cplusplus

#include "aw_image.h"


typedef struct _aw_object_detection AW_Obj_Det;
struct  _aw_object_detection
{
    AW_Box bbox;
    int label;
    float prob;
};

typedef struct _aw_object_detection_outputs AW_Obj_Det_Outputs;
struct _aw_object_detection_outputs
{
    int num;
    AW_Obj_Det *objects;
};

/*
* A struct that stores models, settings and history for the purpose of
* object detection for an image.
*
* models:     the handle of models.
* models1:    the second handle of models.
* settings:   the handle of runtime settings.
* history:    the handle of history information.
*/
typedef struct _aw_object_detection_container AW_Obj_Det_Container;
struct _aw_object_detection_container
{
    void *models;
    void *settings;
    void *history;
};

/*
* A struct that stores params.
*
* target_size: the target image size.
*/
typedef struct _aw_object_detection_settings AW_Obj_Det_Settings;
struct _aw_object_detection_settings
{
    int target_size;
    void *option;
};

/*
*  Make a YUV AW_Yuv_Buf and copy data from an array.
*/

AW_Yuv_Buf *aw_make_yuvbuf(const unsigned char *yuv_buffer, int width, int height, AW_YUV_TYPE yuv_type);

/*
* Initialization for container.
*
* Input:
*   container:  the container need to be initialized.
*   model_path: the first half part of path where model files is stored.
* Output:
*   0,  initialized successfully.
*  -1,  something wrong.
*/

int aw_init_object_detection(AW_Obj_Det_Container *container, AW_Obj_Det_Outputs *outputs, const char *model_path);

/*
* Run object detection.
*
* Input:
*   container:    the initialized container.
*   yuv_buf:      the input yuv data which contains objects.
* Output:
*   label:        result of object detection: .
*   prob:         result of object detection: .
*   x,y,width,height:   result of object detection: the rect
*/

void aw_run_object_detection(AW_Obj_Det_Container *container, AW_Yuv_Buf *yuv_buf, AW_Obj_Det_Outputs *outputs);

/*
* Free memory for container and output.
*/

void aw_deinit_object_detection(AW_Obj_Det_Container *container, AW_Obj_Det_Outputs *outputs);

/*
*  Free memory for YUV AW_Yuv_Buf.
*/
void aw_free_yuvbuf(AW_Yuv_Buf *yuvbuf);

#ifdef __cplusplus
}
#endif //__cplusplus

#endif // !AW_OBJECT_DETECTION_H
