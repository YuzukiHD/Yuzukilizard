sample_demux测试流程：
	从视频文件（如：test.mp4）中分离出所有的视频、音频、subtitle数据，分别组成对应文件。

读取测试参数的流程：
	sample提供了sample_demux.conf，测试参数都写在该文件中。
	启动sample_demux时，在命令行参数中给出sample_demux.conf的具体路径，sample_demux会读取sample_demux.conf，完成参数解析。
	然后按照参数运行测试。
	从命令行启动sample_demux的指令：
	./sample_demux -path /mnt/extsd/sample_demux.conf
	"-path /mnt/extsd/sample_demux.conf"指定了测试参数配置文件的路径。

测试参数的说明：
(1)src_file：指定原始视频文件的路径
(2)src_size：指定原始视频文件的视频大小，如1080p
(3)seek_position：指定原始视频文件的开始解析位置(ms)
(4)video_dst_file:解析出来的视频数据生成的文件路径
(5)audio_dst_file:解析出来的音频数据生成的文件路径
(6)subtitle_dst_file:解析出来的文字数据生成的文件路径
(7)test_duration: sample一次测试时间（单位：s）