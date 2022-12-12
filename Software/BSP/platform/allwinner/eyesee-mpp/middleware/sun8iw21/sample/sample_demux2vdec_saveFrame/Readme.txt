sample_demux2vdec_saveFrame测试流程：
	从原始视频文件（如：264.mp4）中分离出视频数据帧并解码生成yuv文件

读取测试参数的流程：
	sample提供了sample_demux2vdec_saveFrame.conf，测试参数都写在该文件中。
	启动sample_demux2vdec_saveFrame时，在命令行参数中给出sample_demux2vdec_saveFrame.conf的具体路径，sample_demux2vdec_saveFrame会读取sample_demux2vdec_saveFrame.conf，完成参数解析。
	然后按照参数运行测试。
	从命令行启动sample_demux2vdec_saveFrame的指令：
	./sample_demux2vdec_saveFrame -path /mnt/extsd/sample_demux2vdec_saveFrame.conf
	"-path /mnt/extsd/sample_demux2vdec_saveFrame.conf"指定了测试参数配置文件的路径。

测试参数的说明：
(1)src_file：指定原始视频文件的路径
(3)seek_position：指定原始视频文件的开始解析位置(ms)
(4)save_num: 保存视频帧的数量
(5)dst_dir: 保存视频帧的目录