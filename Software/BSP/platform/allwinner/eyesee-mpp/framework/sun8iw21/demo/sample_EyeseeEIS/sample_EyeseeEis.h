#ifndef __SAMPLE_EYESEEEIS_H__
#define __SAMPLE_EYESEEEIS_H__

#include <string>
#include <deque>

#include <plat_type.h>
#include <tsemaphore.h>
#include <mpi_sys.h>
#include <mpi_clock.h>
#include <mpi_vo.h>
#include <PictureRegionCallback.h>

#include <Errors.h>
#include <EyeseeISE.h>
#include "sample_EyeseeEis_config.h"

#define DEFAULT_SAMPLE_EYESEEEIS_CONF_PATH  "/mnt/extsd/sample_EyeseeEIS/sample_EyeseeEis.conf"


typedef struct SampleEyeseeEISCmdLineParam_s
{
    std::string mConfigFilePath;
}SampleEyeseeEisCmdLineParam;

class SampleEyeseeEisContext;

// define listener for EyeseeCamara 
class EyeseeCameraListener : public EyeseeLinux::EyeseeCamera::InfoCallback,
								public EyeseeLinux::EyeseeCamera::PictureCallback, 
								public EyeseeLinux::PictureRegionCallback
{
public:
    bool onInfo(int chnId, CameraMsgInfoType info, int extra, EyeseeLinux::EyeseeCamera *pCamera);
    void onPictureTaken(int chnId, const void *data, int size, EyeseeLinux::EyeseeCamera* pCamera);
    void addPictureRegion(std::list<PictureRegionType> &rPicRgnList);

    EyeseeCameraListener(SampleEyeseeEisContext *pContext);
    virtual ~EyeseeCameraListener(){}
private:
    SampleEyeseeEisContext *const mpContext;
};

//define listener for EyeseeISE
class EyeseeIseListener : public EyeseeLinux::EyeseeISE::ErrorCallback, 
						    public EyeseeLinux::EyeseeISE::InfoCallback, 
						    public EyeseeLinux::EyeseeISE::PictureCallback,
						    public EyeseeLinux::PictureRegionCallback
{
public:
    void onError(int chnId, int error, EyeseeLinux::EyeseeISE *pISE);
    bool onInfo(int chnId, CameraMsgInfoType info, int extra, EyeseeLinux::EyeseeISE *pISE);
    void onPictureTaken(int chnId, const void *data, int size, EyeseeLinux::EyeseeISE* pISE);
    void addPictureRegion(std::list<PictureRegionType> &rPicRgnList);

    EyeseeIseListener(SampleEyeseeEisContext *pContext);
    virtual ~EyeseeIseListener(){}
private:
    SampleEyeseeEisContext *const mpContext;
}; 

// define listener for EyeseeEIS
class EyeseeEisListener : public EyeseeLinux::EyeseeEIS::ErrorCallback, 
							public EyeseeLinux::EyeseeEIS::InfoCallback,
						    public EyeseeLinux::EyeseeEIS::PictureCallback, 
						    public EyeseeLinux::PictureRegionCallback
{
public:
    void onError(int chnId, int error, EyeseeLinux::EyeseeEIS *pEIS);
    bool onInfo(int chnId, CameraMsgInfoType info, int extra, EyeseeLinux::EyeseeEIS *pEIS);
    void onPictureTaken(int chnId, const void *data, int size, EyeseeLinux::EyeseeEIS* pEIS);
    void addPictureRegion(std::list<PictureRegionType> &rPicRgnList);

    EyeseeEisListener(SampleEyeseeEisContext *pContext);
    virtual ~EyeseeEisListener(){}
private:
    SampleEyeseeEisContext *const mpContext;
};

// define listener for EyeseeRecorder
class EyeseeRecorderListener : public EyeseeLinux::EyeseeRecorder::OnErrorListener,
								    public EyeseeLinux::EyeseeRecorder::OnInfoListener,
								    public EyeseeLinux::EyeseeRecorder::OnDataListener
{
public:
    void onError(EyeseeLinux::EyeseeRecorder *pMr, int what, int extra);
    void onInfo(EyeseeLinux::EyeseeRecorder *pMr, int what, int extra);
    void onData(EyeseeLinux::EyeseeRecorder *pMr, int what, int extra);

    EyeseeRecorderListener(SampleEyeseeEisContext *pOwner);
    virtual ~EyeseeRecorderListener(){}
private:
    SampleEyeseeEisContext *const mpContext;
};

// define params for camara
typedef struct CamDeviceParam_s {
    int mCamCapWidth;
    int mCamCapHeight;
	
    int mCamPrevWidth;
    int mCamPrevHeight;

	int mCamFrameRate;
    int mCamBufNum;
    int mCamRotation;  // 0,90,180,270

	int mCamId;         // guarantee mCamId == mCamCsiChn
    int mCamCsiChn;
    int mCamIspDev;
    int mCamIspScale0;
    int mCamIspScale1;
} CamDeviceParam;

// define param for display
typedef struct DispParam_s {
    int mDispRectX;
    int mDispRectY;
    int mDispRectWidth;
    int mDispRectHeight;
} DispParam;

// define param for photo taking
typedef struct PhotoParam_s{
    int mPhotoMode;     // 0-continuous; 1-fast
    
    int mCamChnId;
	
    int mPicNumId;

	int mPicWidth;
    int mPicHeight;
    char mPicPath[64];
//    char mThumbPath[64];
//    int mThumbWidth;
//    int mThumbHeight;
    // only for continuous
    int mContPicCnt;
    int mContPicItlMs;
} PhotoParam;

// define params for region
typedef struct RegionParam_s{
    // for overlay
    int mRgnOvlyEnable;

	int mRgnOvlyX;
    int mRgnOvlyY;
    int mRgnOvlyW;
    int mRgnOvlyH;
	
    int mRgnOvlyPixFmt;             // 0-ARGB8888; 1-ARGB1555
    int mRgnOvlyCorVal;             // 0xAARRGGBB
    int mRgnOvlyInvCorEn;           // invert color enable
    int mRgnOvlyPri;                // [0,VI_MAX_OVERLAY_NUM-1]

	// for cover
    int mRgnCovEnable;

	int mRgnCovX;
    int mRgnCovY;
    int mRgnCovW;
    int mRgnCovH;

	int mRgnCovCor;
    int mRgnCovPri;                 // [0,VI_MAX_COVER_NUM-1]
} RegionParam;

// define params for recorder
typedef struct RecParam_s {
    char mRecParamFileName[64];          // "/mnt/extsd/xyz.mp4"
    
    char mRecParamVideoEncoder[64];      // "H.264", "H.265"

	int mRecParamVideoSizeWidth;
    int mRecParamVideoSizeHeight;
	int mRecParamVideoBitRate;          // mbps
	int mRecParamVideoFrameRate;

	char mRecParamAudioEncoder[64];  // "AAC", "MP3"

	int mRecParamAudioSamplingRate;
    int mRecParamAudioChannels;
    int mRecParamAudioBitRate;
} RecParam;

// define params for using in one fish mode of ise
typedef struct IseMoParam_s {
    //ref frome ISE_lib_mo.h
    int mIseMoType; //WARP_Type_MO: 1-WARP_PANO180; 2-WARP_PANO360; 3-WARP_NORMAL; 4-WARP_UNDISTORT; 5-WARP_180WITH2; 6-WARP_PTZ4IN1
    
    int mIseMoMountTpye;    //MOUNT_Type_MO: 1-MOUNT_TOP; 2-MOUNT_WALL; 3-MOUNT_BOTTOM

	int mIseMoInHeight;     //get from CamDeviceParam.mCamCapHeight
    int mIseMoInWidth;      //get from CamDeviceParam.mCamCapWidth

	int mIseMoInLumaPitch;
    int mIseMoInChromaPitch;

	int mIseMoInYuvType;    //0-NV21
	
    float mIseMoPan;
    float mIseMoTilt;
    float mIseMoZoom;

    //only for PTZ4IN1
    float mIseMoPanSub[4];
    float mIseMoTiltSub[4];
    float mIseMoZoomSub[4];

    //only for dynamic ptz
    float mIseMoPtzSubChg[4];

	

    int mIseMoOutEn[4];
	
    int mIseMoOutHeight[4];
    int mIseMoOutWidth[4];
    int mIseMoOutLumaPitch[4];
    int mIseMoOutChromaPitch[4];
    int mIseMoOutFlip[4];
    int mIseMoOutMirror[4];
    int mIseMoOutYuvType;    //0-NV21

    //for len param
    float mIseMoP;
    float mIseMoCx;
    float mIseMoCy;

    //only for undistort
    float mIseMoFx;
    float mIseMoFy;
    float mIseMoCxd;
    float mIseMoCyd;
    float mIseMoFxd;
    float mIseMoFyd;
    float mIseMoK[8];

    float mIseMoPUndis[2];
    double mIseMoCalibMatr[3][3];
    double mIseMoCalibMatrCv[3][3];
    double mIseMoDistort[8];
    char mIseMoReserved[32];
} IseMoParam;

typedef struct EisParam_s{
	int mInputWidth;
	int mInputHeight;
	int mInputLumaPitch;
	int mInputChormaPitch;
	int mInputYuvType;

	int mOutputWidth;
	int mOutputHeight;
	int mOutputLumaPitch;
	int mOutputChormaPitch;
	int mOutputYuvType; 
}EisParam;

// all configured params set in configuration file
typedef struct SampleEyeseeEisConfig_s {
    //camera
    BOOL mCamEnable[2];
    CamDeviceParam mCamDevParam[2];
	
    BOOL mCamDispEnable[2];
    BOOL mCamDispEnable_fake[2];
    DispParam mDispParam[2];
	
    BOOL mCamPhotoEnable[2];
    PhotoParam mCamPhotoParam[2];

	BOOL mCamRgnEnable[2];          // only for jpeg
    RegionParam mCamRgnParam[2];

	BOOL mCamRecEnable[2];
    RecParam mCamRecParam[2];

    //ise
    int mIseMode;   //0-none; 1-mo; 2-bi; 3-sti
    //ise-mo
    BOOL mIseMoEnable;
    IseMoParam mIseMoParam;				// xxx

	BOOL mIseMoDispEnable[4];
    DispParam mIseMoDispParam[4];		// disp related

	BOOL mIseMoPhotoEnable[4];
    PhotoParam mIseMoPhotoParam[4];		// photo related

	BOOL mIseMoRgnEnable[4];          	// only for jpeg
    RegionParam mIseMoRgnParam[4];		// region related

	BOOL mIseMoRecEnable[4];			// recoder related
    RecParam mIseMoRecParam[4]; 

	// eis
	BOOL mEisEnable;
	EisParam mEisParam;

	int mEisMulThrd;

	BOOL mEisDispEnable;
	DispParam mEisDispParam;

	BOOL mEis1DispEnable;
	DispParam mEis1DispParam;

	BOOL mEisPhotoEnable;
	PhotoParam mEisPhotoParam;

	BOOL mEisRgnEnable;
	RegionParam mEisRgnParam;

	BOOL mEisRecordEnable;
	RecParam mEisRecParam;
} SampleEyeseeEisConfig; 

class SampleEyeseeEisContext
{ 
public:
	SampleEyeseeEisContext();
	~SampleEyeseeEisContext();

////////
    EyeseeLinux::status_t ParseCmdLine(int argc, char *argv[]);
    EyeseeLinux::status_t loadConfig();
    EyeseeLinux::status_t checkConfig();

    SampleEyeseeEisCmdLineParam mCmdLineParam;
    SampleEyeseeEisConfig       mConfigParam;	// store parsed configuration params	samuel.zhou
//////////
	
    cdx_sem_t mSemExit;

    MPP_SYS_CONF_S mSysConf;

	// VO
    VO_DEV mVoDev;
    int mUILayer; 

	// Camara
    VO_LAYER mCamVoLayer[2];		// preview layer for camara0,and preview layer for camara 1
    int mCamDispChnId[2];			// vipp chl for camara0,and vipp chl for camara1 (preview)  samuel.zhou
    
    CameraInfo mCameraInfo[2];	// store camara info got from camara	samuel.zhou

	// ISE
    BOOL mIseChnEnable[4][4];	// [ise_dev][p_idx]		flag indicating channel enabled or not	samuel.zhou
    ISE_CHN mIseChnId[4][4];	// [ise_dev][p_idx]		
    ISE_CHN_ATTR_S mIseChnAttr[4];	// <--- ise configuration param 
    
    BOOL mIseVoLayerEnable[4];	// p0 ~p3    dispaly or not			samuel.zhou
    VO_LAYER mIseVoLayer[4]; 	// p0~p3	video laye index		samuel.zhou 

	// EIS
	BOOL  mEisChnEnable;
	EIS_CHN mEisChnId;
	EIS_ATTR_S mEisChnAttr;
	BOOL mEisVoLayerEnable;
	BOOL mEisVoLayerEnable2;
	VO_LAYER mEisVoLayer;
	VO_LAYER mEisVoLayer2;
	
    EyeseeLinux::EyeseeCamera *mpCamera[2];
    EyeseeLinux::EyeseeISE *mpIse;
    EyeseeLinux::EyeseeEIS *mpEis[2];
    EyeseeLinux::EyeseeRecorder *mpRecorder[5];
	
    EyeseeCameraListener mCameraListener;
    EyeseeIseListener mIseListener;
    EyeseeEisListener mEisListener;
    EyeseeRecorderListener mRecorderListener;   // not use

};



								


#endif
