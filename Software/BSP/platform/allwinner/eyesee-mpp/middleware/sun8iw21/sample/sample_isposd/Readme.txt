sample_isposd测试流程：
	该sample测试mpi_vi和mpi_venc组件绑定。
	创建mpi_vi和mpi_venc，将它们绑定，再分别启动。mpi_vi采集图像，调用mpi_vi相关接口获取实时的ISP参数添加overlay到VENC通道.
读取测试参数的流程：
	sample提供了sample_isposd.conf，测试参数都写在该文件中。
	启动sample_isposd时，在命令行参数中给出sample_isposd.conf的具体路径，sample_isposd会读取sample_isposd.conf，完成参数解析。
	然后按照参数运行测试。
	从命令行启动sample_isposd 的指令：
	./sample_isposd -path /mnt/extsd/sample_isposd.conf
	"-path /mnt/extsd/sample_isposd.conf"指定了测试参数配置文件的路径。

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

须知：sample_isposd从sample_region移植，详情可参考sample_region
