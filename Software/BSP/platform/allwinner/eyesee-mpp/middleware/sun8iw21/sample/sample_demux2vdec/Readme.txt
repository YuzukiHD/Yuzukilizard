sample_demux2vdec测试流程：
	从原始视频文件（如：xxx.mp4）中分离出视频数据帧并解码生成yuv文件

读取测试参数的流程：
	sample提供了sample_demux2vdec.conf，测试参数都写在该文件中。
	启动sample_demux2vdec时，在命令行参数中给出sample_demux2vdec.conf的具体路径，sample_demux2vdec会读取sample_demux2vdec.conf，完成参数解析。
	然后按照参数运行测试。
	从命令行启动sample_demux2vdec的指令：
	./sample_demux2vdec -path /mnt/extsd/sample_demux2vdec.conf
	"-path /mnt/extsd/sample_demux2vdec.conf"指定了测试参数配置文件的路径。

测试参数的说明：
(1)src_file：指定原始视频文件的路径
(2)src_size：指定原始视频文件的视频大小，如1080p
(3)seek_position：指定原始视频文件的开始解析位置(ms)
(4)y_dst_file: 视频数据帧解码出来y数据分量
(5)u_dst_file: 视频数据帧解码出来u数据分量
(6)v_dst_file: 视频数据帧解码出来v数据分量
(7)yuv_dst_file: 视频数据帧解码出来对应的yuv数据文件
(8)test_duration: sample一次测试时间（单位：s）