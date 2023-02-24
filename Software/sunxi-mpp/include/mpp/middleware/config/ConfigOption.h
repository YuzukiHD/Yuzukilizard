#ifndef _CONFIGOPTION_H_
#define _CONFIGOPTION_H_

// option for CDXCFG_FILE_SYSTEM.
#define OPTION_FILE_SYSTEM_LINUX_VFS    (0)
#define OPTION_FILE_SYSTEM_DIRECT_FATFS (1)
#define OPTION_FILE_SYSTEM_DIRECT_IO    (2)

//option for CDXCFG_HW_DISPLAY
#define OPTION_HW_DISPLAY_ENABLE    (1)
#define OPTION_HW_DISPLAY_DISABLE   (0)

//option for MPPCFG_VI
#define OPTION_VI_ENABLE    (1)
#define OPTION_VI_DISABLE   (0)

//option for MPPCFG_VO
#define OPTION_VO_ENABLE    (1)
#define OPTION_VO_DISABLE   (0)

//option for MPPCFG_TEXTENC
#define OPTION_TEXTENC_ENABLE (1)
#define OPTION_TEXTENC_DISABLE (0)

//option for MPPCFG_VENC
#define OPTION_VENC_ENABLE (1)
#define OPTION_VENC_DISABLE (0)

//option for MPPCFG_VDEC
#define OPTION_VDEC_ENABLE (1)
#define OPTION_VDEC_DISABLE (0)

//option for MPPCFG_AIO
#define OPTION_AIO_ENABLE    (1)
#define OPTION_AIO_DISABLE   (0)

//option for MPPCFG_AENC
#define OPTION_AENC_ENABLE (1)
#define OPTION_AENC_DISABLE (0)

//option for MPPCFG_ADEC
#define OPTION_ADEC_ENABLE (1)
#define OPTION_ADEC_DISABLE (0)

//option for MPPCFG_MUXER
#define OPTION_MUXER_ENABLE    (1)
#define OPTION_MUXER_DISABLE   (0)

//option for MPPCFG_DEMUXER
#define OPTION_DEMUXER_ENABLE    (1)
#define OPTION_DEMUXER_DISABLE   (0)

//option for MPPCFG_ISE
#define OPTION_ISE_ENABLE    (1)
#define OPTION_ISE_DISABLE   (0)

//option for MPPCFG_ISE_MO
#define OPTION_ISE_MO_ENABLE     (1)
#define OPTION_ISE_MO_DISABLE    (0)

//option for MPPCFG_ISE_GDC
#define OPTION_ISE_GDC_ENABLE    (1)
#define OPTION_ISE_GDC_DISABLE   (0)

//option for MPPCFG_EIS
#define OPTION_EIS_ENABLE    (1)
#define OPTION_EIS_DISABLE   (0)

//option for MPPCFG_MOD
#define OPTION_MOD_ENABLE    (1)
#define OPTION_MOD_DISABLE   (0)

//option for MPPCFG_ADAS
#define OPTION_ADAS_DETECT_ENABLE    (1)
#define OPTION_ADAS_DETECT_DISABLE   (0)

//option for MPPCFG_EVEFACE
#define OPTION_EVEFACE_ENABLE    (1)
#define OPTION_EVEFACE_DISABLE   (0)

//option for MPPCFG_VLPR
#define OPTION_VLPR_ENABLE    (1)
#define OPTION_VLPR_DISABLE   (0)

//option for MPPCFG_BDII
#define OPTION_BDII_ENABLE    (1)
#define OPTION_BDII_DISABLE   (0)

//option for MPPCFG_BDII
#define OPTION_AEC_ENABLE    (1)
#define OPTION_AEC_DISABLE   (0)

#define OPTION_ANS_ENABLE    (1)
#define OPTION_ANS_DISABLE   (0)

#define OPTION_SOFTDRC_ENABLE    (1)
#define OPTION_SOFTDRC_DISABLE   (0)

#define OPTION_AGC_ENABLE    (1)
#define OPTION_AGC_DISABLE   (0)



//option for MPPCFG_MOD
#define OPTION_MOTION_DETECT_SOFT_ENABLE    (1)
#define OPTION_MOTION_DETECT_SOFT_DISABLE   (0)

//option for MPPCFG_AND_LIB
#define OPTION_ANS_LIBRARY_WEBRTC (0)
#define OPTION_ANS_LIBRARY_LSTM (1)

//option for MPPCFG_GPS_PACK_METHOD
#define OPTION_GPS_PACK_IN_TRACK (0)
#define OPTION_GPS_PACK_IN_MDAT (1)

/*
// option for CDXCFG_GPU_TYPE
#define OPTION_GPU_TYPE_MALI    (0)
#define OPTION_GPU_TYPE_IMG     (1)
#define OPTION_GPU_TYPE_NONE    (255)

// option for CDXCFG_VE_NATIVE_OUTPUT_FORMAT
#define OPTION_VE_NATIVE_OUTPUT_FORMAT_YV12    (0)
#define OPTION_VE_NATIVE_OUTPUT_FORMAT_MB32    (1)

//property key/value for WRITE_FILE_METHOD
#define PROP_KEY_WRITE_FILE_METHOD   "media.cedarx.WRITE_FILE_METHOD"
#define PROP_VALUE_FS_THREAD_CACHE  "FS_THREAD_CACHE"
#define PROP_VALUE_FS_SIMPLE_CACHE  "FS_SIMPLE_CACHE"
#define PROP_VALUE_FS_NO_CACHE      "FS_NO_CACHE"

//property key/value for simple cache size
#define PROP_KEY_FS_SIMPLE_CACHE_SIZE   "media.cedarx.cache_size"

//property key/value for REC_VBV_TIME
#define PROP_KEY_REC_VBV_TIME   "media.cedarx.REC_VBV_TIME"

//property key/value for FrameInterval, unit:ms
#define PROP_KEY_PLAY_FRAME_INTERVAL   "media.cedarx.PlayFrameInterval"

//property key/value for output resolution constraint
#define PROP_KEY_CONSTRAINT_RES  "media.cedarx.cons_res"

//property key/value for video encode color space
#define PROP_KEY_ENCODE_COLOR_SPACE   "media.cedarx.EncodeColorSpace"
#define PROP_VALUE_BT601 "BT601"
#define PROP_VALUE_FOLLOW_SOURCE "FollowSource"
*/

#endif  /* _CONFIGOPTION_H_ */

