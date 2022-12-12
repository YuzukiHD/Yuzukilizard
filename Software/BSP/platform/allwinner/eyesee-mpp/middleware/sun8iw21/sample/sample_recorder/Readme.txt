sample_recorder 支持最大四路录制编码封装或者预览显示。
每路创建一个vipp和虚通道，根据参数的不同与编码器绑定测试录制编码，与VO绑定测试预览显示。

参数说明：
recorder1_vi_dev = 0					VIPP设备号
recorder1_isp_dev = 0					ISP设备号
recorder1_cap_width	= 1920				录制分辨率宽度
recorder1_cap_height = 1080				录制分辨率高度
recorder1_cap_frmrate = 20				录制帧率
recorder1_cap_format = "nv21"			录制视频格式
recorder1_vi_bufnum	= 5					配置VIPP设备buffer数量
recorder1_enable_WDR = 0				是否启用WDR模式(0：不启用 1：启用)
recorder1_enc_online = 0				是否启用在线编码模式(0：不启用 1：启用)
recorder1_enc_online_share_bufbum = 2	在线编码共享buffer个数
recorder1_enc_type = "H.265"			编码器类型
recorder1_enc_width = 1920				编码视频分辨率宽度
recorder1_enc_height = 1080				编码视频分辨率高度
recorder1_enc_frmrate = 20				编码视频帧率
recorder1_enc_bitrate = 2097152			编码视频码率
recorder1_enc_rcmode = 0				编码码率控制模式
recorder1_disp_x = 0					显示区域X坐标
recorder1_disp_y = 0					显示区域y坐标
recorder1_disp_width = 1920				显示区域宽度
recorder1_disp_height = 1080			显示区域高度
recorder1_disp_dev	= "lcd"				显示设备类型
recorder1_rec_duration = 20				视频文件录制时长
recorder1_rec_file_cnt = 3				视频文件最大分片数
recorder1_rec_file = "/mnt/extsd/recorder1_1080p@20.mp4"	视频文件保存路径

支持录制格式：
	nv21、yv12、nv12、yu12、aw_fbc、aw_lbc_2_0x、aw_lbc_2_5x、aw_lbc_1_5x、aw_lbc_1_0x
支持编码格式
	H.264、 H.265、MJPEG
支持封装格式
	MP4、TS

将recordern_vi_dev设置为 -1 ，表示不启用这组配置测试。
将recordern_enc_width 或者 recordern_enc_height设置为 0，表示不测试这组配置的视频编码与封装。
将recordern_disp_width 或 recordern_disp_width设置为0，表示不测试这组配置的预览显示。
视频编码与预览显示不可在同一组配置中同时使用。