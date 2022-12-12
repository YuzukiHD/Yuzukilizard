#include <float.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include "list.h"
#include "aw_image.h"
#include "media/mm_comm_vi.h"
#include "media/mm_comm_region.h"
#include "media/mpi_vi.h"
#include "vo/hwdisplay.h"
#include "log/log_wrapper.h"
#include <ClockCompPortIndex.h>
#include <mpi_videoformat_conversion.h>
#include <utils/VIDEO_FRAME_INFO_S.h>
#include <confparser.h>
#include <plat_type.h>
#include <tsemaphore.h>
#include <mm_comm_vo.h>
#include <mpi_sys.h>
#include <mpi_clock.h>
#include <mpi_vo.h>
#include <mpi_isp.h>
#include <mm_comm_venc.h>
#include <mpi_venc.h>
#include <mpi_region.h>
#include "aw_object_detection.h"
#include "det_res.h"

const char *object_labels[10] = { "person", "car", "bird", "cat", "dog",
                                  "potted plant", "dining table", "tv", "chair", "clock"
                                };
static AW_Obj_Det_Container *det_container;
static AW_Obj_Det_Outputs *det_outputs;

int npu_init_models(void)
{
    // Init models
    const char *model_path = "/mnt/sdcard/object_det/model/";

    // Object Detection
    det_container = (AW_Obj_Det_Container *)calloc(1, sizeof(AW_Obj_Det_Container));
    det_outputs = (AW_Obj_Det_Outputs *)calloc(1, sizeof(AW_Obj_Det_Outputs));

    if (det_outputs == NULL || det_container == NULL)
    {
        printf("npu init models failure.\n");
        return -1;
    }

    // Make containers.
    //printf("--------------------> Making containers begining---------------\n");
    aw_init_object_detection(det_container, det_outputs, model_path);

    return 0;
}

void npu_deinit_models(void)
{
    aw_deinit_object_detection(det_container, det_outputs);
    free(det_container);
    free(det_outputs);
}

static unsigned long detect_counter = 0;
static double detect_time = 0;
float run_single_object_det(VIDEO_FRAME_INFO_S *pFrameInfo, int width, int height, detect_res_t *res, float *single_det_time)
{
    //printf("Begin to load data.\n");
    float npu_check_time;
    static AW_Yuv_Buf yuv_buf;

    yuv_buf.fmt = YUV420SP_NV21;
    yuv_buf.h = height;
    yuv_buf.w = width;
    yuv_buf.c = 3;
    yuv_buf.phy_addr[0] = pFrameInfo->VFrame.mPhyAddr[0];
    yuv_buf.phy_addr[1] = pFrameInfo->VFrame.mPhyAddr[1];
    yuv_buf.phy_addr[2] = pFrameInfo->VFrame.mPhyAddr[2];

    yuv_buf.vir_addr[0] = pFrameInfo->VFrame.mpVirAddr[0];
    yuv_buf.vir_addr[1] = pFrameInfo->VFrame.mpVirAddr[1];
    yuv_buf.vir_addr[2] = pFrameInfo->VFrame.mpVirAddr[2];
    yuv_buf.str = NULL;
    //unsigned char *yuv_buffer = image_data;

    /*AW_Yuv_Buf *yuv_buf = aw_make_yuvbuf(yuv_buffer, width, height, YUV420SP_NV21);*/

    //printf("\n.......................... Run object detection ..........................\n");
    /*struct timeval start, end;*/
    /*gettimeofday(&start, NULL);*/

    static struct timeval start, end;

    gettimeofday(&start, NULL);
    aw_run_object_detection(det_container, &yuv_buf, det_outputs);
    gettimeofday(&end, NULL);

    double time_sum = ((1000.f * 1000 * end.tv_sec + end.tv_usec) - (1000.f * 1000 * start.tv_sec + start.tv_usec));
    npu_check_time = time_sum / 1000.f;   // convert to ms.
    *single_det_time = npu_check_time;

    detect_counter ++;
    detect_time += npu_check_time;

    float tmp = detect_time / detect_counter ;
    /*gettimeofday(&end, NULL);*/
    /*int time_sum = (1000.f * 1000 * (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec));*/
    //printf("aw_run_obj_det cost %dus, %dms.\n", time_sum, time_sum/1000);

    res->num = det_outputs->num;
    //printf("det_outputs->num = %d\n", det_outputs->num);
    for (int i = 0; i < det_outputs->num; ++i)
    {
#if 0
        printf("%d, label %d, class %s, prob %f, x1 %d, y1 %d, x2 %d, y2 %d\n", i, det_outputs->objects[i].label,
               object_labels[det_outputs->objects[i].label - 1],
               det_outputs->objects[i].prob, det_outputs->objects[i].bbox.tl_x, det_outputs->objects[i].bbox.tl_y,
               det_outputs->objects[i].bbox.br_x, det_outputs->objects[i].bbox.br_y);
#endif

        if (i < MAX_OBJECT_DET_NUM)
        {
            res->objs[i].label        = det_outputs->objects[i].label;
            res->objs[i].prob         = det_outputs->objects[i].prob;
            res->objs[i].left_up_x    = det_outputs->objects[i].bbox.tl_x;
            res->objs[i].left_up_y    = det_outputs->objects[i].bbox.tl_y;
            res->objs[i].right_down_x = det_outputs->objects[i].bbox.br_x;
            res->objs[i].right_down_y = det_outputs->objects[i].bbox.br_y;
        }
    }

    //printf("\nFree memory.\n");
    //aw_free_yuvbuf(yuv_buf);
    //printf("aw_free_yuvbuf finished.\n");

    //printf("aw_deinit_object_detection finished.\n");
    return tmp;
}

void run_single_object_det_old(unsigned char *image_data, int width, int height)
{
	//printf("Begin to load data.\n");

	int buffer_sz = width * height * 1.5;

	unsigned char *yuv_buffer = image_data;

	// Init models
	const char *model_path = "/mnt/sdcard/object_det/model/";

	// Object Detection
	AW_Obj_Det_Container *det_container = (AW_Obj_Det_Container *)calloc(1, sizeof(AW_Obj_Det_Container));
	AW_Obj_Det_Outputs *det_outputs = (AW_Obj_Det_Outputs *)calloc(1, sizeof(AW_Obj_Det_Outputs));

	// Make containers.
	//printf("--------------------> Making containers begining---------------\n");
	aw_init_object_detection(det_container, det_outputs, model_path);

	AW_Yuv_Buf *yuv_buf = aw_make_yuvbuf(yuv_buffer, width, height, YUV420SP_NV21);

	//printf("\n.......................... Run object detection ..........................\n");
	struct timeval start, end;
	gettimeofday(&start, NULL);

	aw_run_object_detection(det_container, yuv_buf, det_outputs);

	gettimeofday(&end, NULL);
	int time_sum = (1000.f * 1000 * (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec));
	printf("aw_run_obj_det cost %dus, %dms.\n", time_sum, time_sum/1000);

	printf("det_outputs->num = %d\n", det_outputs->num);
	for (int i = 0; i < det_outputs->num; ++i)
	{
		printf("%d, label %d, class %s, prob %f, x1 %d, y1 %d, x2 %d, y2 %d\n", i, det_outputs->objects[i].label,
			object_labels[det_outputs->objects[i].label - 1],
			det_outputs->objects[i].prob, det_outputs->objects[i].bbox.tl_x, det_outputs->objects[i].bbox.tl_y, 
			det_outputs->objects[i].bbox.br_x, det_outputs->objects[i].bbox.br_y);
	}

	//printf("\nFree memory.\n");
	aw_free_yuvbuf(yuv_buf);
	//printf("aw_free_yuvbuf finished.\n");

	aw_deinit_object_detection(det_container, det_outputs);
	free(det_container);
	free(det_outputs);

	//printf("aw_deinit_object_detection finished.\n");
}
