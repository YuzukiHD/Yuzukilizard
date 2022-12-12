#ifndef _SAMPLE_ADAS_UI_H_
#define _SAMPLE_ADAS_UI_H_
#include <minigui/common.h>
#include <minigui/minigui.h>
#include <minigui/gdi.h>
#include <minigui/window.h>
#include <minigui/control.h>
#include "ion_memmanager.h"

#include <linux/fb.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <fcntl.h>
#include <string>
#include <sc_interface.h>
#include <AdasData.h>
#include <math.h>
#include <video/sunxi_display2.h>

#define SUB_WIDTH  640
#define SUB_HEIGHT 360
#define WARN_IMAGE_NUMBER 9
#define DIST_IMAGE_NUMBER 45
#define SHOW_ALIGNIMAGE_TIME	2*60
#define CAR_NUMBER  10
#define OFFSET_SH 60*2
#define PI 3.14159

#define DRAW_ADAS_WARN
//#define DRAW_ALIGN_IMAGE
#define DRAW_ROAD_LINE
#define DRAW_CAR_DIST_RECT
#define DRAW_LOG_ON_SCREEN
#define RES_PATH "/usr/share/minigui/res/adas"// "/mnt/extsd/adas"


enum {
	CRASH_WARN=0,//前方保持车距
	LEFT_WARN_RED,
	LEFT_WARN_GREEN,
	LEFT_WARN_YELLOW,
	RIGHT_WARN_RED,
	RIGHT_WARN_GREEN,
	RIGHT_WARN_YELLOW,
	LEFT_CRASH_WARN,//left保持车距
	RIGHT_CRASH_WARN,//right 保持车距
	NO_WARN,
};

typedef enum LaneLine
 {
   LANELINE_NO_DETECT = 0,//0-黄色-未检测到
   LANELINE_DETECTED,//1-绿色-检测到 
   LANELINE_CRIMPING_ALARM,//2-红色-压线报警 
   LANELINE_INVAILED,//3-请重新标定摄像头
 };
   
typedef enum CarWarn
{
  CAR_WARN_INVAILED = 0,
  CAR_WARN_LINE_DEPARTURE,//车道偏移预警
  CAR_WARN_LINE_COLLISION,//碰撞预警

};

typedef enum CtrlId
{
    ID_DIST_ICON = 200,
    ID_FULL_ICON,
    ID_ALIGN_ICON,
    
};

typedef enum MsgId{
    MSG_ID_CLEAN_ALL = 300,
};
    typedef struct _ADASOutData_
{
	unsigned int leftLaneLineX0;
	unsigned int leftLaneLineY0;
	unsigned int leftLaneLineX1;
	unsigned int leftLaneLineY1;
	unsigned int rightLaneLineX0;
	unsigned int rightLaneLineY0;

	unsigned int rightLaneLineX1;
	unsigned int rightLaneLineY1;
	unsigned char leftLaneLineWarn;
	unsigned char rightLaneLineWarn;
	int    colorPointsNum; 
	unsigned char dnColor;
	unsigned short rowIndex[MAX_INDEX_NUM]; 
	unsigned short ltColIndex[MAX_INDEX_NUM];
	unsigned short mdColIndex[MAX_INDEX_NUM]; 
	unsigned short rtColIndex[MAX_INDEX_NUM];
	unsigned int carX[MAX_CAR_NUM];
	unsigned int carY[MAX_CAR_NUM];
	unsigned int carW[MAX_CAR_NUM];
	unsigned int carH[MAX_CAR_NUM];
	float carTTC[MAX_CAR_NUM];
	float carDist[MAX_CAR_NUM];
	unsigned char carWarn[MAX_CAR_NUM];
	int carNum;
	unsigned int plateX[MAX_PLATE_NUM];
	unsigned int plateY[MAX_PLATE_NUM];
	unsigned int plateW[MAX_PLATE_NUM];
	unsigned int plateH[MAX_PLATE_NUM];
	unsigned int plateNum;

}ADASOutData_;
class SampleAdasUi
{
public:
    SampleAdasUi();
    ~SampleAdasUi();
    void initWarnImage();
    void initDistImage();
    int MiniGuiInit(int args, const char* argv[]);
    void initAlignImage();
    void showAlignImage(HDC hdc,int x,int y);
    int showDistImage(HDC hdc,int index_,int x,int y);
	void unLoadImage();
    void showWarnBitmap(HDC hdc,int warn,int x,int y);
    void HandleAdasOutMsg(AW_AI_ADAS_DETECT_R_ DADASOutData);
    void HandleAdasUi(HDC hdc,AW_AI_ADAS_DETECT_R_ DADASOutData);
    int createWindow();
    void LoopUi();
    float awDistVehicleBottomY(int vanishY,int bottomY,int srcHeight,float cameraHeight);

    void drawLines(HDC hdc,int x1,int y1,int x2,int y2);
    void drawALignLine(HDC hdc,int subW_,int subH_);
    void drawRoadLine(HDC hdc,int x1,int y1,int x2,int y2);
    void drawRectangle(HDC hdc,int x0, int y0, int x1, int y1);
    int convertCoordinate(int dispVal, int imgVal, int val);
    void testLogOnScreen(HDC hdc,int carNum,int rwarn_,int lwarn_,int carwarn_);
private:
    BITMAP	warnImage[WARN_IMAGE_NUMBER];
    BITMAP distImage[DIST_IMAGE_NUMBER];
    int mDAlign;
    int mlayer;
public:
	BITMAP	mAlignImage;
    HWND mhMainWnd;
    bool mFinishInit;
    BITMAP mDistImage;
    int mDistVal;
    int mWarnVal;
    int carX,carY,carW,carH;
    int mCarLines[32];
    int lx0,ly0,lx1,ly1,rx0,ry0,rx1,ry1;
    int mBufDist[10];
    LOGFONT *mfont;
    AW_AI_ADAS_DETECT_R_ mADASOutData;
    bool mUpdateUi;
    pthread_mutex_t	adas_lock;
    int mNumShowTime;
    bool mShowFullWarn;
};

#endif  /* _SAMPLE_ADAS_UI_H_ */

