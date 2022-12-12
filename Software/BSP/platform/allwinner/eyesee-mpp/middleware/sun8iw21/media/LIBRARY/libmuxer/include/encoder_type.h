

#ifndef __H264__ENCODE__LIB__TYPE__H__
#define __H264__ENCODE__LIB__TYPE__H__

#include <linux/types.h>

typedef void *				__hdle;
//#define ByteIOContext FILE
#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

//#define esFSYS_fopen 	fopen
//#define esFSYS_fclose 	fclose
//#define esFSYS_fseek 	fseek
//#define esFSYS_fread 	fread
//#define esFSYS_fwrite	fwrite
//#define esFSYS_ftell	ftell
//
//#define esMEMS_Pfree(a, b)	free(a)
//#define esMEMS_Palloc(a, b)	malloc(a*1024)
//
//#define ctm_fwrite		fwrite
//
//#define MALLOC			malloc
//#define FREE			free
//#define MEMCPY			memcpy
//#define MEMSET			memset
#define MAX(a,b) (((a) > (b)) ? (a) : (b))
#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#define CLIP3(low,high,x) MIN( MAX(low,x), high )
#define FLOOR(x) ((x) >= 0 ? (int)(x) : (int)((x)-1))
#define ABS(x) ((x) >= 0 ? (x) : -(x))

#if 0
typedef struct SIZE_t
{
	int src_width;
	int src_height;
	int width;
	int height;
}SIZE;


typedef struct VIDEO_ENCODE_FORMAT
{
    unsigned int     codec_type;
    int		width;
	int		height;
	
    unsigned int     frame_rate;
    int		color_format;
	int		color_space;
    int		qp_max;					// 40
	int		qp_min;					// 20
    int       avg_bit_rate;       	// average bit rate
//    int       max_bit_rate;       	// maximum bit rate
    
	int		maxKeyInterval;
    //define private information for video decode
//    unsigned int       video_bs_src;       // video bitstream source
//    void        *private_inf;       // video bitstream private information pointer
//    int       priv_inf_len;       // video bitstream private information length

	// star add
	unsigned char		profileIdc;
	unsigned char		levelIdc;

	unsigned int		src_width;
	unsigned int		src_height;

	int			scale_factor;
	double		focal_length;

	unsigned int		quality;			// for jpeg encoder
	unsigned int		orientation;		// for jpeg encoder

	// gps exif
	unsigned char		enable_gps;
	double      gps_latitude;  //input
	double		gps_longitude;  //input
	long        gps_altitude;  
	long        gps_timestamp;
	char		gps_processing_method[100];
	int         whitebalance;
	unsigned int       thumb_width;
	unsigned int       thumb_height;
	unsigned int       ratioThreahold;

	unsigned char  		CameraMake[64];//for the cameraMake name
	unsigned char  		CameraModel[64];//for the cameraMode
	unsigned char  		DateTime[21];//for the data and time
	unsigned char        picEncmode;	 //0 for frame encoding 1: for field encoding 2:field used for frame encoding
	int                  rot_angle; // 0,1,2,3
	//divide slices
	unsigned int        h264SliceEn;	//0 for one slice;1 for several slice

	//for I frame temporal_filter
	unsigned int		I_filter_enable;
} __video_encode_format_t;

typedef struct crop_para
{
	unsigned char  crop_img_enable;   //for enable the crop image
	unsigned int start_x;           //for crop image start position 
 	unsigned int start_y;           //for crop image start position 	        		
	unsigned int crop_pic_width;    //for crop image size of width 
	unsigned int crop_pic_height;   //for crop image size of height 
	
}__video_crop_crop_para_t;

typedef struct crop_enc_para
{
	unsigned char  crop_img_enable;                                  
	unsigned int start_crop_mb_x;      
 	unsigned int start_crop_mb_y;                		
	unsigned int crop_pic_width_mb;    
	unsigned int crop_pic_height_mb;  
}crop_enc_para_t;

typedef struct enc_extradata_t //don't touch it, because it also defined in type_camera.h
{
	char *data;
	int length;
}enc_extradata_t;


typedef enum
{
    BT601 = 0,
	BT709,
	YCC,
	VXYCC
}__cs_mode_t;

typedef enum __PIXEL_YUVFMT                         /* pixel format(yuv)                                            */
{
    PIXEL_YUV444 = 0x10,                            /* only used in scaler framebuffer                              */
    PIXEL_YUV422,
    PIXEL_YUV420,
    PIXEL_YUV411,
    PIXEL_CSIRGB,
    PIXEL_OTHERFMT,
    PIXEL_YVU420,
    PIXEL_YVU422,
    PIXEL_TILE_32X32,
    PIXEL_CSIARGB,
    PIXEL_CSIRGBA,
    PIXEL_CSIABGR,
    PIXEL_CSIBGRA,
    PIXEL_TILE_128X32,
    PIXEL_YUV420P,
    PIXEL_YVU420P,
	PIXEL_YUV422P,
	PIXEL_YVU422P,
    PIXEL_YUYV422,
    PIXEL_UYVY422,
    PIXEL_YVYU422,
    PIXEL_VYUY422
}__pixel_yuvfmt_t;

typedef enum
{
    M_ENCODE = 0,
    M_ISP_THUMB,
    M_ENCODE_ISP_THUMB,
    M_UNSUPPORT,
} __venc_output_mod_t;

typedef enum __COLOR_SPACE
{
    COLOR_SPACE_BT601,
    COLOR_SPACE_BT709,
    COLOR_SPACE_,
}__color_space_t;
#endif

#endif	// __H264__ENCODE__LIB__TYPE__H__

