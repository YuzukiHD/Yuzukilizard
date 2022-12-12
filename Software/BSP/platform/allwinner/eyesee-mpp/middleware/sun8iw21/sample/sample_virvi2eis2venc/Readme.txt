sample_virvi2eis2venc测试流程：
	该例程用于测试防抖相关功能，支持离线仿真、在线调试模式，支持绑定、非绑定的组件形式。

1. 使用 config.mk 文件里面的 EIS_OFFLINE_SIMU ＝ Y/N 来控制是否使用离线仿真模式。
2. 使用 config.mk 文件里面的 EIS_MULTI_INSTANCE ＝ Y/N 来控制是否使用防抖多实例模式。
3. Bypass 模式需要修改根目录下的下面注释（用于旁路防抖输出的视频数据，通常用于防抖调试或者仿真的数据采集）：
	#if 0
		ViEisVencCfg.EisCfg.stEisAttr.eEisAlgoMode = EIS_ALGO_MODE_BP;
		ViEisVencCfg.EisCfg.stEisAttr.pBPDataSavePath = "/tmp";
		ViEisVencCfg.EisCfg.stEisAttr.bSaveYUV = 0;
	#endif
	并且该模式必须全部关闭 VO 打开编码，也就是 use_vo = 0，use_venc = 1
4. 使用 #define EIS_BYPASS_VENC 宏定义来控制是否开启离线模式下的编码，如果定义了该宏，那么会直接输出 YUV 文件。

#参数说明（如无特殊说明，适用于所有模式）
reboot_cnt = 1000  #仅用于在线模式，控制重启测试的次数
frame_cnt = 150    #控制采集的视频帧数

input_bufnum = 10  #输入 buffer 的数量，目前调试阶段建议值至少为 11
output_bufnum = 5  #输出 buffer 的数量，目前调试阶段建议值至少为 10
time_delay = 0     #控制陀螺仪与视频帧的时间戳对齐偏移，目前推荐 -33
sync_tolerance = 1 #时间戳对齐的容忍度，建议值 5 以内，不不不不不不不不能为 0
# src parameter
# dev number:0(CSI0)/1(CSI1)
# cap_width * cap_height:720p/1080p;
# fps: 30
video_dev = 1      #仅用于在线模式，控制采集视频的节点
cap_width = 1920   #仅用于在线模式，控制采集视频的分辨率
cap_height = 1080
fps = 30

Vo 如果与编码同时打开，则 Vo 是优先的，也就是编码的使能选项会被 VO 的使能选项覆盖掉。
# VO, type: [lcd]\[hdmi]
use_vo = 1         # 仅用于在线模式，控制是否开启视频实时预览（在多实例模式下该选项无效，强制打开）
disp_w = 1920      # 仅用于在线模式，控制视频显示的分辨率
disp_h = 1080
disp_type = "hdmi" # 仅用于在线模式，视频显示的终端类型

# VENC: default use EIS output size
use_venc = 0       # 在多实例模式下该选项无效，强制打开，离线仿真模式下该选项使用 EIS_BYPASS_VENC 来控制。

# VENC save file and offline simulation mode save file path.
save_file = "/mnt/extsd/SampleEis2Venc.H264" # 控制编码输出文件的保存位置（离线模式如果不开编码则默认保存在 /tmp 目录下，路径为：/tmp/SampleEis2File.yuv）

gyro_file = "/mnt/extsd/SampleEis2VencGyro.txt" #仅用于离线模式，指定输入的陀螺仪数据文件
video_file = "/mnt/extsd/SampleEis2VencVideo.yuv" #仅用于离线模式，指定输入的视频数据文件
timestamp_file = "/mnt/extsd/SampleEis2VencTs.txt" #仅用于离线模式，指定输入的视频的时间戳数据文件
