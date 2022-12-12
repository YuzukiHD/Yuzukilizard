#define LOG_NDEBUG 0
#define LOG_TAG "sample_adas_ui"
#include <utils/plat_log.h>
#include <sys/stat.h>
#include <cstdio>
#include <csignal>
#include <iostream>
#include <hwdisplay.h>
#include <stdio.h>
#include "sample_adas_ui.h"
#include "mm_comm_vo.h"
#include "fbtools.h"
#include "mpi_vo.h"
#include "audioCtrl.h"
#include <audio_hw.h>
#include <math.h>

#define TEST_TIMER  1001
static int num = 0;
static int screen_w, screen_h;
using namespace EyeseeLinux;
using namespace std;

SampleAdasUi::SampleAdasUi()
{
    mDAlign = 0;
    mFinishInit = false;
    screen_w = 640;
    screen_h = 480;
    mDistVal = -1;
    mWarnVal = -1;
    carX = -1;
    carY = -1;
    carW = -1;
    carH = -1;
    lx0 = -1;
    ly0 = -1;
    lx1 = -1;
    ly1 = -1;
    rx0 = -1;
    ry0 = -1;
    rx1 = -1;
    ry1 = -1;
    mfont = NULL;
    mlayer = -1;
    mUpdateUi = false;
    mNumShowTime = 0;
    mShowFullWarn = false;
    memset(&mBufDist,0,sizeof(mBufDist));
    memset(&mCarLines,0,sizeof(mCarLines));
    pthread_mutex_init(&adas_lock,NULL);
}

SampleAdasUi::~SampleAdasUi()
{
    unLoadImage();
    pthread_mutex_destroy(&adas_lock);
}
void SampleAdasUi::unLoadImage()
{
	int i ;
	for(i=0;i<WARN_IMAGE_NUMBER;i++)
		UnloadBitmap(&warnImage[i]);
	for(i=0;i<DIST_IMAGE_NUMBER;i++)
		UnloadBitmap(&distImage[i]);
	UnloadBitmap(&mAlignImage);

}

void SampleAdasUi::initWarnImage()
{
    int ret;
	char filepath[128]={0};
	for(int i =0;i<WARN_IMAGE_NUMBER;i++)
    {
		switch(i)
        {
			case CRASH_WARN:
                snprintf(filepath,sizeof(filepath)-1,"%s/crash_warn.jpg",RES_PATH);
				break;
			case LEFT_WARN_RED:
                snprintf(filepath,sizeof(filepath)-1,"%s/left_warn_red.jpg",RES_PATH);
				break;
            case LEFT_WARN_GREEN:
                snprintf(filepath,sizeof(filepath)-1,"%s/left_warn_green.jpg",RES_PATH);
				break;
            case LEFT_WARN_YELLOW:
                snprintf(filepath,sizeof(filepath)-1,"%s/left_warn_yellow.jpg",RES_PATH);
				break;
			case RIGHT_WARN_RED:
                snprintf(filepath,sizeof(filepath)-1,"%s/right_warn_red.jpg",RES_PATH);
				break;
            case RIGHT_WARN_GREEN:
                snprintf(filepath,sizeof(filepath)-1,"%s/right_warn_green.jpg",RES_PATH);
				break;
            case RIGHT_WARN_YELLOW:
                snprintf(filepath,sizeof(filepath)-1,"%s/right_warn_yellow.jpg",RES_PATH);
				break;
			case LEFT_CRASH_WARN:
                snprintf(filepath,sizeof(filepath)-1,"%s/left_crash_warn.jpg",RES_PATH);
				break;
			case RIGHT_CRASH_WARN:
                snprintf(filepath,sizeof(filepath)-1,"%s/right_crash_warn.jpg",RES_PATH);
				break;
			default:
                snprintf(filepath,sizeof(filepath)-1,"%s/crash_warn.jpg",RES_PATH);
				break;	
		}
		ret = LoadBitmapFromFile(HDC_SCREEN, &warnImage[i], filepath);
		if(ret!= ERR_BMP_OK)
        {
			alogd("load the  %s %d warn bitmap error",filepath, ret);
		}	
	}
}
void SampleAdasUi::showWarnBitmap(HDC hdc,int warn,int x,int y)
{
    BITMAP *Image;
	Image=&warnImage[warn];
    FillBoxWithBitmap(hdc,x,y,Image->bmWidth,Image->bmHeight,Image);
}


void SampleAdasUi::initDistImage()
{
    char filepath[128]={0};
    snprintf(filepath,sizeof(filepath)-1,"%s/dist_",RES_PATH);

	char fpath[64]={0};
	int ret;
	for(int i =0;i<DIST_IMAGE_NUMBER;i++){
		sprintf(fpath,"%s%d.png",filepath,i+1);
		//alogd("<***fpath=%s*****filepath=%s**>",fpath,filepath);
		ret = LoadBitmapFromFile(HDC_SCREEN, &distImage[i], fpath);
		if(ret!= ERR_BMP_OK){
			alogd("load the %d Dist bitmap error",ret);	
		}
	}
}
int SampleAdasUi::showDistImage(HDC hdc,int index_,int x,int y)
{
    //alogd("距离前车车距还有:%d m", index_);
	if(index_ >DIST_IMAGE_NUMBER || index_ < 1){
        return -1;
	}
	BITMAP *Image;
	Image = &distImage[index_-1];
    FillBoxWithBitmap(hdc,x,y,Image->bmWidth,Image->bmHeight,Image);
    
}

void SampleAdasUi::initAlignImage()
{
    //crash_warn
	char filepath[128]={0};
    snprintf(filepath,sizeof(filepath)-1,"%s/adas_calibration_line.png",RES_PATH);
	int ret = LoadBitmapFromFile(HDC_SCREEN, &mAlignImage, filepath);
	if(ret!= ERR_BMP_OK){
		aloge("load the adas_calibration_line bitmap error");
	}
}
void SampleAdasUi::showAlignImage(HDC hdc,int x,int y)
{

    //alogd("showAlignImage w = %d h=%d",mAlignImage.bmWidth,mAlignImage.bmHeight);
    FillBoxWithBitmap(hdc,x,y,mAlignImage.bmWidth,mAlignImage.bmHeight,&mAlignImage);
}

//绘画矫正的线
void SampleAdasUi::drawALignLine(HDC hdc,int subW_,int subH_)
{
    //alogd("habo---> drawALignLine");
	float midRow = (float)65 * subH_ / 100;
	float midCol = (float)subW_ / 2;
	float vLen = (float)subH_ -midRow - 2;
	float ang = (float)(55.0 / 180 * PI);
	float hLen = (float)((float)vLen * tan(ang));
	
	int UpX = convertCoordinate(screen_w, subW_, midCol);
	int UpY = convertCoordinate(screen_h-OFFSET_SH, subH_, midRow);
	int leftDnX = convertCoordinate(screen_w, subW_, midCol - hLen);
	int leftDnY = convertCoordinate(screen_h-OFFSET_SH, subH_, midRow + vLen);
	int rightDnX = convertCoordinate(screen_w, subW_, midCol + hLen);
	int rightDnY = convertCoordinate(screen_h-OFFSET_SH, subH_, midRow + vLen);		

	SetPenWidth(hdc,5);
	SetPenColor(hdc, RGBA2Pixel(hdc, 0x00, 0x00, 0xff, 0xff));
	LineEx(hdc,UpX, UpY, leftDnX, leftDnY);
	LineEx(hdc,UpX, UpY, rightDnX, rightDnY);
}


//绘画本车行驶的道路线
void SampleAdasUi::drawRoadLine(HDC hdc,int x1,int y1,int x2,int y2)
{
	SetPenWidth (hdc, 5);
    SetPenColor(hdc, RGBA2Pixel(hdc, 0x00, 0xff, 0x00, 0xff));
	LineEx (hdc, x1, y1, x2, y2);
}

//绘画边框检查到的车辆
void SampleAdasUi::drawRectangle(HDC hdc,int x0, int y0, int x1, int y1)
{
    SetPenWidth(hdc, 3);
    SetPenColor(hdc, RGBA2Pixel(hdc, 0xff, 0x00, 0x00, 0xff));
    //Rectangle(hdc,x0,y0,x0+x1,y0+y1);
    LineEx (hdc, x0, y0, x0+x1, y0);
    LineEx (hdc, x0, y0, x0, y0+y1);
    LineEx (hdc, x0+x1, y0, x0+x1, y0+y1);
    LineEx (hdc, x0, y0+y1, x0+x1, y0+y1);
}
int SampleAdasUi::convertCoordinate(int dispVal, int imgVal, int val)
{
    	return (int)(1.0 * dispVal * val / imgVal);
}

void SampleAdasUi::drawLines(HDC hdc,int x1,int y1,int x2,int y2)
{
    SetPenWidth(hdc, 3);
    SetPenColor(hdc, RGBA2Pixel(hdc, 0xff, 0x00, 0x00, 0xff));
	//MoveTo(hdc,x1,y1);
	//LineTo(hdc,x2,y2);
    LineEx (hdc, x1, y1, x2, y2);
}

//计算车距
float SampleAdasUi::awDistVehicleBottomY(int vanishY,int bottomY,int srcHeight,float cameraHeight)
{
	float k,b;
	float w;
	float dist;
	k = -0.2548*(cameraHeight/100)+0.703;//输入单位:cm
	b = -k*(float)vanishY;
	w = k*(float)bottomY+b;
	dist = 567.2069/(720.0*w/(float)srcHeight+3.0285)-0.5;//(单位：m)
	return dist;
}

void SampleAdasUi::testLogOnScreen(HDC hdc,int carNum,int rwarn_,int lwarn_,int carwarn_)
{
    char buf[1024]={0};
    RECT rect_;
    rect_.left = 0;
    rect_.top = 5;
    rect_.right = rect_.left+640;
    rect_.bottom = rect_.top+360;
    if(carNum == 0)
        snprintf(buf,sizeof(buf),"Rwarn:%d    Lwarn:%d    CarWarn:%d",rwarn_,lwarn_,carwarn_);
     else if(carNum == 1)
        snprintf(buf,sizeof(buf),"Rwarn:%d    Lwarn:%d    CarWarn:%d    Car[0]:%d",rwarn_,lwarn_,carwarn_,mBufDist[0]);
     else if(carNum == 2)
        snprintf(buf,sizeof(buf),"Rwarn:%d    Lwarn:%d    CarWarn:%d    Car[0]:%d    Car[1]:%d",rwarn_,lwarn_,carwarn_,mBufDist[0],mBufDist[1]);
     else if(carNum == 3)
        snprintf(buf,sizeof(buf),"Rwarn:%d    Lwarn:%d    CarWarn:%d    Car[0]:%d    Car[1]:%d    Car[2]:%d",rwarn_,lwarn_,carwarn_,mBufDist[0],mBufDist[1],mBufDist[2]);
     else if(carNum == 4)
        snprintf(buf,sizeof(buf),"Rwarn:%d    Lwarn:%d    CarWarn:%d    Car[0]:%d    Car[1]:%d    Car[2]:%d    Car[3]:%d",rwarn_,lwarn_,carwarn_,mBufDist[0],mBufDist[1],mBufDist[2],mBufDist[3]);
     else if(carNum == 5)
        snprintf(buf,sizeof(buf),"Rwarn:%d    Lwarn:%d    CarWarn:%d    Car[0]:%d    Car[1]:%d    Car[2]:%d    Car[3]:%d    Car[4]:%d",rwarn_,lwarn_,carwarn_,mBufDist[0],mBufDist[1],mBufDist[2],mBufDist[3],mBufDist[4]);
     else if(carNum == 6)
        snprintf(buf,sizeof(buf),"Rwarn:%d    Lwarn:%d    CarWarn:%d    Car[0]:%d    Car[1]:%d    Car[2]:%d    Car[3]:%d    Car[4]:%d    Car[5]:%d",rwarn_,lwarn_,carwarn_,mBufDist[0],mBufDist[1],mBufDist[2],mBufDist[3],mBufDist[4],mBufDist[5]);
     else if(carNum == 7)
        snprintf(buf,sizeof(buf),"Rwarn:%d    Lwarn:%d    CarWarn:%d    Car[0]:%d    Car[1]:%d    Car[2]:%d    Car[3]:%d    Car[4]:%d    Car[5]:%d    Car[6]:%d",rwarn_,lwarn_,carwarn_,mBufDist[0],mBufDist[1],mBufDist[2],mBufDist[3],mBufDist[4],mBufDist[5],mBufDist[6]);
     else if(carNum == 8)
        snprintf(buf,sizeof(buf),"Rwarn:%d    Lwarn:%d    CarWarn:%d    Car[0]:%d    Car[1]:%d    Car[2]:%d    Car[3]:%d    Car[4]:%d    Car[5]:%d    Car[6]:%d    Car[7]:%d",rwarn_,lwarn_,carwarn_,mBufDist[0],mBufDist[1],mBufDist[2],mBufDist[3],mBufDist[4],mBufDist[5],mBufDist[6],mBufDist[7]); 
     else if(carNum == 9)
        snprintf(buf,sizeof(buf),"Rwarn:%d    Lwarn:%d    CarWarn:%d    Car[0]:%d    Car[1]:%d    Car[2]:%d    Car[3]:%d    Car[4]:%d    Car[5]:%d    Car[6]:%d    Car[7]:%d    Car[8]:%d",rwarn_,lwarn_,carwarn_,mBufDist[0],mBufDist[1],mBufDist[2],mBufDist[3],mBufDist[4],mBufDist[5],mBufDist[6],mBufDist[7],mBufDist[8]);
     else if(carNum == 10)
        snprintf(buf,sizeof(buf),"Rwarn:%d    Lwarn:%d    CarWarn:%d    Car[0]:%d    Car[1]:%d    Car[2]:%d    Car[3]:%d    Car[4]:%d    Car[5]:%d    Car[6]:%d    Car[7]:%d    Car[8]:%d    Car[9]:%d",rwarn_,lwarn_,carwarn_,mBufDist[0],mBufDist[1],mBufDist[2],mBufDist[3],mBufDist[4],mBufDist[5],mBufDist[6],mBufDist[7],mBufDist[8],mBufDist[9]); 

     SetBkMode(hdc, BM_TRANSPARENT);
     SetTextColor(hdc, PIXEL_red);
     DrawText(hdc, buf, -1, &rect_,DT_LEFT|DT_WORDBREAK);
    //alogd("buf:%s",buf);
}
void SampleAdasUi::HandleAdasOutMsg(AW_AI_ADAS_DETECT_R_ DADASOutData)
{
    if(!mFinishInit) 
    {
        //alogw("mFinishInit is no ready !!!");
        return;
    }

    pthread_mutex_lock(&adas_lock);

   /***********根据打印测试,1S回调有100次左右，过滤掉部分消息,变为1S进来一次******************/
    if(++num < 100/1)
    {
        //alogd("habo--->---------- num = %d",num);
        pthread_mutex_unlock(&adas_lock);
        return;
    }
    //alogd("habo--->---------- num---- = %d",num);
    num = 0;
    
    
    /**************处理如果显示了警报的图片，让show的时间停留2S************************/
    if(mShowFullWarn == true)
    {
        mNumShowTime++;
        if(mNumShowTime >= 10*1)
        {
           // alogd("habo--->---------- mNumShowTime = %d",mNumShowTime);
            mShowFullWarn = false;
            mNumShowTime = 0;
        }
         else
         {
            //alogd("habo---> mNumShowTime = %d",mNumShowTime);
            pthread_mutex_unlock(&adas_lock);
            return;
         }
    }
    /***********************拷贝adas数据，发送清空界面和更新绘画的消息******************************************/
    memset(&mADASOutData, 0, sizeof(AW_AI_ADAS_DETECT_R_));
    memcpy(&mADASOutData, &DADASOutData, sizeof(AW_AI_ADAS_DETECT_R_));
    SendMessage(mhMainWnd,MSG_ID_CLEAN_ALL,0,0);
}

void SampleAdasUi::HandleAdasUi(HDC hdc,AW_AI_ADAS_DETECT_R_ DADASOutData)
{
	int i = 0;
	bool carwarn=false;
	int lwarn = LANELINE_INVAILED, rwarn = LANELINE_INVAILED;
	float carWarnTime;
	float carWarnDist;
 /****************************绘画矫正的线或者show矫正的图片*********************************/
#ifdef DRAW_ALIGN_IMAGE
	if(mDAlign < SHOW_ALIGNIMAGE_TIME)
    {

		if(mDAlign < SHOW_ALIGNIMAGE_TIME -1)
		{
           showAlignImage(hdc,0,0);
		}
		mDAlign ++;
	}
#else
    if(mDAlign < SHOW_ALIGNIMAGE_TIME)
    {

		if(mDAlign < SHOW_ALIGNIMAGE_TIME -1)
		{
            drawALignLine(hdc,DADASOutData.subWidth,DADASOutData.subHeight);
		}
		mDAlign ++;
	}
#endif


/********************************检查车辆相关参数**********************************/
#ifdef DRAW_CAR_DIST_RECT  
	for (i = 0; i < DADASOutData.nADASOutData.carNum &&i < CAR_NUMBER ; ++i)//
    {
       //检车到车的大小已经超出了屏幕显示的范围，就不用继续往下执行
		if(DADASOutData.nADASOutData.carX[i] + DADASOutData.nADASOutData.carW[i] > DADASOutData.subWidth ||
			DADASOutData.nADASOutData.carY[i] + DADASOutData.nADASOutData.carH[i] > DADASOutData.subHeight)
	    {
            break;
        }
        carX = convertCoordinate(screen_w, DADASOutData.subWidth,DADASOutData.nADASOutData.carX[i]);
        carY = convertCoordinate(screen_h-OFFSET_SH, DADASOutData.subHeight,DADASOutData.nADASOutData.carY[i]);
        carW = convertCoordinate(screen_w, DADASOutData.subWidth,DADASOutData.nADASOutData.carW[i]);
        carH = convertCoordinate(screen_h-OFFSET_SH, DADASOutData.subHeight,DADASOutData.nADASOutData.carH[i]);
        //alogd("绘画车的边框--[%d] x = %d y=%d w=%d h=%d\n",i,carX,carY,carW,carH);
        #if 0 //绘画整个方框方式
        drawRectangle(hdc,carX,carY,carW,carH);
        #else //绘画4个角方式
        int len = (int)(carW * 1.0 / 6);
        mCarLines[0] = carX;
        mCarLines[1] = carY;
        mCarLines[2] = carX + len;
        mCarLines[3] = carY;
        mCarLines[4] = carX;
        mCarLines[5] = carY;
        mCarLines[6] = carX;
        mCarLines[7] = carY + len;
        mCarLines[8] = carX + carW - len;
        mCarLines[9] = carY;
        mCarLines[10] = carX + carW;
        mCarLines[11] = carY;
        mCarLines[12] = carX + carW;
        mCarLines[13] = carY;
        mCarLines[14] = carX + carW;
        mCarLines[15] = carY + len;
        mCarLines[16] = carX + carW - len;
        mCarLines[17] = carY + carH;
        mCarLines[18] = carX + carW;
        mCarLines[19] = carY + carH;
        mCarLines[20] = carX + carW;
        mCarLines[21] = carY + carH - len;
        mCarLines[22] = carX + carW;
        mCarLines[23] = carY + carH;
        mCarLines[24] = carX;
        mCarLines[25] = carY + carH;
        mCarLines[26] = carX + len;
        mCarLines[27] = carY + carH;
        mCarLines[28] = carX;
        mCarLines[29] = carY + carH - len;
        mCarLines[30] = carX;
        mCarLines[31] = carY + carH;
        for(int j =3;j<32;j+=4)
        {
			drawLines(hdc,mCarLines[j-3],mCarLines[j-2],mCarLines[j-1],mCarLines[j]);
		}	
        #endif
		if(DADASOutData.nADASOutData.carWarn[i] == CAR_WARN_LINE_COLLISION)//碰撞预警，只要检测到任意一辆车有碰撞预警，就置carwarn=true
        {
			carwarn=true;
			carWarnDist = DADASOutData.nADASOutData.carDist[i];//车距
			carWarnTime = DADASOutData.nADASOutData.carTTC[i];//距离碰撞时间
			//alogd("habo--> carWarnDist[%d]=%d  carWarnTime[%d]=%d",i,carWarnDist,i,carWarnTime);
		}

        //计算车距并且show
        mDistVal = (int)awDistVehicleBottomY(DADASOutData.subHeight/2+10,DADASOutData.nADASOutData.carY[i] + DADASOutData.nADASOutData.carH[i], DADASOutData.subWidth, 120);
        #ifdef DRAW_LOG_ON_SCREEN
        mBufDist[i]=mDistVal;
        #endif
        //alogd("距离前车:[%d]_车距还有:%d m",i,mDistVal);
        showDistImage(hdc,mDistVal,carX,carY+carH+5);
	}
#endif



	/********************************绘画本车道路线************************************/
 #ifdef DRAW_ROAD_LINE
    
    	lx0 = DADASOutData.nADASOutData.leftLaneLineX0;
    	ly0 = DADASOutData.nADASOutData.leftLaneLineY0;
    	lx1 = DADASOutData.nADASOutData.leftLaneLineX1;
    	ly1 = DADASOutData.nADASOutData.leftLaneLineY1;
    	rx0 = DADASOutData.nADASOutData.rightLaneLineX0;
    	ry0 = DADASOutData.nADASOutData.rightLaneLineY0;
    	rx1 = DADASOutData.nADASOutData.rightLaneLineX1;
    	ry1 = DADASOutData.nADASOutData.rightLaneLineY1;
    	lx0 = convertCoordinate(screen_w, DADASOutData.subWidth, lx0);
    	ly0 = convertCoordinate(screen_h-OFFSET_SH, DADASOutData.subHeight, ly0);
    	lx1 = convertCoordinate(screen_w, DADASOutData.subWidth, lx1);
    	ly1 = convertCoordinate(screen_h-OFFSET_SH, DADASOutData.subHeight, ly1);
    	rx0 = convertCoordinate(screen_w, DADASOutData.subWidth, rx0);
    	ry0 = convertCoordinate(screen_h-OFFSET_SH, DADASOutData.subHeight, ry0);
    	rx1 = convertCoordinate(screen_w, DADASOutData.subWidth, rx1);
    	ry1 = convertCoordinate(screen_h-OFFSET_SH, DADASOutData.subHeight, ry1);
    	drawRoadLine(hdc,lx0,ly0,lx1,ly1);
    	drawRoadLine(hdc,rx0,ry0,rx1,ry1);
#endif

    /*************************检查是否有偏移警报**************************/
    //alogd("habo--> leftLaneLineWarn = %d rightLaneLineWarn = %d carwarn=%d",DADASOutData.leftLaneLineWarn,DADASOutData.rightLaneLineWarn,carwarn);
	if(DADASOutData.nADASOutData.leftLaneLineWarn != LANELINE_INVAILED)
    {
		lwarn = DADASOutData.nADASOutData.leftLaneLineWarn;
	}
    else
    {
		if(DADASOutData.nADASOutData.rightLaneLineWarn !=LANELINE_INVAILED)
         {
			rwarn = DADASOutData.nADASOutData.rightLaneLineWarn;
		 }
	}


#ifdef DRAW_LOG_ON_SCREEN
        testLogOnScreen(hdc,DADASOutData.nADASOutData.carNum,rwarn,lwarn,(int)carwarn);       
#endif

#ifdef DRAW_ADAS_WARN
    /***********************************绘画警报图和播放语音***********************************/
    //alogd("habo--> lwarn = %d rwarn = %d carwarn=%d",lwarn,rwarn,carwarn);
	if(lwarn != LANELINE_INVAILED && carwarn)//左边车请保持车距
    {
        alogd("显示 LEFT_CRASH_WARN 图片");
        showWarnBitmap(hdc,LEFT_CRASH_WARN,0,0);
        mShowFullWarn = true;
        AudioCtrl::GetInstance()->PlaySound(AudioCtrl::ADAS_SOUND_CRASH_WARN);
	} 
    else if (rwarn != LANELINE_INVAILED && carwarn)//右边车请保持车距
    {
        alogd("显示 RIGHT_CRASH_WARN 图片");
        showWarnBitmap(hdc,RIGHT_CRASH_WARN,0,0);
        AudioCtrl::GetInstance()->PlaySound(AudioCtrl::ADAS_SOUND_CRASH_WARN);
        mShowFullWarn = true;
	}
    else if(lwarn  == LANELINE_CRIMPING_ALARM)
    {
        alogd("左车道检查到压线，显示红色图片");
        showWarnBitmap(hdc,LEFT_WARN_RED,0,0);
        AudioCtrl::GetInstance()->PlaySound(AudioCtrl::ADAS_SOUND_LEFT_CRASH_WARN);
        mShowFullWarn = true;
    }
    else if(rwarn == LANELINE_CRIMPING_ALARM)
    {
        alogd("右车道检查到压线，显示红色图片");
        showWarnBitmap(hdc,RIGHT_WARN_RED,0,0);
        AudioCtrl::GetInstance()->PlaySound(AudioCtrl::ADAS_SOUND_RIGHT_CRASH_WARN);
        mShowFullWarn = true;
    }
    else if(carwarn)//正前方车距警报
    {
        alogd("显示CRASH_WARN 图片");
	    showWarnBitmap(hdc,CRASH_WARN,0,0);
        AudioCtrl::GetInstance()->PlaySound(AudioCtrl::ADAS_SOUND_CRASH_WARN);
        mShowFullWarn = true;
	}
#endif
    pthread_mutex_unlock(&adas_lock);
}


static long int AdasUiProc(HWND hWnd, unsigned int message, WPARAM wParam, LPARAM lParam)
{
   
    SampleAdasUi* mSampleAdasUi = (SampleAdasUi*)GetWindowAdditionalData(hWnd);
	switch (message)
	{

    	case MSG_CREATE:
    	{
            alogd("habo---> ready to MSG_CREATE");
          /// SetTimer(hWnd,TEST_TIMER,100);//1s
           
           mSampleAdasUi->initAlignImage();
           mSampleAdasUi->initWarnImage();
           mSampleAdasUi->initDistImage();
           
    	}break;
    	case MSG_PAINT: 
    	{
           HDC hdc = 0;
    	   hdc = BeginPaint(hWnd);
           if(mSampleAdasUi->mFinishInit)
           {
               if(mSampleAdasUi->mUpdateUi == true)
                {
                    mSampleAdasUi->HandleAdasUi(hdc,mSampleAdasUi->mADASOutData);
                    mSampleAdasUi->mUpdateUi = false;
                }
           }
          EndPaint(hWnd, hdc);
    	  return 0;
    	}
       case MSG_ID_CLEAN_ALL:
       {
            //alogd("habo---> MSG_ID_CLEAN_ALL");
             mSampleAdasUi->mUpdateUi = true;
            ::InvalidateRect(hWnd,NULL, TRUE);
            return 0;
       }
       case MSG_TIMER:
       {
       
       }break;
    	case MSG_DESTROY:
            KillTimer(hWnd,TEST_TIMER);
    		DestroyAllControls(hWnd);
    		return 0;
    	case MSG_CLOSE:
            KillTimer(hWnd,TEST_TIMER);
    		DestroyMainWindow(hWnd);
    		break;
        default:
            break;
	}
	return DefaultMainWinProc(hWnd, message, wParam, lParam);
}
int SampleAdasUi::MiniGuiInit(int args, const char* argv[])
{
    int ret = -1;
   
    AW_MPI_VO_Enable(0);
    mlayer= HLAY(2, 0);
    AW_MPI_VO_AddOutsideVideoLayer(mlayer);
    AW_MPI_VO_SetVideoLayerPriority(mlayer, ZORDER_MAX);
    VO_VIDEO_LAYER_ALPHA_S stAlpha;
    stAlpha.mAlphaMode = 0;
    stAlpha.mAlphaValue = 150;
    ret = AW_MPI_VO_SetVideoLayerAlpha(mlayer, &stAlpha);
    if (ret != RET_OK) 
    {
        aloge("set failed, hlay: %d, alpha: %d",mlayer,stAlpha.mAlphaValue);
        return ret;
    }

    VO_VIDEO_LAYER_ATTR_S stLayerAttr;
    AW_MPI_VO_GetVideoLayerAttr(mlayer, &stLayerAttr);
    stLayerAttr.stDispRect.X = 0;
    stLayerAttr.stDispRect.Y = 0;
    stLayerAttr.stDispRect.Width =screen_h ;
    stLayerAttr.stDispRect.Height =screen_w ;
    alogd("set ui layer rect, rect:[%d, %d, %d, %d]",
            stLayerAttr.stDispRect.X, stLayerAttr.stDispRect.X,
            stLayerAttr.stDispRect.Width, stLayerAttr.stDispRect.Height);
    ret = AW_MPI_VO_SetVideoLayerAttr(mlayer, &stLayerAttr);
    if(ret != RET_OK)
    {
        aloge("set ui layer rect failed");
        return ret;
    }
    AW_MPI_VO_OpenVideoLayer(mlayer);
    AudioCtrl::GetInstance()->InitPlayer(0);
    AudioCtrl::GetInstance()->SetBeepToneVolume(100);

    #if 0
    //init minigui
    if (InitGUI (args, argv) != 0) {
        alogd("init minigui fail !!! ");
        return -1;
    }
    #endif
    if(createWindow()<0)
    {
        alogd("create window fail");
        return -1;
    }
    return 0;
}



int SampleAdasUi::createWindow()
{
    setenv("FB_SYNC", "1", 1);
    setenv("SCREEN_INFO", "480x640-32bpp", 1);
    system("export FB_SYNC=1");

    //创建主窗口
    int y_pos = 60;//+60 是因为这个屏幕是480 高度，实际显示是360，上下都没有了60，所以y要向下偏移60
    int x_pos = 0;
    screen_w = GetGDCapability(HDC_SCREEN, GDCAP_MAXX) + 1;
    screen_h = GetGDCapability(HDC_SCREEN, GDCAP_MAXY) + 1;
    aloge("LCD width & height: (%d, %d)\n", screen_w, screen_h);

	MAINWINCREATE CreateInfo;
	CreateInfo.dwStyle = WS_VISIBLE;
	CreateInfo.dwExStyle = WS_EX_NONE | WS_EX_TROUNDCNS | WS_EX_BROUNDCNS|WS_EX_TRANSPARENT|WS_EX_TOPMOST;
	CreateInfo.spCaption = "adas";
	CreateInfo.hMenu = 0;
	//CreateInfo.hCursor = GetSystemCursor(0);
	CreateInfo.hIcon = 0;
	CreateInfo.MainWindowProc = AdasUiProc;
	CreateInfo.lx = x_pos;
	CreateInfo.ty = y_pos;
	CreateInfo.rx = screen_w;
	CreateInfo.by = screen_h-y_pos;
	CreateInfo.iBkColor = 0x00000000;//RGBA2Pixel(HDC_SCREEN, 0x00, 0x00, 0x00, 0x00);
	CreateInfo.dwAddData = (DWORD)this;
	CreateInfo.hHosting = HWND_DESKTOP;
    
    mfont = CreateLogFont("sxf", "arialuni", "UTF-8",
	  FONT_WEIGHT_REGULAR, FONT_SLANT_ROMAN, FONT_SETWIDTH_NORMAL,
	  FONT_OTHER_AUTOSCALE, FONT_UNDERLINE_NONE, FONT_STRUCKOUT_NONE, 26, 0);
    
    mhMainWnd = CreateMainWindow(&CreateInfo);
	if (mhMainWnd == HWND_INVALID)
		return -1;
    SetWindowFont(mhMainWnd,mfont);
    ShowWindow(mhMainWnd, SW_SHOWNORMAL);
    fb_clean();
    mFinishInit = true;
	return 0;
}
void SampleAdasUi::LoopUi()
{
    MSG Msg;
    while (GetMessage(&Msg, mhMainWnd)) 
    {
		TranslateMessage(&Msg);
		DispatchMessage(&Msg);
	}
    MainWindowThreadCleanup(mhMainWnd);
}

