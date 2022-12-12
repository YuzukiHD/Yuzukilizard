#ifndef __MOTION_DETECT_H__
#define __MOTION_DETECT_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stdbool.h>

#define REGIONS_MAX_NUM             (4)  /*自定义多分区侦测最大区域个数*/
#define MAX_VE_MOTION_NUM           (50)  /*VE侦测最大返回区域个数*/

#define CROSS_LINE_MAX_NUM          (4)  /*虚拟线最大条数*/
#define CROSS_RECT_MAX_NUM          (1)  /*区域入侵最大区域个数*/
#define CROSS_RECT_MAX_POINT_NUM    (4)  /*入侵区域最大顶点个数*/


// 服务初始化
int aw_service_init();
// 服务反初始化
int aw_service_uninit();
// 服务开始
int aw_service_start();
// 服务结束
int aw_service_stop();


// 移动侦测区域
typedef struct {
	int left;
	int top;
	int right;
	int bottom;
} motion_rect;

typedef struct {
	bool enable;                            // 使能
	int sensitivity;                        // 灵敏度（0~100）
	int num;                                // 区域数量
	motion_rect regions[REGIONS_MAX_NUM]; // 区域参数
} detect_region_settings;

// 运动事件回调函数参数
typedef struct {
	int num;                    // 区域数量
	motion_rect rect[MAX_VE_MOTION_NUM];      // 区域参数
} motion_callback_data;
// 移动侦测回调函数
typedef int (*on_motion_callback_t)(const motion_callback_data data);

// 设置移动侦测回调函数
int set_motion_callback(on_motion_callback_t callback);
// 设置移动侦测参数设置
int set_motion_settings(detect_region_settings *settings);
// 获取移动侦测参数设置
int get_motion_settings(detect_region_settings *settings);



// 区域检测模式
typedef enum {
	CROSS_WORK_MODE_NONE,
	CROSS_WORK_MODE_LINE,
	CROSS_WORK_MODE_RECT,
	CROSS_WORK_MODE_HUMANOID
} work_mode;

// 越界入侵界线参数
typedef struct {
	int x1;
	int y1;
	int x2;
	int y2;
	int direction; // 触发方向 0：双向，1：左->右，2：右->左
} cross_line;
// 区域入侵参数
typedef struct {
	int x[CROSS_RECT_MAX_POINT_NUM];
	int y[CROSS_RECT_MAX_POINT_NUM];
	int direction;      // 触发方向 0：双向，1：进入，2：离开
} cross_rect;

typedef struct {
	work_mode mode;
	bool enable;                            // 使能
	int sensitivity;                        // 灵敏度（0~100）
	int outdoor;                            // 模式 0：室外，1：室内
	int num;                                // 区域数量
	union {
		cross_line lines[CROSS_LINE_MAX_NUM];   // 界线参数
		cross_rect rects[CROSS_RECT_MAX_NUM];   // 区域参数
	} settings;
} cross_settings;

// 越界、区域侦测回调函数参数
typedef struct {
	work_mode mode;
	int id;             // 区域id
	int type;           // 0:方向未知; 1:左->右/进入; 2:右->左/离开
	int x;
	int y;
} cross_callback_data;
// 区域入侵检测回调函数
typedef int (*on_cross_callback_t)(const cross_callback_data data);

// 设置区域入侵检测回调函数
int set_cross_callback(on_cross_callback_t callback);
// 设置区域入侵检测参数设置
int set_cross_settings(cross_settings *settings);
// 获取区域入侵检测参数设置
int get_cross_settings(cross_settings *settings);

typedef int (*on_humanoid_callback_t)(int left, int top, int right, int bottom);
// 设置人形检测回调函数
int set_humanoid_callback(on_humanoid_callback_t callback);










#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __MOTION_DETECT_H__ */