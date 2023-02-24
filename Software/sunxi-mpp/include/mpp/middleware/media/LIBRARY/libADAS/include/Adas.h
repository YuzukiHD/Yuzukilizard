#ifndef _ADAS_H_
#define _ADAS_H_

#define UNITEINTERFACE


typedef struct
{
	   union
	   {
		       int x;
		       int col;
	   };
	   union
	   {
		       int y;
		       int row;
	   };
}PointC;
typedef struct
{
	   union
	   {
		       int x;
		       int col;
	   };
	   union
	   {
		       int y;
		       int row;
	   };
	   union
	   {
		       int width;
		       int cols;
	   };
	   union
	   {
		       int height;
		       int rows;
	   };
}RectC;


typedef struct
{
	    union
	    {
		       int width;
		       int cols;
	    };
	    union
	    {
		       int height;
		       int rows;
	    };
}IMGSIZE;

typedef struct
{
	    unsigned char *ptr;
		IMGSIZE imgSize;
}MtxUchar;
typedef	  struct
{
	    unsigned short *ptr;
	    IMGSIZE imgSize;
}MtxUshort;
/*******************************************/
//输入参数
/*******************************************/
typedef	  struct
{
#ifdef UNITEINTERFACE
      int mode;             //模式选择,mode ==1时暂停检测，除v3之外的其他方案请置为 0
#endif
	  MtxUchar imgSmall;    //子码流图 960*540
	  MtxUchar imgBig;      //主码流图 1920*1080
	  struct
	  {
		      int     enable;//有无GPS信号
		      float   speed; //车速
	  }gps;
#ifdef UNITEINTERFACE
	  struct
	  {
		      unsigned char isStop; //本车是否停止
			  float         a;      //本车的加速度
			  float         v;      //本车的车速
	  }gsensor;//除v3之外的其他方案不必理会
	  struct
	  {

	  }obd;//OBD参数，预留
#endif
	  struct
	  {
		      int fcwSensity;//前车防撞灵敏度：0：高，1：中，2：低    默认值为1
		      int ldwSensity;//车道偏离灵敏度：0：高，1：中，2：低    默认值为1

	  }sensity;

	  int LV;  //光照强度

}FRAMEIN;



typedef	  struct
{
      unsigned char angH;      //镜头的水平视角
	  unsigned char angV;      //镜头的垂直视角

	  unsigned short widthOri; //原始的图像宽度
	  unsigned short heightOri;//原始的图像高度
}ViewAng;


/*******************************************/
//输出参数
/*******************************************/
typedef struct
{
	  RectC idx;                   //该车牌在图像上的位置(左上角坐标和宽高)
}PLATE;

typedef struct
{
	   int Num;                    //有多少车牌
	   PLATE *plateP;              //所有车牌的信息

	   struct
	   {
		        int   refresh;    //refresh为真时，更新车牌副本
		        int   isDisp;     //isDisp 为真时，显示车牌副本
	   }watermark;	//车牌水印
}PLATES;

typedef struct
{
	   unsigned char isWarn;     //该车是否报警
	   unsigned char color;      //该车需要涂上的颜色。0-不涂，1-黄色，2-红色
	   RectC idx;                //该车在图像上的位置(左上角坐标和宽高)
	   float dist;	             //该车到本车的距离
	   float time;	             //与该车的碰撞时间
}CAR;

typedef struct
{
	   unsigned char isDisp;     //是否显示车道线
	   unsigned char ltWarn;     //左车道线报警标记 ，128：压线(小声报警)。255：压线超过灵敏度设定的时长(大声报警)
	   unsigned char rtWarn;     //右车道线报警标记 ，128：压线(小声报警)。255：压线超过灵敏度设定的时长(大声报警)
	   PointC ltIdxs[2];         //左车道线在图像上的位置(由两个点确定)
	   PointC rtIdxs[2];         //右车道线在图像上的位置(由两个点确定)



	   int    colorPointsNum;    //车道线条状块分割点的个数
	   unsigned char dnColor;    //下方一块的颜色。 1-蓝，2-绿
	   unsigned short *rows;     //车道线条状块分割点的行坐标
	   unsigned short *ltCols;   //车道线条状块左边分割点的列坐标
	   unsigned short *mdCols;   //车道线条状块中间分割点的列坐标
	   unsigned short *rtCols;   //车道线条状块右边分割点的列坐标
}LANE;
typedef struct
{
	   int Num;                  //有多少辆车
	   CAR *carP;                //所有车辆的信息
}CARS;
typedef struct
{
	   unsigned char  isSave;    //该标记位为真时 vanishLine和table保存到外设。
	   unsigned short vanishLine;
	   unsigned char  vanishLineIsOk;
	   MtxUshort table;
}SAVEPARA;

typedef struct
{
	   LANE lane;                //车道线信息
	   CARS cars;	             //车辆信息
	   PLATES   plates;          //车牌信息
	   SAVEPARA savePara;
	   int  score;               //驾驶习惯打分
#ifndef UNITEINTERFACE
	   char *version;            //算法版本号
#endif

}ADASOUT;
/*******************************************/
//初始化输入参数
/*******************************************/

typedef struct
{
#ifdef UNITEINTERFACE
	   char     *version;                //算法版本号
	   struct
	   {
		      unsigned char isGps;       //有无gps
		      unsigned char isGsensor;   //有无Gsensor，出v3之外其他方案请置为0
		      unsigned char isObd;       //有无读取OBD
			  unsigned char isCalibrate; //是否进行安装标定
	   }config;
	   struct
	   {

	   }calibrate;//安装参数，预留
#endif
	  int fps;                          //帧率
	  IMGSIZE imgSizeSmall;             //子码流图 960*540
	  IMGSIZE imgSizeBig;               //主码流图 1920*1080

	  ViewAng           viewAng;	    //初始化的时候读入
	  unsigned short    vanishLine;     //初始化的时候读入
	  unsigned char     vanishLineIsOk;
	  MtxUshort         table;          //初始化的时候读入
	  void   *setPara;	                //留个空指针是算法可能要外面设置一些参数，取决于路测的结果，暂时定义不了
#ifdef UNITEINTERFACE
      struct
	  {
		      PointC ltLane[2]; //左边校准线的两个端点
			  PointC rtLane[2]; //右边校准线的两个端点
	  }adjustLane;//校准线,安装的时候，调整校准线和路上的车道线，使之尽量重合
#endif
}ADASIN;




/*******************************************/
typedef void (*AdasIn_v2)(FRAMEIN &frameIn,void *dv); //回调函数，给adas输入参数
typedef void (*AdasOut_v2)(ADASOUT &adasOut,void *dv);//回调函数，获取adas的计算结果
extern  AdasIn_v2  callbackIn;
extern  AdasOut_v2 callbackOut;
/*******************************************/
//接口函数
/*******************************************/
#ifdef UNITEINTERFACE
void InitialAdas(ADASIN &adasIn,void **dev);
#else
void InitialAdas(ADASIN adasIn,void **dev);
#endif
void ReleaseAdas();


#endif
