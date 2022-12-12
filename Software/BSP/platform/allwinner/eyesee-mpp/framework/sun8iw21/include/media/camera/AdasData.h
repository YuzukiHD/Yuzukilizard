#ifndef _AW_ADAS_DETECT_TYPE_H
#define _AW_ADAS_DETECT_TYPE_H
#include <ADASAPI.h>
#include <Adas.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif


#define DEF_MPPCFG_ADAS_DETECT_V2


typedef struct AW_AI_ADAS_DETECT_R
{
     ADASOutData nADASOutData;
     int subWidth;
     int subHeight;
}AW_AI_ADAS_DETECT_R_;

typedef struct AW_AI_ADAS_DETECT_R_v2
{
     ADASOUT nADASOutData_v2;
     int subWidth;
     int subHeight;
}AW_AI_ADAS_DETECT_R__v2;



typedef struct AdasDetectParam
{
    unsigned int nframeWidth;
    unsigned int nframeHeight;
    int nroiX;
    int nroiY;
    int nroiH;
    int nroiW;
    unsigned int nfps;
    unsigned int nfocalLength;
    unsigned int npixelSize;
    unsigned int nhorizonViewAngle;
    unsigned int nverticalViewAngle;
    unsigned int nvanishX;
    unsigned int nvanishY;
    unsigned int nhoodLineDist;
    unsigned int nvehicleWidth;
    unsigned int nwheelDist;
    unsigned int ncameraToCenterDist;
    unsigned int ncameraHeight;
    unsigned int nleftSensity;
    unsigned int nrightSensity;
    unsigned int nfcwSensity;
    AdasDetectParam()
    {
        //ADAS init default data
        nframeWidth = 640;//视频宽度
        nframeHeight = 480;//视频高度
        nroiX = 0;//感兴趣区域X坐标
        nroiY = 0;//感兴趣区域Y坐标
        nroiW = 640;//感兴趣区域宽度
        nroiH = 480;//感兴趣区域高度
        nfps = 30;//视频的帧率fps
        //camera paramter
        nfocalLength = 2800;//焦距um
        npixelSize = 3;//sensor 单个像素大小um
        nhorizonViewAngle = 120;//水平视场角
        nverticalViewAngle = 90;//垂直视场角
        nvanishX = 640/2;//天际消失点X坐标
        nvanishY = 480/2+10;//天际消失点Y坐标
        nhoodLineDist = 120;//车头距离
        nvehicleWidth = 182;//车身宽度
        nwheelDist = 160;//车轮距离
        ncameraToCenterDist = 0;//摄像头中心距离
        ncameraHeight = 120;//相机安装高度
        nleftSensity = 1;//左侧车道线检测灵敏度
        nrightSensity = 1;//右侧车道线检测灵敏度
        nfcwSensity = 1;//前车碰撞预警灵敏度
    }
}AdasDetectParam;
typedef struct AdasInParam
{
    unsigned char ngpsSpeed;
    unsigned char ngpsSpeedEnable;
    unsigned char nGsensorStop;
    unsigned char nGsensorStopEnable;
    unsigned int nsensity;
    unsigned int nCPUCostLevel;
    unsigned int nluminanceValue;
    unsigned char nlightSignal;
    unsigned char nlightSignalEnable;
    unsigned int ncameraHeight;
    unsigned char *nptrFrameDataArray;
    unsigned char ndataType;
    unsigned char ncameraType;
    unsigned int nimageWidth;
    unsigned int nimageHeight;
    unsigned int ncameraNum;
    unsigned char nLng;
    float nLngValue;
    unsigned char nLat;
    float nLatValue;
    float naltitude;
    unsigned char ndeviceType;
    char nsensorID[255];
    unsigned char nnetworkStatus;

    AdasInParam()
    {
        //GPS数据
        ngpsSpeed = 80;//KM/H  千米每小时
        ngpsSpeedEnable = 0;//1-gps速度有效  0-gps速度无效
        //Gsensor数据
        nGsensorStop = 1;//加速度传感器停止标志
        nGsensorStopEnable = 0;//加速度传感器停止标志使能标记
        nsensity = 2;//检测灵敏度
        nCPUCostLevel = 0;//cpu消耗等级
        nluminanceValue = 1500;//关照强度LV
        //车辆转向灯信号
        nlightSignal = 0;//0-无信号 1-左转向  2-右转向
        nlightSignalEnable = 0;//信号灯使能信号
        ncameraHeight = 120;//相机安装的高度
        nptrFrameDataArray = NULL;//色彩图像数据
        ndataType = 2;//0-YUV 1-NV12 2-NV21 3-RGB...
        ncameraType = 0;//摄像头类型(0-前置摄像头 1-后置摄像头 2-左侧摄像头 3-右侧摄像头...)
        nimageWidth = 640;
        nimageHeight = 480;
        ncameraNum = 1;//摄像头个数
        //GPS经纬度信息
        nLng = 'E';//GPS经度
        nLngValue = 113.64;
        nLat = 'N';//GPS维度
        nLatValue = 22.27;
        naltitude = 20.00;//GPS海拔
        ndeviceType = 1;//设备类型(0-行车记录仪 1-后视镜...)
        strcpy(nsensorID,"imx317");
        nnetworkStatus = 0;//0-端口 1-连接

    }
}AdasInParam;

typedef struct AdasDetectParam_v2
{
    unsigned char isGps;       //有无gps
    unsigned char isGsensor;    //有无Gsensor，出v3之外其他方案请置为0
    unsigned char isObd;        //有无读取OBD
    unsigned char isCalibrate; //是否进行安装标定
    int fps;                          //帧率
    int width_small;
    int height_small;
    int width_big;
    int height_big;
    unsigned char angH;      //镜头的水平视角
    unsigned char angV;      //镜头的垂直视角
    unsigned short widthOri; //原始的图像宽度
    unsigned short heightOri;//原始的图像高度
    unsigned short    vanishLine;     //初始化的时候读入
    unsigned char     vanishLineIsOk;

    AdasDetectParam_v2()
    {
        //配置ADAS 初始化参数
        isGps = 0;//有无gps
        isGsensor = 0;//有无Gsensor，出v3之外其他方案请置为0
        isObd = 0;//有无读取OBD
        isCalibrate = 0;//是否进行安装标定

        fps = 30;//帧率
        width_big = 1920;//主码流图 1920*1080
        height_big = 1080;

        width_small = 960;//子码流图 960*540
        height_small = 540;

        angH = 105;//镜头的水平视角
        angV = 62;//镜头的垂直视角
        widthOri = 1920;//原始的图像宽度
        heightOri = 1080;//原始的图像高度
        vanishLine = 0;//???
        vanishLineIsOk = 0;//????
    }
}AdasDetectParam_v2;

typedef struct AdasInParam_v2
{
    int mode;
    int     gps_enable;//有无GPS信号
    float   speed; //车速
    int fcwSensity;//前车防撞灵敏度：0：高，1：中，2：低    默认值为1
    int ldwSensity;//车道偏离灵敏度：0：高，1：中，2：低    默认值为1
    int small_w;
    int small_h;
    int big_w;
    int big_h;
    AdasInParam_v2()
    {
        mode = 0;
        gps_enable = 0;
        speed = 80;
        fcwSensity = 1;
        ldwSensity = 1;
        small_w = 960;
        small_h = 540;
        big_w = 1920;
        big_h = 1080;
    }
}AdasInParam_v2;



#ifdef __cplusplus
}
#endif

#endif
