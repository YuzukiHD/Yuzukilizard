/* *******************************************************************************
 * Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 * *******************************************************************************/
/**
 * @file dev_ctrl_adapter.h
 * @brief 设备公共控制适配器接口
 * @author id:826
 * @version v0.3
 * @date 2016-08-29
 */

#pragma once

#include "remote_connector.h"
#include "onvif/onvif_param.h"

#include <vector>
#include <map>
#include <stdint.h>

#define OSD_MAX_TEXT_LEN        128
#define OSD_MAX_COVER_NUM       4

#define AW_MAX_ENCODE_CHN_NUM   6

#define AW_STREAM_TYPE_MAIN     0
#define AW_STREAM_TYPE_SUB      1
#define AW_STREAM_TYPE_THIRD    2
#define AW_STREAM_TYPE_SNAP     3
#define AW_MAX_STREAM_TYPE      4

#define AW_MAX_CHN_NUM          2
#define ALARM_OUTPUT_NUM        1
#define ALARM_PTZ_POINT_NUM     8

#define EVENT_TYPE_VIDEO_LOSS   0x1
#define EVENT_TYPE_MD           0x2
#define EVENT_TYPE_COVER        0x4
#define EVENT_TYPE_INPUT        0x8
#define EVENT_TYPE_DISK_FULL    0x10
#define EVENT_TYPE_DISK_ERROR   0x20
#define EVENT_TYPE_NET_DISCONNECT   0x40
#define EVENT_TYPE_IP_CONFLIC   0x80

#define MOTION_AREA_WIDTH       22
#define MOTION_AREA_HEIGTH      18
#define MOTION_AREA_NUM         (((MOTION_AREA_WIDTH)*(MOTION_AREA_HEIGTH) + 7) / 8)

#define AW_MAX_DISK_NUM         1
#define DEVICE_NAME_SIZE        128
#define VERSION_TEXT_SIZE       128

#define AW_NET_MAX_DEV_NUM      8
#define AW_NET_INTERFACE_LEN    32
#define AW_NET_MAC_LEN          32
#define AW_NET_IP_ADDR_LEN      32

#define AW_FREQ_AUTO            0
#define AW_FREQ_50Hz            1
#define AW_FREQ_60HZ            2

/* 事件处理回调函数。 chn-触发事件的通道（vi通道、报警输入通道、硬盘ID号或无效）, event_type-触发事件的类型 */
typedef void (*EventHandleCallBack)(int chn, int event_type, void *arg);

typedef struct AWDeviceInfo_ {
    unsigned int device_id;
    char device_name[DEVICE_NAME_SIZE];
    char software[VERSION_TEXT_SIZE];
    char hardware[VERSION_TEXT_SIZE];
} AWDeviceInfo;

typedef struct AWNetAttr_ {
    char interface[AW_NET_INTERFACE_LEN];
    char mac[AW_NET_MAC_LEN];
    char ip[AW_NET_IP_ADDR_LEN];
    char mask[AW_NET_IP_ADDR_LEN];
    char gateway[AW_NET_IP_ADDR_LEN];
    char dns1[AW_NET_IP_ADDR_LEN];
    char dns2[AW_NET_IP_ADDR_LEN];
    int dhcp_enable;
} AWNetAttr;

typedef struct AWNetAttrList_ {
    int netdev_num;
    AWNetAttr net_attr[AW_NET_MAX_DEV_NUM];
} AWNetAttrList;

typedef struct AWWifiAp_{
    char ssid[32];              // WiFi ssid
    char mode;                  // refer to ENUM_AP_MODE
    char enctype;               // refer to ENUM_AP_ENCTYPE
    char signal;                // signal intensity 0--100%
    char status;
    // 0 : invalid ssid or disconnected
    // 1 : connected with default gateway
    // 2 : unmatched password
    // 3 : weak signal and connected
    // 4 : selected:
    //      - password matched and
    //      - disconnected or connected but not default gateway
} AWWifiAp;

typedef struct AWTimeOsd_ {
    int osd_enable;     // 时间 osd 显示状态. 0：不启用， 1：使能。
    int time_format;    // 时间显示格式. 0：24小时制， 1：12小时制AM/PM
    int date_format;    // 日期显示格式. 0：YYYY/MM/DD, 1:MM/DD/YYYY, 2:YYYY-MM-DD, 3:MM-DD-YYYY
    int left;           // 时间 osd 相对于左边的偏离位置. 该值为相对值, 比例为：0~10000
    int top;            // 时间 osd 相对于顶部的偏离位置. 该值为相对值, 比例为：0~10000
} AWTimeOsd;

typedef struct AWChannelOsd_ {
    int osd_enable;     // 通道名 osd 显示状态. 0：不启用， 1：使能。
    int left;           // 通道名 osd 相对于左边的偏离位置. 该值为相对值，比例为：0~10000
    int top;            // 通道名 osd 相对于顶部的偏离位置. 该值为相对值，比例为：0~10000
    char channel_name[OSD_MAX_TEXT_LEN];    // 通道名内容
} AWChannelOsd;

typedef struct AWPicOsd_ {
    int osd_enable;     // 通道名 osd 显示状态. 0：不启用， 1：使能。
    int left;           // 通道名 osd 相对于左边的偏离位置. 该值为相对值，比例为：0~10000
    int top;            // 通道名 osd 相对于顶部的偏离位置. 该值为相对值，比例为：0~10000
    char filename[OSD_MAX_TEXT_LEN];    // 需要叠加到osd的文件名
} AWPicOsd;

typedef struct AWDeviceOsd_ {
    int osd_enable;     // 设备名 osd 显示状态. 0：不启用， 1：使能。
    int left;           // 设备名 osd 相对于左边的偏离位置. 该值为相对值，比例为：0~10000
    int top;            // 设备名 osd 相对于顶部的偏离位置. 该值为相对值，比例为：0~10000
    char device_name[OSD_MAX_TEXT_LEN]; // 设备名内容
} AWDeviceOsd;

typedef struct AWCoverOsd_ {
    int osd_enable;     // 视频遮盖 osd 显示状态. 0：不启用， 1：使能。
    int cover_type;      // 视频遮盖类型. 0：全部遮盖， 1：仅编码遮盖， 2：仅预览遮盖
    int left;           // 视频遮盖 osd 相对于左边的偏离位置。该值为相对值，比例为：0~10000
    int top;            // 视频遮盖 osd 相对于顶部的偏离位置。该值为相对值，比例为：0~10000
    int width;          // 视频遮盖 osd 的宽度。该值为相对值，比例为：0~10000
    int heigth;         // 视频遮盖 osd 的高度。该值为相对值，比例为：0~10000
    int color;          // 视频遮盖块的颜色。按 RGB8888，设置。
} AWCoverOsd;

typedef struct AWImageColour_ {
    int brightness;     //亮度：   0～100
    int contrast;       //对比度： 0～100
    int saturation;     //饱和度： 0～100
    int sharpness;      //锐度：   0～100
    int hue;            //色度：   0～100
} AWImageColour;

typedef struct AWDayNightMode_ {
    int detect_type;    // 检查源类型。0-自动检测，1-光敏电阻检测，2-视频检测，
    int day_night_mode; // 日夜模式。0-白天，1-夜晚，2-自动，3-定时
    int trigger_leve;   // 触发灵敏度调节
    int delay_time;     // 触发后延时起效时间
    /* 日夜模式为定时，该时间段为白天模式，此外为夜间模式 */
    int start_hour;     // 开始时间
    int start_minute;
    int end_hour;       // 结束时间
    int end_minute;
} AWDayNightMode;

typedef struct AWEventHandle_ {
    int snap_enable;               // 报警触发 抓拍 功能
    int snap_num;                  // 抓拍张数。间隔为 0.5s
    char snap_chn[AW_MAX_CHN_NUM];  // 报警触发 相应通道的抓拍功能，可同时触发多个通道的抓拍

    int record_enable;             // 报警触发 录像 功能
    int record_time;               // 报警触发后，录像的时间长度，单位为s。
    char record_chn[AW_MAX_CHN_NUM];// 报警触发 相应通道的录像功能，可同时触发多个通道的录像

    int alarm_out_enable;          // 报警触发 输出报警 功能
    int alarm_time;                // 报警触发后，报警输出持续时间
    char alarm_out_chn[ALARM_OUTPUT_NUM]; // 报警触发 相应的报警输出通道，可同时触发多个输出

    int ptz_enable;                // 报警触发 PTZ 云台功能
    int ptz_delay;                 // PTZ预置点停留时间
    char ptz_point[ALARM_PTZ_POINT_NUM];    // 报警触发后，PTZ所跳转的预置点。

    int email_enable;              // 报警触发 Email 发送功能。
} AWEventHandle;

typedef struct AWScheduleTime_ {
    char start_hour;
    char start_min;
    char stop_hour;
    char stop_min;
}AWScheduleTime;

typedef struct AWAlarmMd_ {
    int md_enable;      // 对应channel的MD使能标志
    int channel;        // 需要检测MD的vi通道
    int sensitive;      // MD检测的灵敏度
    int delay_time;     // MD报警时间间隔，即，该时间段内，不重复报警。

    /*
     * 检测移动侦测区域，以bit为单位。默认输入分别率为D1即720*576，宏块大小为16*16。
     * 因此宽高的个数：45*36。bit为1时该块检测，为0时该块滤过。判断移动侦测时，更具MPP
     * 层提供的移动矩形框，与有效bit块进行判断，并结合灵敏度值。总体判断是否发生了移动侦测。
     * 但目前，仅支持全覆盖区域移动侦测检测
     */
    char detec_area[MOTION_AREA_NUM];

    AWEventHandle alarm_handle; // 报警联动处理
    int schedule_enable;        // 是否使能布放计划
    AWScheduleTime schedule_time[7][4]; // 布防时刻表 7天/4个时间段
} AWAlarmMd;

typedef struct AWAlarmCover_ {
    int cover_enable;   // 对应channel的遮盖检测使能标志
    int channel;        // 需要检测遮盖的vi通道
    int sensitive;      // 遮盖检测的灵敏度
    int delay_time;     // 遮盖报警时间间隔，即，该时间段内，不重复报警。
    AWEventHandle alarm_handle; // 报警联动处理
    int schedule_enable;        // 是否使能布放计划
    AWScheduleTime schedule_time[7][4]; // 布防时刻表 7天/4个时间段
} AWAlarmCover;

typedef struct AWDiskStatus_ {
    int disk_id;        // 磁盘ID号
    int disk_type;      // 磁盘类型：0-SD/TF卡, 1-U盘，2-硬盘
    int capacity;       // 磁盘容量，kb
    int free_space;     // 剩余空间，kb
    int disk_status;    // 磁盘状态：0-正常，1-未格式化，2-错误，3-文件系统不匹配，4-休眠
}AWDiskStatus;

typedef struct AWDiskInfo_ {
    int disk_num;       // 当前系统支持的磁盘数量. 0-无有效磁盘存在 >0-表示存在的有效磁盘个数
    AWDiskStatus disk_status[AW_MAX_DISK_NUM];
}AWDiskInfo;

typedef struct AWFormatStatus_ {
    int disk_id;        // 磁盘ID号
    int format_status;  // 格式化状态：0-格式化开始，1-正在格式化，2-格式化完成
    int process;        // 格式化进度。0～100
    int result;         // 格式化结果。0-成功，非0-失败
}AWFormatStatus;

typedef struct AWRecordAttr_ {
//    int channel;        // 需要进行录像的vi通道
    int record_type;    // 录像类型。0-手动录像，1-计划录像，2-关闭录像。该属性值设置成功即可控制录像的开始和停止
                        // 手动录像: 设置手动录像后，系统就一直录像，即使是下次开机之后也是如此。直到选择[关闭录像]后停止
                        // 计划录像: 设置计划录像后，则根据计划时刻表所选择参数进行录像。直到选择[关闭录像]后停止
                        // 关闭录像: 停止该通道的录像。
    int stream_type;    // 录像码流类型。0-主码流，1-子码流，2-抓拍图片
    int audio_enable;   // 音频录像使能。0-不录制音频符合流，1-录制音频符合流。
    int cover_enable;   // 循环覆盖使能。0-循环覆盖，1-停止循环覆盖
    int pack_time;      // 录像分包时间。0-不分包。>=0,分包时间单位s

    /* 计划录像时参数 */
    int perrecord_time; // 预录时间。>=0,时间单位s
    int schedule;       // 计划录像时间表。目前暂未支持该功能。
} AWRecordAttr;

typedef struct AWSnapAttr_ {
    int channel;    // 需要进行抓拍的vi通道
    int quality;    // 抓拍图像质量。0～4。0为最好。
    int pic_wide;   // 抓拍图片的宽度。
    int pic_high;   // 抓拍图片的高度
    int snap_delay; // 延时抓拍。>=0, 为0时，表示立即抓拍，时间单位为ms
    int snap_num;   // 抓拍张数。
    int interval;   // 连续抓拍间隔。时间单位为ms

    int status;     // 抓拍执行状态。0-完成抓拍，1-正在执行抓拍任务。
}AWSnapAttr;

typedef struct AWSystemTime_ {
    unsigned int year;
    unsigned int month;
    unsigned int day;
    unsigned int week;
    unsigned int hour;
    unsigned int minute;
    unsigned int second;
    unsigned int milliisecond;
}AWSystemTime;

typedef struct AWNtpConfig_ {
    int  ntp_enable;        // NTP校时是否启用：0－否，1－是
    int  ntp_port;          // NTP服务器端口，设备默认为123
    int  interval;          // 校时间隔时间（以小时为单位）
    int  time_zone;         // 时区选择
    char ntp_server[128];   // NTP服务器域名或者IP地址(需要做IP check)
}AWNtpConfig;

typedef struct AWEncodeSize_ {
    int width;
    int height;
}AWEncodeSize;

typedef struct AWEncodeROI_ {
    uint8_t index;      // 当前roi索引
    uint8_t enable;     // 是否启用当前roi
    uint8_t is_abs_qp;  // 是否使用绝对qp
    uint16_t qp;         // 当前roi QP值
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;
} AWEncodeROI;

typedef struct AWEncodeConfig_ {
    uint8_t enc_format;
    uint8_t bctype;     // 码率控制方式
    uint8_t profile;    // 编码profile
    uint8_t quality;    // 编码质量
    uint8_t fps;
    uint16_t gop;
    uint32_t bps;
    AWEncodeSize size;
    std::vector<AWEncodeROI> roi_config;
}AWEncodeConfig;

typedef struct AWSnapshotConfig_ {
    uint8_t enable;
    uint16_t qp;
    uint16_t speed;
} AWSnapshotConfig;

typedef struct AWVIDeviceConfig_ {
    uint8_t used;
    uint8_t index;
    uint16_t width;
    uint16_t height;
    std::vector<AWEncodeConfig> enc_config;
    AWSnapshotConfig snapshot_config;
} AWVIDeviceConfig;

typedef std::map<unsigned int, std::map<unsigned int, AWVIDeviceConfig> > AWMediaConfig;

typedef struct AWStreamHandle_ {
    unsigned int cam_id;    // 物理camera id
    unsigned int vi_chn;    // video input scaler 通道
    unsigned int enc_chn;   // 视频编码通道
} AWStreamHandle;

namespace EyeseeLinux {
    class IPresenter;
}

/**
 * @brief 用于对接第三方sdk的适配器接口类
 */
class DeviceAdapter
{
    public:
        DeviceAdapter() {};

        DeviceAdapter(EyeseeLinux::IPresenter *presenter);

        ~DeviceAdapter();

        /**
         * @brief 设备多媒体相关的控制接口
         */
        class MediaControl {
            public:
                virtual ~MediaControl() {}

                /**
                 * @brief 获取视频流handle列表
                 * @return
                 *  - 成功 0
                 *  - 失败 -1
                 */
                virtual int GetStreamHandle(std::vector<AWStreamHandle> &stream_handles) = 0;

                /**
                 * @brief 获取Onvif的简版profile
                 * @param ch 流通道号
                 * @param profile simple profile
                 * @return
                 *  - 成功 0
                 *  - 失败 -1
                 */
                virtual int GetOnvifSimpleProfile(const AWStreamHandle &stream_handle, OnvifSimpleProfile &profile) = 0;

                /**
                 * @brief 获取onvif视频编码配置参数选项
                 * @param options 编码配置参数选项
                 * @return
                 *  - 成功 0
                 *  - 失败 -1
                 */
                virtual int GetOnvifVideoEncodingConfigOptions(const AWStreamHandle &stream_handle, OnvifVideoEncodingConfigOptions &options) = 0;

                /**
                 * @brief 设置流分辨率
                 * @param width 视频宽度
                 * @param height 视频高度
                 * @param bitrate 视频码率, 单位 bps
                 * @param framerate 视频帧率
                 * @return
                 *  - 成功 0
                 *  - 失败 -1
                 */
                virtual int SetVideoQuality(const AWStreamHandle &stream_handle, int width, int height,
                                            int bitrate, int framerate) = 0;

                /**
                 * @brief 修改录像文件打包策略
                 * @param stragety
                 *  - 0 按录像时长
                 *  - 1 按录像文件大小
                 *  - ...
                 * @param value 数值
                 *  - stragety: 0 录像时长s
                 *  - stragety: 1 录像文件大小MBytes
                 * @return
                 *  - 成功 0
                 *  - 失败 -1
                 */
                virtual int SetVencPackStrategy(const AWStreamHandle &stream_handle, uint8_t stragety, uint32_t value) = 0;

                /**
                 * @brief 修改编码通道帧率
                 * @param framerate 视频帧率 fps
                 * @return
                 *  - 成功 0
                 *  - 失败 -1
                 */
                virtual int SetVencFrameRate(const AWStreamHandle &stream_handle, int framerate) = 0;

                /**
                 * @brief 修改编码通道码率
                 * @param bitrate 视频码率 bps
                 * @return
                 *  - 成功 0
                 *  - 失败 -1
                 */
                virtual int SetVencBitRate(const AWStreamHandle &stream_handle, int bitrate) = 0;

                /**
                 * @brief 修改编码通道I帧间隔
                 * @param val I帧间隔数量
                 * @return
                 *  - 成功 0
                 *  - 失败 -1
                 */
                virtual int SetVencIFrameInterval(const AWStreamHandle &stream_handle, int val) = 0;

                /**
                 * @brief 修改编码通道码率控制方式
                 * @param type 编码类型 0:CBR, 1:VBR
                 * @return
                 *  - 成功 0
                 *  - 失败 -1
                 */
                virtual int SetVencBitrateCtrl(const AWStreamHandle &stream_handle, int type) = 0;

                /**
                 * @brief 修改编码通道编码Profile
                 * @param type 编码Profile 0:baseline, 1:main profile 2: high profile
                 * @return
                 *  - 成功 0
                 *  - 失败 -1
                 */
                virtual int SetVencProfile(const AWStreamHandle &stream_handle, int profile) = 0;

                /**
                 * @brief 修改编码通道编码格式
                 * @param format 编码格式 0-H264, 1-H265
                 * @return
                 *  - 成功 0
                 *  - 失败 -1
                 */
                virtual int SetVencFormat(const AWStreamHandle &stream_handle, int format) = 0;

                /**
                 * @brief 修改编码ROI设置
                 * @param cfg roi配置
                 * @return
                 *  - 成功 0
                 *  - 失败 -1
                 */
                virtual int SetVencROI(const AWStreamHandle &stream_handle, const AWEncodeROI &cfg) = 0;

                /**
                 * @brief 获取流通道的rtsp url地址
                 * @param ch 流通道号
                 * @param url rtsp url地址
                 * @param size 地址长度
                 * @return
                 *  - 成功 0
                 *  - 失败 -1
                 */
                virtual int GetStreamUrl(const AWStreamHandle &stream_handle, char *url, int size) = 0;

                virtual int GetMediaConfig(AWMediaConfig &media_cfg) = 0;

                virtual int SaveMediaConfig(void) = 0;

                virtual int DefaultMediaConfig(void) = 0;

                virtual int SetVencSize(const AWStreamHandle &stream_handle, int width, int height) = 0;

                virtual int SetVencQuality(const AWStreamHandle &stream_handle, int quality) = 0;

                /**
                 * @brief 拍照/抓图
                 * @param ch 流通道
                 * @return
                 *  - 成功 0
                 *  - 失败 -1
                 */
                virtual int SnapShot(const AWStreamHandle &stream_handle) = 0;

                virtual int EnableSnapshotMode(const AWStreamHandle &stream_handle, int enable) = 0;

                virtual int SetSnapshotCfg(const AWStreamHandle &stream_handle, const AWSnapshotConfig &cfg) = 0;

                virtual int GetSnapshotCfg(const AWStreamHandle &stream_handle, AWSnapshotConfig &cfg) = 0;

                virtual int EnableVencRecord(const AWStreamHandle &stream_handle, int onoff) = 0;
        };

        /**
         *@brief 视频osd控制接口
         */
        class OverlayControl
        {
            public:
                virtual ~OverlayControl() {}

                /**
                 * @brief 设置时间OSD
                 * @param stream_id 编码通道，目前不支持对独立的编码通道进行设置
                 * @param time_osd 时间osd设置参数
                 * @return
                 *  - 成功 0
                 *  - 操作失败 -1
                 *  - OSD模块未初始化 -2
                 */
                virtual int SetTimeOsd(AWStreamHandle &stream_handle, const AWTimeOsd &time_osd) = 0;

                /**
                 * @brief 获取时间OSD相关参数
                 * @param stream_id 编码通道，目前不支持对独立的编码通道进行设置
                 * @param time_osd 时间osd设置参数
                 * @return
                 *  - 成功 0
                 *  - 操作失败 -1
                 *  - OSD模块未初始化 -2
                 */
                virtual int GetTimeOsd(AWStreamHandle &stream_handle, AWTimeOsd &time_osd) = 0;

                /**
                 * @brief 设置 通道名 OSD
                 * @param chn_osd 通道名osd设置参数
                 * @return
                 *  - 成功 0
                 *  - 操作失败 -1
                 *  - OSD模块未初始化 -2
                 */
                virtual int SetChannelOsd(AWStreamHandle &stream_handle, const AWChannelOsd &chn_osd) = 0;

                /**
                 * @brief 获取 通道名 OSD
                 * @param chn_osd 通道名osd设置参数
                 * @return
                 *  - 成功 0
                 *  - 操作失败 -1
                 *  - OSD模块未初始化 -2
                 */
                virtual int GetChannelOsd(AWStreamHandle &stream_handle, AWChannelOsd &chn_osd) = 0;

                /**
                 * @brief 设置 设备名 OSD
                 * @param device_osd 设备名osd设置参数
                 * @return
                 *  - 成功 0
                 *  - 操作失败 -1
                 *  - OSD模块未初始化 -2
                 */
                virtual int SetDeviceNameOsd(AWStreamHandle &stream_handle, const AWDeviceOsd &device_osd) = 0;

                /**
                 * @brief 获取 设备名 OSD
                 * @param device_osd 设备名osd设置参数
                 * @return
                 *  - 成功 0
                 *  - 操作失败 -1
                 *  - OSD模块未初始化 -2
                 */
                virtual int GetDeviceNameOsd(AWStreamHandle &stream_handle, AWDeviceOsd &device_osd) = 0;

                /**
                 * @brief 设置 遮盖 OSD
                 * @param cover_id 视频遮盖块ID
                 * - 0 <= cover_id < OSD_MAX_COVER_NUM（4）
                 * @param device_osd 设备名osd设置参数
                 * @return
                 *  - 成功 0
                 *  - 操作失败 -1
                 *  - OSD模块未初始化 -2
                 */
                virtual int SetCoverOsd(AWStreamHandle &stream_handle, int cover_id, const AWCoverOsd &cover_osd) = 0;

                /**
                 * @brief 获取 遮盖 OSD
                 * @param cover_id 视频遮盖块ID
                 * - 0 <= cover_id < OSD_MAX_COVER_NUM（4）
                 * @param device_osd 设备名osd设置参数
                 * @return
                 *  - 成功 0
                 *  - 操作失败 -1
                 *  - OSD模块未初始化 -2
                 */
                virtual int GetCoverOsd(AWStreamHandle &stream_handle, int cover_id, AWCoverOsd &cover_osd) = 0;

                virtual int SetPicOsd(AWStreamHandle &stream_handle, const AWPicOsd &pic_osd) = 0;

                virtual int GetPicOsd(AWStreamHandle &stream_handle, AWPicOsd &pic_osd) = 0;

                virtual int SaveConfig(void) = 0;

                virtual int DefaultConfig(void) = 0;
        };

        /**
         * @brief 图像image控制接口
         */
        class ImageControl
        {
            public:
                virtual ~ImageControl() {}

                /**
                 * @brief 设置 VI 图像调节
                 * @param image_colour 亮度，饱和度，对比度，锐度，色度值
                 * - 0 <= val =< 100
                 * @return
                 *  - 成功 0
                 *  - 操作失败 -1
                 */
                virtual int SetImageColour(AWStreamHandle &stream_handle, const AWImageColour &image_colour) = 0;

                /**
                 * @brief 获取 VI 图像调节
                 * @param image_colour 亮度，饱和度，对比度，锐度，色度值
                 * - 0 <= val =< 100
                 * @return
                 *  - 成功 0
                 *  - 操作失败 -1
                 */
                virtual int GetImageColour(AWStreamHandle &stream_handle, AWImageColour &image_colour) = 0;

                /**
                 * @brief 设置 VI 通道白平衡
                 * @param wb_type 白平衡类型
                 * - 0 关闭白平衡功能
                 * - 1 自动白平衡
                 * - 2 白炽灯
                 * - 3 暖光灯
                 * - 4 自然光
                 * - 5 白日光
                 * @return
                 *  - 成功 0
                 *  - 操作失败 -1
                 */
                virtual int SetImageWhiteBalance(AWStreamHandle &stream_handle, int wb_type) = 0;

                /**
                 * @brief 获取 VI 通道白平衡
                 * @param wb_type 白平衡类型
                 * - 0 自动白平衡
                 * - 1 白炽灯
                 * - 2 暖光灯
                 * - 3 自然光
                 * - 4 白日光
                 * @return
                 *  - 成功 0
                 *  - 操作失败 -1
                 */
                virtual int GetImageWhiteBalance(AWStreamHandle &stream_handle, int &wb_type) = 0;

                /**
                 * @brief 设置 VI 通道白平衡红蓝增益
                 * @param r_gain 红色增益 0~100
                 * @param b_gain 蓝色增益 0~100
                 * @return
                 *  - 成功 0
                 *  - 操作失败 -1
                 */
                virtual int SetImageWhiteBalanceRBGain(AWStreamHandle &stream_handle, int r_gain, int b_gain) = 0;

                /**
                 * @brief 获取 VI 通道白平衡红蓝增益
                 * @param r_gain 红色增益 0~100
                 * @param b_gain 蓝色增益 0~100
                 * @return
                 *  - 成功 0
                 *  - 操作失败 -1
                 */
                virtual int GetImageWhiteBalanceRBGain(AWStreamHandle &stream_handle, int &r_gain, int &b_gain) = 0;

                /**
                 * @brief 设置 VI 通道曝光调节
                 * @param exp_type 曝光类型
                 * - 0 自动曝光
                 * - 1 手动设置
                 * @param exp_time 曝光时间，该值为倒数，如exp_time=5，即1/（5+1）
                 * - >0
                 * @return
                 *  - 成功 0
                 *  - 操作失败 -1
                 */
                virtual int SetImageExposure(AWStreamHandle &stream_handle, int exp_type, int exp_time) = 0;

                /**
                 * @brief 获取VI 通道曝光调节
                 * @param exp_type 曝光类型
                 * - 0 自动曝光
                 * - 1 手动设置
                 * @param exp_time 曝光时间，该值为倒数，如exp_time=5，即1/（5+1）
                 * - >0
                 * @return
                 *  - 成功 0
                 *  - 操作失败 -1
                 */
                virtual int GetImageExposure(AWStreamHandle &stream_handle, int &exp_type, int &exp_time) = 0;

                /**
                 * @brief 设置 VI 通道曝光增益
                 * @param value
                 * - 1600~36000
                 * @return
                 *  - 成功 0
                 *  - 操作失败 -1
                 */
                virtual int SetImageExposureGain(AWStreamHandle &stream_handle, int value) = 0;

                /**
                 * @brief 获取VI 通道曝光增益
                 * @param value
                 * @return
                 *  - 成功 0
                 *  - 操作失败 -1
                 */
                virtual int GetImageExposureGain(AWStreamHandle &stream_handle, int &value) = 0;
                /**
                 * @brief 设置 VI 通道宽动态
                 * @param wd_leve 等级
                 * - 0 关闭宽动态
                 * - 1 宽动态1级
                 * - n 宽动态n级
                 * - 6 宽动态6级
                 * @return
                 *  - 成功 0
                 *  - 操作失败 -1
                 */
                virtual int SetImageWideDynamic(AWStreamHandle &stream_handle, int wd_leve) = 0;

                /**
                 * @brief 获取 VI 通道宽动态
                 * @param wd_leve 等级
                 * - 0 关闭宽动态
                 * - 1 宽动态1级
                 * - n 宽动态n级
                 * - 6 宽动态6级
                 * @return
                 *  - 成功 0
                 *  - 操作失败 -1
                 */
                virtual int GetImageWideDynamic(AWStreamHandle &stream_handle, int &wd_leve) = 0;

                /**
                 * @brief 设置 VI 通道的日夜模式（IRcut）
                 * @param mode 日夜模式相关参数
                 * @return
                 *  - 成功 0
                 *  - 操作失败 -1
                 */
                virtual int SetImageDayNightMode(AWStreamHandle &stream_handle, const AWDayNightMode &mode) = 0;

                /**
                 * @brief 获取 VI 通道的日夜模式（IRcut）
                 * @param mode 日夜模式相关参数
                 * @return
                 *  - 成功 0
                 *  - 操作失败 -1
                 */
                virtual int GetImageDayNightMode(AWStreamHandle &stream_handle, AWDayNightMode &mode) = 0;

                /**
                 * @brief 设置 VI 通道的降噪功能
                 * @param dn_leve 降噪级别
                 * - 0 关闭降噪功能
                 * - 1 降噪1级
                 * - n 降噪n级
                 * - 6 降噪1级
                 * @return
                 *  - 成功 0
                 *  - 操作失败 -1
                 */
                virtual int SetImageDenoise(AWStreamHandle &stream_handle, int dn_leve) = 0;

                /**
                 * @brief 获取 VI 通道的降噪级别
                 * @param dn_leve 降噪级别
                 * - 0 关闭降噪功能
                 * - 1 降噪1级
                 * - n 降噪n级
                 * - 6 降噪1级
                 * @return
                 *  - 成功 0
                 *  - 操作失败 -1
                 */
                virtual int GetImageDenoise(AWStreamHandle &stream_handle, int &dn_leve) = 0;

                /**
                 * @brief 设置  VI 通道的曝光频率
                 * @param freq 曝光频率 0-自动，1-50Hz, 2-60Hz
                 * - >= 0
                 * @return
                 *  - 成功 0
                 *  - 操作失败 -1
                 */
                virtual int SetImageFlickerFreq(AWStreamHandle &stream_handle, int freq) = 0;

                /**
                 * @brief 获取  VI 通道的曝光频率
                 * @param freq 曝光频率 0-自动，1-50Hz, 2-60Hz
                 * @return
                 *  - 成功 0
                 *  - 操作失败 -1
                 */
                virtual int GetImageFlickerFreq(AWStreamHandle &stream_handle, int &freq) = 0;

                /**
                 * @brief 设置  VI 通道的图像翻转
                 * @param enable_flip 图像翻转使能标志
                 * - 0 停止图像翻转
                 * - 1 使能图像翻转
                 * @return
                 *  - 成功 0
                 *  - 操作失败 -1
                 */
                virtual int SetImageFlip(AWStreamHandle &stream_handle, int enable_flip) = 0;

                /**
                 * @brief 获取  VI 通道的图像翻转
                 * @param enable_flip 图像翻转使能标志
                 * - 0 停止图像翻转
                 * - 1 使能图像翻转
                 * @return
                 *  - 成功 0
                 *  - 操作失败 -1
                 */
                virtual int GetImageFlip(AWStreamHandle &stream_handle, int &enable_flip) = 0;

                /**
                 * @brief 设置  VI 通道的图像镜像
                 * @param enable_mirror 图像镜像使能标志
                 * - 0 停止图像镜像
                 * - 1 使能图像镜像
                 * @return
                 *  - 成功 0
                 *  - 操作失败 -1
                 */
                virtual int SetImageMirror(AWStreamHandle &stream_handle, int enable_mirror) = 0;

                /**
                 * @brief 获取  VI 通道的图像镜像
                 * @param enable_mirror 图像镜像使能标志
                 * - 0 停止图像镜像
                 * - 1 使能图像镜像
                 * @return
                 *  - 成功 0
                 *  - 操作失败 -1
                 */
                virtual int GetImageMirror(AWStreamHandle &stream_handle, int &enable_mirror) = 0;

                virtual int SaveImageConfig() = 0;

                virtual int DefaultImageConfig() = 0;
        };

        /**
         * @brief 事件控制接口。事件主要包括：报警和异常，两大类。
         */
        class EventControl
        {
            public:
                virtual ~EventControl(){}

                /**
                 * @brief 设置 VI 通道的移动侦测
                 * @param alarm_md 移动侦测参数
                 * @return
                 *  - 成功 0
                 *  - 操作失败 -1
                 */
                virtual int SetAlarmMdConfig(const AWStreamHandle &stream_handle, const AWAlarmMd &alarm_md) = 0;

                /**
                 * @brief 获取 VI 通道的移动侦测参数
                 * @param alarm_md 移动侦测参数输出
                 * @return
                 *  - 成功 0
                 *  - 操作失败 -1
                 */
                virtual int GetAlarmMdConfig(const AWStreamHandle &stream_handle, AWAlarmMd &alarm_md) = 0;

                /**
                 * @brief 设置 VI 通道的遮盖侦测
                 * @param alarm_cover 遮盖检测参数
                 * @return
                 *  - 成功 0
                 *  - 操作失败 -1
                 */
                virtual int SetAlarmCoverConfig(const AWStreamHandle &stream_handle, const AWAlarmCover &alarm_cover) = 0;

                /**
                 * @brief 获取 VI 通道的遮盖侦测参数
                 * @param alarm_cover 遮盖检测参数
                 * @return
                 *  - 成功 0
                 *  - 操作失败 -1
                 */
                virtual int GetAlarmCoverConfig(const AWStreamHandle &stream_handle, AWAlarmCover &alarm_cover) = 0;

                /**
                 * @brief 设置 输入（高/低电平） 通道的报警检测
                 * @param alarm_chn 输入报警通道号
                 * - >= 0
                 * @param alarm_in 报警输入检测参数
                 * @return
                 *  - 成功 0
                 *  - 操作失败 -1
                 */
                virtual int SetAlarmInputConfig(int alarm_chn) = 0;

                /**
                 * @brief 获取 输入（高/低电平） 通道的报警检测参数
                 * @param alarm_chn 输入报警通道号
                 * - >= 0
                 * @param alarm_in 报警输入检测参数
                 * @return
                 *  - 成功 0
                 *  - 操作失败 -1
                 */
                virtual int GetAlarmInputConfig(int alarm_chn) = 0;

                /**
                 * @brief 获取相应通道的事件报警状态。用于轮询操作
                 * @param chn 事件源通道。 VI通道，报警输入通道，硬盘ID号等。
                 * - >= 0
                 * @param event_type 事件类型。不支持多事件类型'与'操作的同时查询。
                 * @param status 事件查询结果
                 * 0 - 无事件触发
                 * 1 - 相应事件触发
                 * @return
                 *  - 成功 0
                 *  - 操作失败 -1
                 */
                virtual int GetEventStatus(const AWStreamHandle &stream_handle, int event_type, int &status) = 0;

                /**
                 * @brief 注册相应事件触发的事件处理函数。
                 * @param event_type 事件类型。支持多事件类型'与'操作的同时注册。
                 * @param event_handle 对应事件的处理函数
                 * @return
                 *  - 成功 >=0 , 返回相应事件处理的ID号,用于注销时使用
                 *  - 操作失败 -1
                 */
                virtual int RegisterEventHandle(int event_type, EventHandleCallBack event_handle, void *arg) = 0;

                /**
                 * @brief 注销相应事件触发的事件处理函数。
                 * @param handle_id , 注册处理函数时返回的ID号
                 * @return
                 *  - 成功 0,
                 *  - 操作失败 -1
                 */
                virtual int UnregisterEventHandle(int handle_id) = 0;

                virtual int SaveEventConfig(void) = 0;

                virtual int DefaultEventConfig(void) = 0;
        };

        /**
         * @brief 录像管理接口
         */
        class RecordControl
        {
            public:
                virtual ~RecordControl() {}

                /**
                 * @brief 设置录像配置属性
                 * @param chn 录像通道，在大多数场景下，对应的是vi通道
                 * @param record_attr 录像属性输入
                 * @return
                 *  - 成功 0
                 *  - 操作失败 -1
                 */
                virtual int SetRecordAttr(int chn, const AWRecordAttr &record_attr) = 0;

                /**
                 * @brief 获取录像配置属性
                 * @param chn 录像通道，在大多数场景下，对应的是vi通道
                 * @param record_attr 录像属性输出
                 * @return
                 *  - 成功 0
                 *  - 操作失败 -1
                 */
                virtual int GetRecordAttr(int chn, AWRecordAttr &record_attr) = 0;

                /**
                 * @brief 设置抓拍属性。供单次抓拍任务使用
                 * @param chn 录像通道，在大多数场景下，对应的是vi通道
                 * @param record_attr 录像属性输入值
                 * @return
                 *  - 成功 0
                 *  - 操作失败 -1
                 */
                virtual int SetSnapAttr(int chn, const AWSnapAttr &snap_attr) = 0;

                /**
                 * @brief 获取抓拍属性。供单次抓拍任务使用
                 * @param chn 录像通道，在大多数场景下，对应的是vi通道
                 * @param record_attr 录像属性输出
                 * @return
                 *  - 成功 0
                 *  - 操作失败 -1
                 */
                virtual int GetSnapAttr(int chn, AWSnapAttr &snap_attr) = 0;

                /**
                 * @brief 根据抓拍属性，启动该次抓拍任务。供单次抓拍任务使用
                 * @param chn 录像通道，在大多数场景下，对应的是vi通道
                 * @param vi_ch 视频输入通道
                 * @return
                 *  - 成功 0
                 *  - 操作失败 -1
                 */
                virtual int DoSnaptask(int chn) = 0;
        };

        /**
         * @brief 系统控制接口
         */
        class SystemControl
        {
            public:
                virtual ~SystemControl() {}

                /**
                 * @brief 获取设备IP地址
                 * @param interface 网络接口名, 若传入NULL, 则获取当前系统默认网络通路的IP地址
                 * @param ipaddr ip地址, 地址格式为点分十进制
                 * @param size 地址长度
                 * @return
                 *  - 成功 0
                 *  - 失败 -1
                 */
                virtual int GetIpAddress(char *interface, char *ipaddr, int size) = 0;

                /**
                 * @brief 设置指定interface的网络属性
                 * @param net_attr 网络属性值
                 * net_attr->interface 指定修改属性的网络接口
                 * @return
                 *  - 成功 0
                 *  - 失败 -1
                 */
                virtual int SetNetworkAttr(const AWNetAttr &net_attr) = 0;

                /**
                 * @brief 获取指定interface的网络属性
                 * @param net_attr 网络属性值
                 * net_attr->interface 指定获取属性的网络接口
                 * @return
                 *  - 成功 0
                 *  - 失败 -1
                 */
                virtual int GetNetworkAttr(AWNetAttr &net_attr) = 0;

                /**
                 * @brief 获取系统中所有网络设备的网络属性值
                 * @param net_attr_list 网络属性值列表
                 * @return
                 *  - 成功 0
                 *  - 失败 -1
                 */
                virtual int GetNetworkAttrList(AWNetAttrList &net_attr_list) = 0;

                /**
                 * @brief 获取系统local时间
                 * @param struct tm
                 * @return
                 *  - 成功 0
                 *  - 失败 -1
                 */
                virtual int GetLocalDateTime(struct tm &tm) = 0;

                /**
                 * @brief 设置系统local时间
                 * @param struct tm
                 * @return
                 *  - 成功 0
                 *  - 失败 -1
                 */
                virtual int SetLocalDateTime(struct tm &tm) = 0;

                /**
                 * @brief 设置系统UTC时间
                 * @param struct tm
                 * @return
                 *  - 成功 0
                 *  - 失败 -1
                 */
                virtual int GetUTCDateTime(struct tm &tm) = 0;

                /**
                 * @brief 设置NTP服务配置
                 * @param ntp_cfg ntp配置
                 * @return
                 *  - 成功 0
                 *  - 失败 -1
                 */
                virtual int SetNtpConfig(const AWNtpConfig &ntp_cfg) = 0;

                /**
                 * @brief 获取NTP服务配置
                 * @param ntp_cfg ntp配置
                 * @return
                 *  - 成功 0
                 *  - 失败 -1
                 */
                virtual int GetNtpConfig(AWNtpConfig &ntp_cfg) = 0;

                virtual int GetApList(AWWifiAp *wifi_ap[], int cnt) = 0;

                virtual int SetSoftAp(const char *ssid, const char *pwd, int mode, int enctype, int freq) = 0;

                virtual int GetSoftAp(char *ssid, char *pwd, int *mode, int *enctype, int *freq) = 0;

                virtual int SetWifi(const char *ssid, const char *pwd) = 0;

                virtual int GetWifi(char *ssid, char *pwd, char *mode, char *enctype) = 0;

                /**
                 * @brief 恢复默认值
                 * @param void
                 * @return
                 *  - 成功 0
                 *  - 失败 -1
                 */
                virtual int DefaultSystemConfig(void) = 0;

                /**
                 * @brief 重启系统
                 * @param delay_system 延时重启时间，单位毫秒
                 * @return
                 *  - 成功 0
                 *  - 失败 -1
                 */
                virtual int RebootSystem(int delay_time) = 0;

                /**
                 * @brief 系统自动维护设置
                 * @param maintain_time 系统自动维护时间
                 * @return
                 *  - 成功 0
                 *  - 失败 -1
                 */
                virtual int SetSystemMaintain(int value) = 0;
                virtual int SetSystemMaintain(const AWSystemTime &maintain_time) = 0;
        };

        /**
         * @brief 设备控制接口
         */
        class DeviceControl
        {
            public:
                virtual ~DeviceControl() {}

                /**
                 * @brief 设置设备名
                 * @param dev_name 设备名
                 * @return
                 *  - 成功 0
                 *  - 失败 -1
                 */
                virtual int SetDeviceName(const char *dev_name) = 0;

                virtual int GetDeviceName(char *dev_name, uint8_t size) = 0;

                /**
                 * @brief 设置设备ID号
                 * @param dev_id 设备ID号
                 * >= 0
                 * @return
                 *  - 成功 0
                 *  - 失败 -1
                 */
                virtual int SetDeviceId(int dev_id) = 0;

                virtual int GetDeviceId(int *dev_id) = 0;

                /**
                 * @brief 获取设备信息
                 * @param dev_info 设备信息输出
                 * @return void
                 */
                virtual void GetDeviceInfo(AWDeviceInfo &dev_info) = 0;

                /* storage */
                /**
                 * @brief 获取系统磁盘信息
                 * @param disk_info 磁盘信息
                 * @return
                 *  - 成功 0
                 *  - 操作失败 -1
                 */
                virtual int GetDiskInfo(AWDiskInfo &disk_info) = 0;

                /**
                 * @brief 格式化硬盘，该操作最好做成非阻塞形式，通过获取状态来判断是否已经完成
                 * @param disk_id 磁盘ID号。
                 * - >= 0
                 * @return
                 *  - 成功 0
                 *  - 操作失败 -1
                 */
                virtual int FormatDisk(int disk_id) = 0;

                /**
                 * @brief 获取当前系统的格式化磁盘状态。
                 * @param disk_id 磁盘ID号。
                 * - >= 0
                 * @param format_status 输出的格式化状态信息
                 * @return
                 *  - 成功 0
                 *  - 操作失败 -1
                 */
                virtual int GetFormatStatus(int disk_id, AWFormatStatus &format_status) = 0;

                /**
                 * @brief 加载指定的磁盘
                 * @param disk_id 磁盘ID号。
                 * - >= 0
                 * @return
                 *  - 成功 0
                 *  - 操作失败 -1
                 */
                virtual int MountDisk(int disk_id) = 0;

                /**
                 * @brief 卸载指定的磁盘
                 * @param disk_id 磁盘ID号。
                 * - >= 0
                 * @return
                 *  - 成功 0
                 *  - 操作失败 -1
                 */
                virtual int UMountDisk(int disk_id) = 0;

                virtual int SaveDeviceConfig(void) = 0;

                virtual int DefaultDeviceConfig(void) = 0;
        };

    public:
        DeviceAdapter::MediaControl   *media_ctrl_;
        DeviceAdapter::DeviceControl  *dev_ctrl_;
        DeviceAdapter::SystemControl  *sys_ctrl_;
        DeviceAdapter::OverlayControl *overlay_ctrl_;
        DeviceAdapter::ImageControl   *image_ctrl_;
        DeviceAdapter::EventControl   *event_ctrl_;
        DeviceAdapter::RecordControl  *record_ctrl_;
};

