sample_vo测试流程：
	从yuv原始数据文件xxx.yuv中读取视频帧，标记时间戳，送给mpi_vo组件显示。
	sample_vo也负责视频帧的帧管理，接收mpi_vo组件归还的视频帧，重装新帧，再送入mpi_vo组件显示。

读取测试参数的流程：
	sample提供了sample_vo.conf，测试参数都写在该文件中。
	启动sample_vo时，在命令行参数中给出sample_vo.conf的具体路径，sample_vo会读取sample_vo.conf，完成参数解析。
	然后按照参数运行测试。
	从命令行启动sample_vo的指令：
	./sample_vo -path /mnt/extsd/sample_vo.conf
	"-path /mnt/extsd/sample_vo.conf"指定了测试参数配置文件的路径。

测试参数的说明：
(1)yuv_file_path：指定yuv原始数据文件的路径。
(2)pic_width：指明yuv原始数据文件的视频帧的宽度
(4)pic_height：指明yuv原始数据文件的视频帧的高度
(5)display_width：指定输出图像的宽度
(6)display_height：指定输出图像的高度
(7)pic_format:指明yuv原始数据文件的视频帧的像素格式
(8)disp_type：指定显示设备类型，lcd,hdmi,cvbs
(9)frame_rate：指定播放yuv原始数据文件的帧率。