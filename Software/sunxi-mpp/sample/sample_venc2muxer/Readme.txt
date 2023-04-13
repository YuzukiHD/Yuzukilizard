sample_venc2muxer测试流程：
	从yuv原始数据文件xxx.yuv中读取视频帧，编码，并由muxer进行封装生成相应的视频输出文件。

读取测试参数的流程：
	sample提供了sample_venc2muxer.conf，测试参数都写在该文件中。
	启动sample_venc2muxer时，在命令行参数中给出sample_venc2muxer.conf的具体路径，sample_venc2muxer会读取sample_venc2muxer.conf，完成参数解析。
	然后按照参数运行测试。
	从命令行启动sample_venc2muxer的指令：
	./sample_venc2muxer -path /mnt/extsd/sample_venc2muxer.conf
	"-path /mnt/extsd/sample_venc2muxer.conf"指定了测试参数配置文件的路径。

测试参数的说明：
(1)yuv_src_file：指定原始yuv文件的路径
(2)yuv_src_size：指定原始视频文件的视频大小，如1080p
(3)video_dst_file: 生成的视频文件路径
(4)video_size: 生成的视频文件视频大小，如1080p
(5)video_encoder: 视频编码方式
(6)video_framerate: 生成视频文件的帧率
(7)video_bitrate: 生成视频文件的码率
(8)video_duration: 生成一个视频文件的最大持续时间(单位s)（如每个视频文件长度一分钟）
(9)media_file_format:视频文件的封装格式(mp4 / ts)
(10)test_duration: sample一次测试时间（单位：s）