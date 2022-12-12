
#ifndef _SAMPLE_VIRVI2FISH2VENC_H_
#define _SAMPLE_VIRVI2FISH2VENC_H_

#include <plat_type.h>
#include <tsemaphore.h>

#define MAX_FILE_PATH_SIZE  (256)

typedef struct SampleVirvi2Fish2VencCmdLineParam
{
    char mConfigFilePath[MAX_FILE_PATH_SIZE];
}SampleVirvi2Fish2VencCmdLineParam;

typedef struct SampleVIConfig
{
    int DevId;
    int SrcWidth;
    int SrcHeight;
    int SrcFrameRate;
}SampleVIConfig;

typedef struct SampleVencConfig
{
    int mTimeLapseEnable;
    int mTimeBetweenFrameCapture;
    int DestWidth;
    int DestHeight;
    int DestFrameRate;
    int DestBitRate;
    char OutputFilePath[MAX_FILE_PATH_SIZE];
}SampleVencConfig;

typedef struct SampleISEGroupConfig
{
    int ISEPortNum;
    //WARP_Type_MO ISE_Dewarp_Mode;
    eWarpType ISE_GDC_Dewarp_Mode;
    float Lens_Parameter_P;
    int Lens_Parameter_Cx;
    int Lens_Parameter_Cy;
    //MOUNT_Type_MO Mount_Mode;
    eMount_Type ISE_GDC_Mount_Mode;
    float normal_pan;
    float normal_tilt;
    float normal_zoom;
    float ptz4in1_pan[4];
    float ptz4in1_tilt[4];
    float ptz4in1_zoom[4];
}SampleISEGroupConfig;

typedef struct SampleISEPortConfig
{
    int ISEWidth;
    int ISEHeight;
    int ISEStride;
    int flip_enable;
    int mirror_enable;
}SampleISEPortConfig;

typedef struct SampleVirvi2Fish2VencConfig
{
    int AutoTestCount;
    SampleVIConfig VIDevConfig;
    SampleISEGroupConfig ISEGroupConfig;
    SampleISEPortConfig ISEPortConfig[4];
    int VencChnNum;
    PIXEL_FORMAT_E DestPicFormat; //PIXEL_FORMAT_YUV_PLANAR_420
    int EncoderCount;
    PAYLOAD_TYPE_E EncoderType;
    SampleVencConfig VencChnConfig[4];
}SampleVirvi2Fish2VencConfig;

typedef struct SampleVirvi2Fish2VencConfparser
{
    SampleVirvi2Fish2VencCmdLineParam mCmdLinePara;
    SampleVirvi2Fish2VencConfig mConfigPara;
}SampleVirvi2Fish2VencConfparser;

#endif  /* _SAMPLE_VIRVI2FISH2VENC_H_ */

