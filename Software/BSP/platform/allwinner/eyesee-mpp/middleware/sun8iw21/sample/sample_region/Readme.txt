sample_region测试流程：
	该sample测试mpi_vi和mpi_vo组件或者mpi_vi和mpi_venc组件绑定。
	创建mpi_vi和mpi_vo，将它们绑定，再分别启动。mpi_vi采集图像，添加overlay和cover不同的region到VI通道，传输给mpi_vo显示。
	也可以选择加入mpi_vi和mpi_venc组件绑定，把region添加到VENC通道上，保存编码后的视频数据文件，可以用VLC播放。
	到达测试时间后，分别停止运行并销毁。也可以手动按ctrl-c，终止测试。

读取测试参数的流程：
	sample提供了sample_region.conf，测试参数都写在该文件中。
	启动sample_region时，在命令行参数中给出sample_region.conf的具体路径，sample_region会读取sample_region.conf，完成参数解析。
	然后按照参数运行测试。
	从命令行启动sample_region 的指令：
	./sample_region -path /mnt/extsd/sample_region.conf
	"-path /mnt/extsd/sample_region.conf"指定了测试参数配置文件的路径。

测试参数的说明：
(1)capture_width：指定camera采集的图像宽度
(2)capture_height：指定camera采集的图像高度
(3)pic_format：指定camera采集的图像格式
(4)frame_rate：指定camera采集的帧率
(5)test_duration：指定测试时间
(6)overlay_x,overlay_y,overlay_w，overlay_h：overlay类型的region参数
(7)cover_x,cover_y,cover_w,cover_h:cover类型的region参数
(8)add_venc_channel:是否选择VENC通道
(9)encoder_count：保存编码视频帧数
(10)bit_rate：编码位率
(11)encoder_type：编码也类型   #H.264，H.264, MJPEG
(12)output_file_path：编码视频保存路径
