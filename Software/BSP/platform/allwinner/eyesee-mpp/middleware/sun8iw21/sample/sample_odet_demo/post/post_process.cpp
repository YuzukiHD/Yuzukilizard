// yolo_v3_post_process.cpp : Defines the entry point for the console application.
//


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __linux__
#include <sys/time.h>
#endif
#include <string>
#include <iostream>
//#include <opencv2/objdetect/objdetect.hpp>
//#include <opencv2/highgui/highgui.hpp>
//#include <opencv2/imgproc/imgproc.hpp>
#include "yolo_layer.h"
#include <time.h>
#include "../det_res.h"
#define BILLION                                 1000000000
static uint64_t get_perf_count()
{
#if defined(__linux__) || defined(__ANDROID__) || defined(__QNX__) || defined(__CYGWIN__)
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (uint64_t)((uint64_t)ts.tv_nsec + (uint64_t)ts.tv_sec * BILLION);
#elif defined(_WIN32) || defined(UNDER_CE)
    LARGE_INTEGER ln;

    QueryPerformanceCounter(&ln);

    return (uint64_t)ln.QuadPart;
#endif
}





using namespace std;
//using namespace cv;
char *coco_names[] = {"person","bicycle","car","motorbike","aeroplane","bus","train","truck","boat","traffic light","fire hydrant","stop sign","parking meter","bench","bird","cat","dog","horse","sheep","cow","elephant","bear","zebra","giraffe","backpack","umbrella","handbag","tie","suitcase","frisbee","skis","snowboard","sports ball","kite","baseball bat","baseball glove","skateboard","surfboard","tennis racket","bottle","wine glass","cup","fork","knife","spoon","bowl","banana","apple","sandwich","orange","broccoli","carrot","hot dog","pizza","donut","cake","chair","sofa","pottedplant","bed","diningtable","toilet","tvmonitor","laptop","mouse","remote","keyboard","cell phone","microwave","oven","toaster","sink","refrigerator","book","clock","vase","scissors","teddy bear","hair drier","toothbrush"};
/************************************************************************/
/*                                                                      */
/************************************************************************/
int get_tensor_data(string file_path, float **data)
{
    int len = 0;
    static float *memory = NULL;
    static int max_len = 10*1024*1024;

    if (memory == NULL)
        memory = (float *)malloc(max_len * sizeof(float));

    FILE *fp = NULL;
    if ((fp = fopen(file_path.c_str(), "r")) == NULL)
    {
        cout << "open tensor file error ! file name : " << file_path << endl;
        exit(-1);
    }

    int file_len = 0;
    while (!feof(fp))
    {
        fscanf(fp, "%f ", &memory[len++]);
    }

    *data = (float *)malloc(len * sizeof(float));
    memcpy(*data, memory, len * sizeof(float));
    fclose(fp);

    if (len == 0 || *data == NULL)
    {
        cout << "read tensor error happened ! " << "len : " << len << " data address: " << *data << endl;
        exit(-1);
    }

    return len;
}

//////////////////////////////////////////////////////////////////////////
extern "C" { int post_main(float **tensor);  };
extern "C" { detect_res_t *get_ret_boxes(void);  };
int post_main(float **tensor)
{
    //char * ouput_file_path[3];
#if 0
    if(argc < 4)
    {
        return 0;
    }
#endif

    //ouput_file_path[0]= ( char *)argv[1];
    //ouput_file_path[1]= ( char *)argv[2];
    //char *image_path = ( char *)argv[3];
    
    vector<blob> blobs;
    for (int i=0; i<2; i++)
    {
        blob temp_blob = {0, NULL};
        //get_tensor_data(ouput_file_path[i], &temp_blob.data);
	    temp_blob.data =tensor[i];
        blobs.push_back(temp_blob);
    }
    blobs[0].w = 13;
    blobs[1].w = 26;

    //cv::Mat image = cv::imread(image_path);
    //int img_w = image.cols;
    //int img_h = image.rows;
    
    int img_w = 416;
    int img_h = 416;

    int classes = 80;
    float thresh = 0.2;
    float nms = 0.45;
    int nboxes = 0;
    uint64_t tmsStart, tmsEnd, msVal, usVal;

    tmsStart = get_perf_count();
    detection *dets = get_detections(blobs, 416, 416, 416, 416, &nboxes, classes, thresh, nms);

    detect_res_t *get_ret_boxes(void);
    detect_res_t * res = get_ret_boxes();
    int i,j;
	int boxesnum = 0;
    for(i=0;i< nboxes;++i){
        //char labelstr[4096] = {0};
        int cls = -1;
        for(j=0;j<classes;++j){
            if(dets[i].prob[j] > 0.1){
                if(cls < 0){
                    cls = j;
                    break;
                }
            }
        }
        if(cls >= 0)
        {
			int iter = boxesnum ++;
            box b = dets[i].bbox;

            int left  = (b.x-b.w/2.)*img_w;
            int right = (b.x+b.w/2.)*img_w;
            int top   = (b.y-b.h/2.)*img_h;
            int bot   = (b.y+b.h/2.)*img_h;
            res->objs[iter].label        = cls;
            res->objs[iter].prob         = dets[i].prob[cls];
            res->objs[iter].left_up_x    = left;
            res->objs[iter].left_up_y    = top;
            res->objs[iter].right_down_x = right;
            res->objs[iter].right_down_y = bot;
            //cv::rectangle(image,cv::Point(left,top),cv::Point(right,bot),cv::Scalar(0,0,255),2,8,0);
            //cv::putText(image,coco_names[cls],cv::Point(left,top),2,0.5,cv::Scalar(0,255,255),1,1,0);
            printf("=====================================%s  %.0f%% %d %d %d %d==============================================\n",coco_names[cls],dets[i].prob[cls]*100,left,top,right-left,bot-top);
        }
    }
	res->num = boxesnum;
    tmsEnd = get_perf_count();
    msVal = (tmsEnd - tmsStart)/1000000;
    usVal = (tmsEnd - tmsStart)/1000;
    //cv::imwrite("result.jpg",image);
  return 0;
}

