sample_timelapse演示缩时录影的实现：
	从camera节点取vi输入数据，对venc组件设置取帧间隔，设置编码帧率，并对数据进行编码封装，生成对应的视频输出文件。
	
读取测试参数的流程：
	sample提供了sample_timelapse.conf，测试参数都写在该文件中。
	启动sample_timelapse时，在命令行参数中给出sample_timelapse.conf的具体路径，sample_timelapse会读取sample_timelapse.conf，完成参数解析。
	然后按照参数运行测试。
	从命令行启动sample_timelapse的指令：
	./sample_timelapse -path /mnt/extsd/sample_timelapse.conf
	"-path /mnt/extsd/sample_timelapse.conf"指定了测试参数配置文件的路径。

测试参数的说明：
(1)vipp_index: 获取视频数据的VIPP编号(0~4)
(2)vipp_width: vipp输出视频图像的宽度
(3)vipp_height: vipp输出视频图像的高度
(4)vipp_frame_rate: vipp的采集帧率
(5)timelapse: 缩时录影模式下取帧的帧间隔，单位us。
(6)video_frame_rate: 编码帧率，即设置编码文件的播放帧率。
(7)video_duration: 编码文件的播放时长。
(8)video_bitrate: 编码码率，单位Mbit/s。
(9)video_file_path: 编码文件的存储路径。