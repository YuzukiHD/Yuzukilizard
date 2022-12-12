
#ifndef _SAMPLE_FISHEYE_H_
#define _SAMPLE_FISHEYE_H_

#include <string>
#include <deque>

#include <plat_type.h>
#include <tsemaphore.h>
#include <mpi_sys.h>
#include <mpi_clock.h>
#include <mpi_vo.h>
#include <PictureRegionCallback.h>

#include <Errors.h>

#define DEFAULT_SAMPLE_EYESEEISE_CONF_PATH  "/mnt/extsd/sample_EyeseeIse/sample_fisheye.conf"

/******************************************************************************/
// cam device param
#define SAMPLE_EYESEEISE_CAM_CSI_CHN               "cam_csi_chn"
#define SAMPLE_EYESEEISE_CAM_ISP_DEV               "cam_isp_dev"
#define SAMPLE_EYESEEISE_CAM_VIPP_NUM              "cam_vipp_num"
#define SAMPLE_EYESEEISE_CAM_CAP_WIDTH             "cam_cap_width"
#define SAMPLE_EYESEEISE_CAM_CAP_HEIGHT            "cam_cap_height"
#define SAMPLE_EYESEEISE_CAM_FRAME_RATE            "cam_frame_rate"
#define SAMPLE_EYESEEISE_CAM_BUF_NUM               "cam_buf_num"
// cam preview param
#define SAMPLE_EYESEEISE_CAM_DISP_ENABLE           "cam_disp_enable"
#define SAMPLE_EYESEEISE_CAM_DISP_RECT_X           "cam_disp_rect_x"
#define SAMPLE_EYESEEISE_CAM_DISP_RECT_Y           "cam_disp_rect_y"
#define SAMPLE_EYESEEISE_CAM_DISP_RECT_WIDTH       "cam_disp_rect_width"
#define SAMPLE_EYESEEISE_CAM_DISP_RECT_HEIGHT      "cam_disp_rect_height"
// cam record param
#define SAMPLE_EYESEEISE_CAM_REC_ENABLE            "cam_rec_enable"
#define SAMPLE_EYESEEISE_CAM_REC_V_TYPE            "cam_rec_v_type"
#define SAMPLE_EYESEEISE_CAM_REC_V_WIDTH           "cam_rec_v_width"
#define SAMPLE_EYESEEISE_CAM_REC_V_HEIGHT          "cam_rec_v_height"
#define SAMPLE_EYESEEISE_CAM_REC_V_BITRATE         "cam_rec_v_bitrate"
#define SAMPLE_EYESEEISE_CAM_REC_V_FRAMERATE       "cam_rec_v_framerate"
#define SAMPLE_EYESEEISE_CAM_REC_A_TYPE            "cam_rec_a_type"
#define SAMPLE_EYESEEISE_CAM_REC_A_SR              "cam_rec_a_sr"
#define SAMPLE_EYESEEISE_CAM_REC_A_CHN             "cam_rec_a_chn"
#define SAMPLE_EYESEEISE_CAM_REC_A_BR              "cam_rec_a_br"

/******************************************************************************/
// ise-mo
#define SAMPLE_EYESEEISE_ISE_MO_ENABLE              "ise_mo_enable"
#define SAMPLE_EYESEEISE_ISE_GROUP_NUM              "ise_group_num"
#define SAMPLE_EYESEEISE_ISE_MO_P                   "ise_mo_p"
#define SAMPLE_EYESEEISE_ISE_MO_CX                  "ise_mo_cx"
#define SAMPLE_EYESEEISE_ISE_MO_CY                  "ise_mo_cy"

#define SAMPLE_EYESEEISE_ISE_GROUP0_DEWARP_MODE     "ise_mo_group0_dewarp_mode"
#define SAMPLE_EYESEEISE_ISE_GROUP0_MO_MOUNT_TYPE   "ise_mo_group0_mount_type"
#define SAMPLE_EYESEEISE_ISE_GROUP0_MO_PAN          "ise_mo_group0_pan"
#define SAMPLE_EYESEEISE_ISE_GROUP0_MO_TILT         "ise_mo_group0_tilt"
#define SAMPLE_EYESEEISE_ISE_GROUP0_MO_ZOOM         "ise_mo_group0_zoom"
#define SAMPLE_EYESEEISE_ISE_GROUP0_MO_OUT_W        "ise_mo_group0_w"
#define SAMPLE_EYESEEISE_ISE_GROUP0_MO_OUT_H        "ise_mo_group0_h"
#define SAMPLE_EYESEEISE_ISE_GROUP0_MO_OUT_FLIP     "ise_mo_group0_flip"
#define SAMPLE_EYESEEISE_ISE_GROUP0_MO_OUT_MIR      "ise_mo_group0_mir"

#define SAMPLE_EYESEEISE_ISE_GROUP1_DEWARP_MODE     "ise_mo_group1_dewarp_mode"
#define SAMPLE_EYESEEISE_ISE_GROUP1_MO_MOUNT_TYPE   "ise_mo_group1_mount_type"
#define SAMPLE_EYESEEISE_ISE_GROUP1_MO_PAN          "ise_mo_group1_pan"
#define SAMPLE_EYESEEISE_ISE_GROUP1_MO_TILT         "ise_mo_group1_tilt"
#define SAMPLE_EYESEEISE_ISE_GROUP1_MO_ZOOM         "ise_mo_group1_zoom"
#define SAMPLE_EYESEEISE_ISE_GROUP1_MO_OUT_W        "ise_mo_group1_w"
#define SAMPLE_EYESEEISE_ISE_GROUP1_MO_OUT_H        "ise_mo_group1_h"
#define SAMPLE_EYESEEISE_ISE_GROUP1_MO_OUT_FLIP     "ise_mo_group1_flip"
#define SAMPLE_EYESEEISE_ISE_GROUP1_MO_OUT_MIR      "ise_mo_group1_mir"

#define SAMPLE_EYESEEISE_ISE_GROUP2_DEWARP_MODE     "ise_mo_group2_dewarp_mode"
#define SAMPLE_EYESEEISE_ISE_GROUP2_MO_MOUNT_TYPE   "ise_mo_group2_mount_type"
#define SAMPLE_EYESEEISE_ISE_GROUP2_MO_PAN          "ise_mo_group2_pan"
#define SAMPLE_EYESEEISE_ISE_GROUP2_MO_TILT         "ise_mo_group2_tilt"
#define SAMPLE_EYESEEISE_ISE_GROUP2_MO_ZOOM         "ise_mo_group2_zoom"
#define SAMPLE_EYESEEISE_ISE_GROUP2_MO_OUT_W        "ise_mo_group2_w"
#define SAMPLE_EYESEEISE_ISE_GROUP2_MO_OUT_H        "ise_mo_group2_h"
#define SAMPLE_EYESEEISE_ISE_GROUP2_MO_OUT_FLIP     "ise_mo_group2_flip"
#define SAMPLE_EYESEEISE_ISE_GROUP2_MO_OUT_MIR      "ise_mo_group2_mir"

#define SAMPLE_EYESEEISE_ISE_GROUP3_DEWARP_MODE     "ise_mo_group3_dewarp_mode"
#define SAMPLE_EYESEEISE_ISE_GROUP3_MO_MOUNT_TYPE   "ise_mo_group3_mount_type"
#define SAMPLE_EYESEEISE_ISE_GROUP3_MO_PAN          "ise_mo_group3_pan"
#define SAMPLE_EYESEEISE_ISE_GROUP3_MO_TILT         "ise_mo_group3_tilt"
#define SAMPLE_EYESEEISE_ISE_GROUP3_MO_ZOOM         "ise_mo_group3_zoom"
#define SAMPLE_EYESEEISE_ISE_GROUP3_MO_OUT_W        "ise_mo_group3_w"
#define SAMPLE_EYESEEISE_ISE_GROUP3_MO_OUT_H        "ise_mo_group3_h"
#define SAMPLE_EYESEEISE_ISE_GROUP3_MO_OUT_FLIP     "ise_mo_group3_flip"
#define SAMPLE_EYESEEISE_ISE_GROUP3_MO_OUT_MIR      "ise_mo_group3_mir"

// ise-mo-preview param
#define SAMPLE_EYESEEISE_ISE_MO_DISP_ENABLE               "ise_mo_disp_enable"
#define SAMPLE_EYESEEISE_ISE_MO_GROUP0_DISP_RECT_X        "ise_mo_disp_group0_rect_x"
#define SAMPLE_EYESEEISE_ISE_MO_GROUP0_DISP_RECT_Y        "ise_mo_disp_group0_rect_y"
#define SAMPLE_EYESEEISE_ISE_MO_GROUP0_DISP_RECT_WIDTH    "ise_mo_disp_group0_rect_w"
#define SAMPLE_EYESEEISE_ISE_MO_GROUP0_DISP_RECT_HEIGHT   "ise_mo_disp_group0_rect_h"
#define SAMPLE_EYESEEISE_ISE_MO_GROUP1_DISP_RECT_X        "ise_mo_disp_group1_rect_x"
#define SAMPLE_EYESEEISE_ISE_MO_GROUP1_DISP_RECT_Y        "ise_mo_disp_group1_rect_y"
#define SAMPLE_EYESEEISE_ISE_MO_GROUP1_DISP_RECT_WIDTH    "ise_mo_disp_group1_rect_w"
#define SAMPLE_EYESEEISE_ISE_MO_GROUP1_DISP_RECT_HEIGHT   "ise_mo_disp_group1_rect_h"
#define SAMPLE_EYESEEISE_ISE_MO_GROUP2_DISP_RECT_X        "ise_mo_disp_group2_rect_x"
#define SAMPLE_EYESEEISE_ISE_MO_GROUP2_DISP_RECT_Y        "ise_mo_disp_group2_rect_y"
#define SAMPLE_EYESEEISE_ISE_MO_GROUP2_DISP_RECT_WIDTH    "ise_mo_disp_group2_rect_w"
#define SAMPLE_EYESEEISE_ISE_MO_GROUP2_DISP_RECT_HEIGHT   "ise_mo_disp_group2_rect_h"
#define SAMPLE_EYESEEISE_ISE_MO_GROUP3_DISP_RECT_X        "ise_mo_disp_group3_rect_x"
#define SAMPLE_EYESEEISE_ISE_MO_GROUP3_DISP_RECT_Y        "ise_mo_disp_group3_rect_y"
#define SAMPLE_EYESEEISE_ISE_MO_GROUP3_DISP_RECT_WIDTH    "ise_mo_disp_group3_rect_w"
#define SAMPLE_EYESEEISE_ISE_MO_GROUP3_DISP_RECT_HEIGHT   "ise_mo_disp_group3_rect_h"

// ise-mo-record0 param
#define SAMPLE_EYESEEISE_ISE_MO_REC_ENABLE            "ise_mo_rec_enable"
#define SAMPLE_EYESEEISE_ISE_MO_REC_PATH              "ise_mo_rec_path"
#define SAMPLE_EYESEEISE_ISE_MO_REC_V_TYPE            "ise_mo_rec_v_type"
#define SAMPLE_EYESEEISE_ISE_MO_REC_V_WIDTH           "ise_mo_rec_v_width"
#define SAMPLE_EYESEEISE_ISE_MO_REC_V_HEIGHT          "ise_mo_rec_v_height"
#define SAMPLE_EYESEEISE_ISE_MO_REC_V_BITRATE         "ise_mo_rec_v_bitrate"
#define SAMPLE_EYESEEISE_ISE_MO_REC_V_FRAMERATE       "ise_mo_rec_v_framerate"
#define SAMPLE_EYESEEISE_ISE_MO_REC_A_TYPE            "ise_mo_rec_a_type"
#define SAMPLE_EYESEEISE_ISE_MO_REC_A_SR              "ise_mo_rec_a_sr"
#define SAMPLE_EYESEEISE_ISE_MO_REC_A_CHN             "ise_mo_rec_a_chn"
#define SAMPLE_EYESEEISE_ISE_MO_REC_A_BR              "ise_mo_rec_a_br"

/******************************************************************************/
/******************************************************************************/
/******************************************************************************/

typedef struct SampleEyeseeIseCmdLineParam
{
    std::string mConfigFilePath;
}SampleEyeseeIseCmdLineParam;

class SampleEyeseeIseContext;
class EyeseeCameraListener
    : public EyeseeLinux::EyeseeCamera::PictureCallback
    , public EyeseeLinux::EyeseeCamera::InfoCallback
    , public EyeseeLinux::PictureRegionCallback
{
public:
    bool onInfo(int chnId, CameraMsgInfoType info, int extra, EyeseeLinux::EyeseeCamera *pCamera);
    void onPictureTaken(int chnId, const void *data, int size, EyeseeLinux::EyeseeCamera* pCamera);
    void addPictureRegion(std::list<PictureRegionType> &rPicRgnList);

    EyeseeCameraListener(SampleEyeseeIseContext *pContext);
    virtual ~EyeseeCameraListener(){}
private:
    SampleEyeseeIseContext *const mpContext;
};

class EyeseeIseListener
    : public EyeseeLinux::EyeseeISE::PictureCallback
    , public EyeseeLinux::EyeseeISE::ErrorCallback
    , public EyeseeLinux::EyeseeISE::InfoCallback
    , public EyeseeLinux::PictureRegionCallback
{
public:
    void onError(int chnId, int error, EyeseeLinux::EyeseeISE *pISE);
    bool onInfo(int chnId, CameraMsgInfoType info, int extra, EyeseeLinux::EyeseeISE *pISE);
    void onPictureTaken(int chnId, const void *data, int size, EyeseeLinux::EyeseeISE* pISE);
    void addPictureRegion(std::list<PictureRegionType> &rPicRgnList);

    EyeseeIseListener(SampleEyeseeIseContext *pContext);
    virtual ~EyeseeIseListener(){}
private:
    SampleEyeseeIseContext *const mpContext;
};

class EyeseeRecorderListener 
    : public EyeseeLinux::EyeseeRecorder::OnErrorListener
    , public EyeseeLinux::EyeseeRecorder::OnInfoListener
    , public EyeseeLinux::EyeseeRecorder::OnDataListener
{
public:
    void onError(EyeseeLinux::EyeseeRecorder *pMr, int what, int extra);
    void onInfo(EyeseeLinux::EyeseeRecorder *pMr, int what, int extra);
    void onData(EyeseeLinux::EyeseeRecorder *pMr, int what, int extra);

    EyeseeRecorderListener(SampleEyeseeIseContext *pOwner);
    virtual ~EyeseeRecorderListener(){}
private:
    SampleEyeseeIseContext *const mpContext;
};


typedef struct CamDeviceParam {
    int mCamVippNum;
    int mCamCapWidth;
    int mCamCapHeight;
    int mCamPrevWidth;
    int mCamPrevHeight;
    int mVIChn_Preview;
    int mCamFrameRate;
    int mCamBufNum;
    int mCamRotation;  // 0,90,180,270
    int mCamId;         // guarantee mCamId == mCamCsiChn
    int mCamCsiChn;
    int mCamIspDev;
    int mCamIspScale[4];
} CamDeviceParam;

typedef struct DispParam {
    int mDispRectX;
    int mDispRectY;
    int mDispRectWidth;
    int mDispRectHeight;
} DispParam;

typedef struct PhotoParam{
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

typedef struct RegionParam{
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

typedef struct RecParam {
    char mRecParamFileName[256];          // "/mnt/extsd/xyz.mp4"
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

typedef struct IseMoGroupOutputParam {
    //ref frome ISE_lib_mo.h
    int mIseMoDewarpMode; //WARP_Type_MO: 1-WARP_PANO180; 2-WARP_PANO360; 3-WARP_NORMAL; 4-WARP_UNDISTORT; 5-WARP_180WITH2; 6-WARP_PTZ4IN1
    int mIseMoMountTpye;    //MOUNT_Type_MO: 1-MOUNT_TOP; 2-MOUNT_WALL; 3-MOUNT_BOTTOM
    float mIseMoPan;
    float mIseMoTilt;
    float mIseMoZoom;

    //only for PTZ4IN1
    float mIseMoPanSub[4];
    float mIseMoTiltSub[4];
    float mIseMoZoomSub[4];

    //only for dynamic ptz
    float mIseMoPtzSubChg[4];

    int mIseMoOutEn;
    int mIseMoOutHeight;
    int mIseMoOutWidth;
    int mIseMoOutLumaPitch;
    int mIseMoOutChromaPitch;
    int mIseMoOutFlip;
    int mIseMoOutMirror;
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
    float mIseMoK[6];

    float mIseMoPUndis[2];
    double mIseMoCalibMatr[3][3];
    double mIseMoCalibMatrCv[3][3];
    double mIseMoDistort[8];
    char mIseMoReserved[32];
} IseMoGroupOutputParam;

typedef struct IseMoGroupIutputParam {
    int mIseMoGroupNum;
    int mIseMoInHeight;     //get from CamDeviceParam.mCamCapHeight
    int mIseMoInWidth;      //get from CamDeviceParam.mCamCapWidth
    int mIseMoInLumaPitch;
    int mIseMoInChromaPitch;
    int mIseMoInYuvType;    //0-NV21
}IseMoGroupIutputParam;

typedef struct IseBiParam {


} IseBiParam;

typedef struct IseStiParam {


} IseStiParam;


typedef struct SampleEyeseeIseConfig {
    //camera
    BOOL mCamEnable[2];
    CamDeviceParam mCamDevParam[2];
    BOOL mCamDispEnable;
    DispParam mDispParam[2];
    BOOL mCamPhotoEnable[2];
    PhotoParam mCamPhotoParam[2];
    BOOL mCamRgnEnable[2];          // only for jpeg
    RegionParam mCamRgnParam[2];
    BOOL mCamRecEnable;
    RecParam mCamRecParam[2];

    //ise
    int mIseMode;   //0-none; 1-mo; 2-bi; 3-sti
    //ise-mo
    BOOL mIseMoEnable;
    IseMoGroupIutputParam mIseMoIutputParam;
    IseMoGroupOutputParam mIseMoOutputParam[4];
    BOOL mIseMoDispEnable;
    DispParam mIseMoDispParam[4];
    BOOL mIseMoPhotoEnable[4];
    PhotoParam mIseMoPhotoParam[4];
    BOOL mIseMoRgnEnable[4];          // only for jpeg
    RegionParam mIseMoRgnParam[4];
    BOOL mIseMoRecEnable;
    RecParam mIseMoRecParam[4];
    //ise-bi
    IseBiParam mIseBiParam;
    DispParam mIseBiDispParam;
    RecParam mIseBiRecParam;
    //ise-sti
    IseStiParam mIseStiParam;
    DispParam mIseStiDispParam;
    RecParam mIseStiRecParam;
} SampleEyeseeIseConfig;

class SampleEyeseeIseContext
{
public:
    SampleEyeseeIseContext();
    ~SampleEyeseeIseContext();

    EyeseeLinux::status_t ParseCmdLine(int argc, char *argv[]);
    EyeseeLinux::status_t loadConfig();
    EyeseeLinux::status_t checkConfig();

    SampleEyeseeIseCmdLineParam mCmdLineParam;
    SampleEyeseeIseConfig       mConfigParam;

    cdx_sem_t mSemExit;

    VO_DEV mVoDev;
    int mUILayer;
    VO_INTF_TYPE_E mDispType;
    VO_INTF_SYNC_E mDispSync;

    MPP_SYS_CONF_S mSysConf;
    //VO_VIDEO_LAYER_ATTR_S mLayerAttr;
    VO_LAYER mCamVoLayer[2];
    int mCamDispChnId[2];

    ISE_CHN_ATTR_S mIseChnAttr[4];
    VO_LAYER mIseVoLayer[4];
    BOOL mIseVoLayerEnable[4];
    // for notify app to preview and check chn id.
    // fish:    ptz mode, [x][y] for preview; other mode, only use [0][x] for preview.
    // dfish:   only use [0]
    // sti:     only use [0]
    ISE_CHN mIseChnId[4][4];
    BOOL mIseChnEnable[4][4];

    CameraInfo mCameraInfo[2];
    EyeseeLinux::EyeseeCamera *mpCamera[4];
    EyeseeLinux::EyeseeISE *mpIse[4];
    EyeseeLinux::EyeseeRecorder *mpRecorder[4];
    EyeseeCameraListener mCameraListener;
    EyeseeIseListener mIseListener;
    EyeseeRecorderListener mRecorderListener;   // not use

    int mMuxerId[4];
    //int mFileNum;

    int mCamPicNum;
    int mIsePicNum;
    bool stop_cam_recorder_flag;
    bool stop_ise_recorder_flag;
    int set_ise_recorder_fd_status;
    int set_cam_recorder_fd_status;
};

#endif  /* _SAMPLE_FISHEYE_H_ */

