#ifndef _EISE_LIB_H_
#define _EISE_LIB_H_

#include "IEGcommon.h"

typedef struct _EISE_CFG_PARA_{
	int       in_h; //输入图像高度,4的倍数
	int       in_w;//输入图像宽度,4的倍数
	int       in_luma_pitch;//输入亮度pitch
	int       in_chroma_pitch;//输出色度pitch
	int       in_yuv_type;//输入yuv类型,默认0
	int       out_h;//输出高度
	int       out_w;//输出宽度
	int       out_luma_pitch;//输出亮度pitch
	int       out_chroma_pitch;//输出色度pitch
	int       out_yuv_type;//输出yuv类型
	int       fps;//输入帧率
	float     rt; // ISP读取Sensor输出单行像素所需时间
	float     ts; // 视频帧最后一行与第一行之间的时间差
	float     td; // 由于裁剪高度造成的延时
	float     k_matrix[9]; // 相机内参标定K矩阵
	float     k_matrix_inv[9]; // 相机内参标定K矩阵的逆阵
	float     stable_anglev[3]; // 陀螺仪转子固定旋转角速度
	float     angle_th[3]; // 虚拟帧间旋转角阈值
	int       radius[3]; // 滤波半径
	int       xy_exchange_en; // 陀螺仪坐标系和相机坐标系的XY轴是否对换,0:不对换;1:对换
	int       rolling_shutter; // rolling shutter顺序,0:从上到下;1:从下到上
	int       rs_correction_en;//rolling_shutter 纠正开关
	int       frame_rotation_en;//帧间旋转纠正开关
	int       filter_type; // 滤波方式,0:低通;1:均值
	int       src_width; // Sensor满分辨率宽度
	int       src_height; // Sensor满分辨率高度
	int       cut_height; // 输入图裁剪后高度
	int       hor_off;
	int       ver_off;
	int       fast_mode;
	int       g_en_angle_filter;
	int       g_interlace;
	int       max_frm_buf; // 最大滞后缓存帧数  max_frm_buf >= radius[1] + radius[2]
	int       max_next_frm; // 滤波序列右边最大帧数  max_next_frm = max_frm_buf - radius[1] - radius[2]
	int       old_radius_th0; // Old2New最后可用的滤波半径阈值
	int       old_radius_th1; // Old2New前一帧与当前帧的滤波半径之差阈值
	char      reserved[32];
}EISE_CFG_PARA;

typedef struct _EIS_PROCIN_PARA_{
	unsigned int		in_luma_phy_Addr;
	void*				in_luma_mmu_Addr;
	unsigned int		in_chroma_phy_Addr;
	void*				in_chroma_mmu_Addr;
	char				reserved[32];
}EIS_PROCIN_PARA;

typedef struct _EIS_PROCOUT_PARA_{
	unsigned int		out_luma_phy_Addr;
	void*				out_luma_mmu_Addr;
	unsigned int		out_chroma_u_phy_Addr;
	void*				out_chroma_u_mmu_Addr;
	char				reserved[32];
}EIS_PROCOUT_PARA;

typedef struct {
	unsigned long long time;
	float  ax;
	float  ay;
	float  az;
	float  vx;
	float  vy;
	float  vz;
} sGyroData;

typedef struct FrameData_EIS {
	int                fid;//帧ID
	int                gyro_num; // 当前帧所包含的可用gyro_data数量
	sGyroData          gyro_data[100];//目前最多单帧65个数据
	unsigned long long frame_stamp;//时间戳
	float              texp; // 当前帧的单行像素的曝光时长
	EIS_PROCIN_PARA    in_addr;//输入邋邋邋BUFFER
	char               reserved[32];
}EISE_FrameData;

// 接口函数
int EIS_Create(EISE_CFG_PARA *ise_cfg, EISE_HANDLE *handle);

void EIS_SetFrq(int freMHz, EISE_HANDLE *handle);

int EIS_setOutputAddr(EISE_HANDLE *handle, EIS_PROCOUT_PARA *fd);

int EIS_setFrameData(EISE_HANDLE *handle, EISE_FrameData *fd);

int EIS_Proc(EISE_HANDLE *handle, int *proc_id);

int EIS_Destroy(EISE_HANDLE *handle);

int EIS_Reset_Soft(EISE_HANDLE *handle);

int EIS_Set_UnitMatrix_Flag(EISE_HANDLE *handle, int flag);

#endif
