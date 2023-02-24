#ifndef _ADASAPI_H
#define _ADASAPI_H

#define MAX_CAR_NUM 10
#define MAX_PLATE_NUM 5
#define MAX_INDEX_NUM 30

#ifdef __cplusplus
extern "C"
{
#endif

//callbackin参数
typedef struct _ADASInData
{
	//图像帧数据
	unsigned char *ptrFrameData;

	//GPS数据
	unsigned char gpsSpeed;//KM/H  千米每小时
	unsigned char gpsSpeedEnable;//1-gps速度有效  0-gps速度无效

	//Gsensor数据
	unsigned char GsensorStop;//
	unsigned char GsensorStopEnable;//

	//敏感度 每个功能占用两位
	unsigned int sensity;//01

	//CPUCostLevel 每个功能占用两位
	unsigned int CPUCostLevel;

	//光照强度LV
	unsigned int luminanceValue;

	//车辆转向灯信号
	unsigned char lightSignal;//0-无信号 1-左转向  2-右转向
	unsigned char lightSignalEnable;

	//天际线
	//unsigned int vanishPointX;
	//unsigned int vanishPointY;
	//unsigned char vanishPointEnable;

	//相机高度
	unsigned int cameraHeight;//mm

	/*****************************************************************/

	/******************************************************************/
	//图像帧数据
	unsigned char *ptrFrameDataArray;//彩色图像数据指针
	unsigned char dataType;//0-YUV 1-NV12 2-NV21 3-RGB...
	unsigned char cameraType;//摄像头类型(0-前置摄像头 1-后置摄像头 2-左侧摄像头 3-右侧摄像头...)
	unsigned int imageWidth;
	unsigned int imageHeight;
	unsigned int cameraNum;
	
	//GPS经纬度信息
	//GPS经度
	unsigned char Lng;//E W
	float LngValue;
	//GPS维度
	unsigned char Lat;//N S
	float LatValue;

	//GPS海拔
	float altitude;

	//设备类型(0-行车记录仪 1-后视镜...)
	unsigned char deviceType;

	//sensor类型
	char sensorID[255];

	unsigned char networkStatus;//0-端口 1-连接
	/******************************************************************/

}ADASInData;


//callbackout参数
typedef struct _ADASOutData
{
	//==================================车道线
	//版本信息
	unsigned char *ptrVersion;

	//左车道线两点
	unsigned int leftLaneLineX0;
	unsigned int leftLaneLineY0;

	unsigned int leftLaneLineX1;
	unsigned int leftLaneLineY1;

	//右车道线两点
	unsigned int rightLaneLineX0;
	unsigned int rightLaneLineY0;

	unsigned int rightLaneLineX1;
	unsigned int rightLaneLineY1;
	
	//车道线报警信息
	unsigned char leftLaneLineWarn;//0-黄色（未检测到） 1-绿色（检测到） 2-红色（压线报警） 3-请重新标定摄像头
	unsigned char rightLaneLineWarn;//0-黄色（未检测到） 1-绿色（检测到） 2-红色（压线报警）3-请重新标定摄像头

	//车道线绘图信息===
	int    colorPointsNum;    //车道线条状块分割点的个数
	unsigned char dnColor;    //下方一块的颜色。 1-蓝，2-绿
	unsigned short rowIndex[MAX_INDEX_NUM];     //车道线条状块分割点的行坐标
	unsigned short ltColIndex[MAX_INDEX_NUM];   //车道线条状块左边分割点的列坐标
	unsigned short mdColIndex[MAX_INDEX_NUM];   //车道线条状块中间分割点的列坐标
	unsigned short rtColIndex[MAX_INDEX_NUM];   //车道线条状块右边分割点的列坐标
	//车道线绘图信息==END

	//=================================车辆信息
	//车辆位置信息
	unsigned int carX[MAX_CAR_NUM];
	unsigned int carY[MAX_CAR_NUM];
	unsigned int carW[MAX_CAR_NUM];
	unsigned int carH[MAX_CAR_NUM];

	float carTTC[MAX_CAR_NUM];
	float carDist[MAX_CAR_NUM];

	//车辆报警信息
	unsigned char carWarn[MAX_CAR_NUM];
	int carNum;

	//=================================车牌信息
	unsigned int plateX[MAX_PLATE_NUM];
	unsigned int plateY[MAX_PLATE_NUM];
	unsigned int plateW[MAX_PLATE_NUM];
	unsigned int plateH[MAX_PLATE_NUM];

	unsigned int plateNum;

	//=================================抠图位置
	//int cropEnable;
	//int roiX;
	//int roiY;
	//int roiH;
	//int roiW;
	//===========================================

}ADASOutData;


typedef void (*AdasIn)(ADASInData *ptrADASInData,void *dv);
typedef void (*AdasOut)(ADASOutData *ptrADASOutData,void *dv);

//初始化参数
typedef struct ADASPara 
{
	//图像宽度,高度
	unsigned int frameWidth;
	unsigned int frameHeight;

	//=================================抠图位置
	//int cropEnable;
	int roiX;
	int roiY;
	int roiH;
	int roiW;

	//video帧率
	unsigned int fps;

	//camera parameter相机参数
	unsigned int focalLength;//um
	unsigned int pixelSize;//um
	unsigned int horizonViewAngle;
	unsigned int verticalViewAngle;

	/***ADD***/
	unsigned int vanishX;//天际消失点坐标
	unsigned int vanishY;

	unsigned int hoodLineDist;//车头距离 cm
	unsigned int vehicleWidth;//车辆宽度 cm
	unsigned int wheelDist;//车轮距离  cm
	unsigned int cameraToCenterDist;//摄像头中心 cm

	unsigned int cameraHeight;//相机高度 cm
	unsigned int leftSensity;//0-1-2-3
	unsigned int rightSensity;//0-1-2-3
	unsigned int fcwSensity;//0-1-2-3
	
	//初始化回调函数
	AdasIn awAdasIn;
	AdasOut awAdasOut;

}ADASPara;

void AW_AI_ADAS_Init(ADASPara *ptrADASPara,void *dev);

void AW_AI_ADAS_UnInit();

#ifdef __cplusplus
}
#endif


#endif
