sample_rtsp：
	演示通过RTSP方式查看编码码流的场景。

测试方法：
	【PHY】
	1. 配置网络IP：ifconfig eth0 169.254.225.12 netmask 255.255.0.0
	2. 执行测试程序：/mnt/extsd/sample_rtsp -path /mnt/extsd/sample_rtsp.conf
	3. 在PC端使用VLC软件的网络串流连接：rtsp://169.254.225.12:8554/ch0
	
	【WLAN】
	1. 配置网络IP：ifconfig wlan0 169.254.225.12 netmask 255.255.0.0
	2. 执行测试程序：/mnt/extsd/sample_rtsp -path /mnt/extsd/sample_rtsp.conf
	3. 在PC端使用VLC软件的网络串流连接：rtsp://169.254.225.12:8554/ch0
	
目前编码支持测试的功能:
	RC控制
	QP设置
	GOP设置
	高级跳帧参考
	Rotate
	Mirror
	彩转灰
	3D降噪
	ROI
	ROI背景帧率
	P帧帧内刷新
	ORL
	Crop

读取测试参数的流程：
    sample提供了sample_rtsp.conf，测试参数都写在该文件中。
启动sample_rtsp时，在命令行参数中给出sample_rtsp.conf的具体路径，sample_rtsp会读取sample_rtsp.conf，完成参数解析。
然后按照参数运行测试。

从命令行启动sample_rtsp的指令：
    ./sample_rtsp -path /home/sample_rtsp.conf
    "-path /home/sample_rtsp.conf"指定了测试参数配置文件的路径。

测试参数的说明：
dev_node：视频数据的获取节点（0:表示/dev/video0  1:表示/dev/video1）
src_size：设定源视频数据的大小，如1080/720
vi_buffer_num: 指定VI buffer个数
src_pixfmt: 指定源视频像素格式
color_space: 指定颜色空间类型
video_dst_file: 生成的视频文件路径（默认录制一个文件的时长为1分钟，如果sample测试超过一分钟，则下个文件会自动的在此文件名上加上数字后缀，以示区分）
dst_file_max_cnt: 指定最多文件个数
video_framerate: 生成视频文件的帧率
video_bitrate: 生成视频文件的码率
video_size: 生成的视频文件大小，如1080p
video_encoder: 视频编码方式
profile: 指定视频编码profile
product_mode: 指定视频编码产品模式
key_frame_interval: 指定视频编码关键帧间隔，单位：帧数
rc_mode: 指定视频编码码率控制模式
qp0: 指定视频编码QP
qp1: 指定视频编码QP
gop_mode: 指定视频编码GOP模式
gop_size: 指定视频编码GOP大小
AdvancedRef_Base:高级跳帧参考的base设置,0表示关闭; 如果开启高级跳帧参考,建议10.
AdvancedRef_Enhance:高级跳帧参考的enhance设置,建议1.
AdvancedRef_RefBaseEn:高级跳帧参考的RefBase设置. 1:开启,0:关闭.
enable_fast_enc: 视频编码快速编码开关
enable_smart: 视频编码smart功能开关
svc_layer: 指定SVC layer
encode_rotate: 指定视频编码旋转角度
mirror: 视频编码水平镜像开关
video_duration: 视频时长，单位:s
test_duration: 测试时间，单位: s
color2grey: 视频编码彩转灰开关
3dnr: 视频编码3D降噪开关
roi_num: 指定视频编码ROI个数
roi_BgFrameRate: 指定视频编码ROI背景帧率
IntraRefresh_BlockNum: 视频编码P帧帧内刷新宏块数
orl_num: 指定视频编码ORL数量
vbvBufferSize: 指定视频编码VBV buffer大小
vbvThreshSize: 指定视频编码VBV buffer阈值
crop_en: 视频编码crop使能
crop_rect_x: 指定视频编码crop矩形X
crop_rect_y: 指定视频编码crop矩形Y
crop_rect_w: 指定视频编码crop矩形宽
crop_rect_h: 指定视频编码crop矩形高