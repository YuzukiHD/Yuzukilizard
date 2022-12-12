
#ifndef _SAMPLE_VIRVI2FISH2VO_H_
#define _SAMPLE_VIRVI2FISH2VO_H_

#include <plat_type.h>
#include <tsemaphore.h>

#define MAX_FILE_PATH_SIZE  (256)

typedef struct SampleVirvi2Fish2VoCmdLineParam
{
    char mConfigFilePath[MAX_FILE_PATH_SIZE];
}SampleVirvi2Fish2VoCmdLineParam;

typedef struct SampleVIConfig
{
    int DevId;
    int SrcWidth;
    int SrcHeight;
    int SrcFrameRate;
}SampleVIConfig;

typedef struct SampleVOConfig
{
    int display_width;
    int display_height;
    int mFrameRate;
    int mTestDuration;  //unit:s, 0 mean infinite
}SampleVOConfig;

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

typedef struct SampleVirvi2Fish2VoConfig
{
    int AutoTestCount;
    SampleVIConfig VIDevConfig;
    SampleISEGroupConfig ISEGroupConfig;
    SampleISEPortConfig ISEPortConfig[4];
    SampleVOConfig  VOChnConfig;
}SampleVirvi2Fish2VoConfig;

typedef struct SampleVirvi2Fish2VoConfparser
{
    SampleVirvi2Fish2VoCmdLineParam mCmdLinePara;
    SampleVirvi2Fish2VoConfig mConfigPara;
}SampleVirvi2Fish2VoConfparser;

#endif  /* _SAMPLE_VIRVI2FISH2VO_H_ */

