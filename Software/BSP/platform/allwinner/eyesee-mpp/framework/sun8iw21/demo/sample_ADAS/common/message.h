/*************************************************
Copyright (C), 2015, AllwinnerTech. Co., Ltd.
File name: record_preview_controller.h
Author: yinzh@allwinnertech.com
Version: 0.1
Date: 2015-11-5
Description:
    定义用于实现观察者模式中，消息类型
History:
*************************************************/
#ifndef _MESSAGE_H
#define _MESSAGE_H

enum{
    PTL_OFF = 0,
    PTL_3   = 3,
    PTL_5   = 5,
    PTL_10  = 10,
    PTL_15  = 15,
    PTL_20  = 20,
    PTL_30  = 30,
};

enum MSG_TYPE {
    MSG_STORAGE_MOUNTED = 0, // 0
    MSG_STORAGE_UMOUNT,
    MSG_STORAGE_FS_ERROR,
    MSG_STORAGE_IS_FULL,
    MSG_STORAGE_FORMAT_FINISHED,
    MSG_STORAGE_CAP_NO_SUPPORT,//5
    MSG_CAMERA_START_PREVIEW,
    MSG_CAMERA_STOP_PREVIEW,
    MSG_CAMERA_VIDEO_RESOLUTION,
    MSG_CAMERA_VIDEO_FPS,
    MSG_CAMERA_TAKEPICTURE_FINISHED,//10
    MSG_CAMERA_CONTINOUS_ON, 
    MSG_CAMERA_PIC_RESOLUTION,
    MSG_CAMERA_PIC_QUALITY,
    MSG_CAMERA_WHITE_BALANCE,
    MSG_CAMERA_CONTRAST,//15
    MSG_CAMERA_EXPOSURE, 
    MSG_CAMERA_LIGHTFREQ,
    MSG_CAMERA_TIME_WATERMARK,
    MSG_CAMERA_LICENSE_WATERMARK,
    MSG_RECORD_START,//20
    MSG_RECORD_STOP, 
    MSG_RECORD_FILE_DONE,
    MSG_VIDEO_PLAY_PREPARED,
    MSG_VIDEO_PLAY_START,
    MSG_PIC_PLAY_START,
    MSG_VIDEO_PLAY_PAUSE,//25
    MSG_VIDEO_PLAY_STOP, 
    MSG_VIDEO_PLAY_COMPLETION,
    MSG_IMPACT_OCCUR,
    MSG_UVC_PLUG_IN,
    MSG_UVC_PLUG_OUT,//30
    MSG_USB_HOST_CONNECTED, 
    MSG_USB_HOST_DETACHED,
    MSG_BATTERY,
    MSG_DELAY_SHUTDOWN,
    MSG_CANCEL_SHUTDOWN,
    MSG_BACK_CAR_IN, // 35
    MSG_BACK_CAR_OUT,
    MSG_TVD_PLUG_IN,
    MSG_TVD_PLUG_OUT,
    MSG_TVOUT_PLUG_IN,
    MSG_TVOUT_PLUG_OUT, // 40
    MSG_HDMI_PLUGIN,
    MSG_HDMI_PLUGOUT,
    MSG_SOFTAP_DISABLED,
    MSG_SOFTAP_ENABLE,
    MSG_SOFTAP_ENABLED, // 45
    MSG_WIFI_DISABLED,
    MSG_WIFI_ENABLE,
    MSG_WIFI_ENABLED,
    MSG_WIFI_CONNECTED,
    MSG_WIFI_DISCONNECTED,  // 50
    MSG_WIFI_SCAN_SCANING,
    MSG_WIFI_SCAN_END,
    // for preview window
    MSG_CAM_0_BUTTON_ON,
    MSG_CAM_0_BUTTON_OFF,
    MSG_CAM_1_BUTTON_ON, // 55
    MSG_CAM_1_BUTTON_OFF,
    MSG_BLUETOOTH_ENABLE,
    MSG_BLUETOOTH_DISABLE,
    MSG_ETH_DISCONNECT,
    MSG_ETH_CONNECT_LAN, // 60
    MSG_ETH_CONNECT_INTERNET,
    MSG_USB_PLUG_IN,
    MSG_USB_PLUG_OUT,
    MSG_CPU_TEMP_HIGH,
    MSG_CPU_TEMP_NORMAL, // 65
    MSG_SHUTDOWN_SYSTEM,
    MSG_VIDEO_MD_ALARM,
    MSG_VIDEO_COVER_ALARM,
    MSG_SOFTAP_SWITCH_DONE,
    MSG_WIFI_SWITCH_DONE, // 70
    MSG_ETH_SWITCH_DONE,
    //key event
    MSG_DOWN_KEY_DOWN, 
    MSG_UP_KEY_DOWN,
    MSG_MENU_KEY_DOWN,
    MSG_OK_KEY_DOWN, //75
    MSG_POWER_KEY_DOWN,
    MSG_HOME_KEY_DOWN,
    MSG_AUDIO_RECORD_CHANGE,
    MSG_TIME_TAKE_PIC_ON,
    MSG_TIME_TAKE_PIC_OFF,//80
    MSG_AUTO_TIME_TAKE_PIC_ON,
    MSG_AUTO_TIME_TAKE_PIC_OFF,
    MSG_TAKE_THUMB_PIC,
    MSG_RECORD_LOOP_CHANGE,
    MSG_RECORD_TIMELAPSE_CHANGE,//85
    MSG_TAKE_PIC_SIGNAL,
    MSG_TAKE_THUMB_VIDEO,
    MSG_CAMERA_SLOW_RESOULATION,
    MSG_CAMERA_ENABLE_OSD,
    MSG_CAMERA_DISABLE_OSD,//90
    MSG_CAMERA_CONTINOUS_OFF,
    MSG_CAMERA_SET_PHOTO,
    //battery status
    MSG_BATTERY_FULL,
    MSG_BATTERY_LOW,
    MSG_USB_CHARGING, // 95
    MSG_USB_MASS_STORAGE,
    MSG_USB_MASS_STORAGE_SD_REMOVE,
    MSG_DATABASE_IS_FULL,
    MSG_NEED_SWITCH_SUB_FILE,
    MSG_NEED_STOP_SUB_RECORD,
    MSG_CAMERA_TAKEPICTURE_ERROR,
    MSG_CAMERA_ON_ERROR,
    MSG_SYSTEM_POWEROFF,
    MSG_SHOW_HDMI_MASK,
    MSG_HIDE_HDMI_MASK,
	MSG_TAKE_THUMB_BACKCAMERA_PIC,
	MSG_TAKE_THUMB_BACKCAMERA_VIDEO,
	MSG_PREPARE_TO_SUSPEND,
	MSG_START_RESUME,
	MSG_IMPACT_HAPPEN,
	MSG_ACCON_HAPPEN,	
	MSG_ACCOFF_HAPPEN,
	MSG_CLOSE_STANDBY_DIALOG,
	MSG_PM_RECORD_START,
	MSG_PM_RECORD_STOP,
	MSG_UPDATED_SYSTEM_TIEM_BY_4G,
	MSG_PLAYBACK_DELETE_PLAY_BUTTON,
	MSG_PLAYBACK_NO_DETECT_FILE_SDCARD,
	MSG_PLAYBACK_DELETE_PLAY_IMAG_SH,
	//msg for screen save time over 1 minue to do window change
	MSG_TO_PREVIEW_WINDOW,
	MSG_BIND_SUCCESS,
	MSG_UNBIND_SUCCESS,
	MSG_DELETE_VIDEOFILE,
	MSG_ADAS_EVENT,
	MSG_AHD_CONNECT,
	MSG_AHD_REMOVE,
	MSG_CAMERA_MOTION_HAPPEN,
};	


#endif