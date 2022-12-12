sample_demux2adec2ao测试流程：
	根据配置参数读取视频文件，解封装，获取到音频数据后，送解码器，然后播放出声音，从耳机口输出声音。

读取测试参数的流程：
	sample_demux2adec2ao提供了sample_demux2adec2ao.conf，测试参数包括：视频文件路径(src_file)。
	启动sample_demux2adec2ao时，在命令行参数中给出sample_demux2adec2ao.conf的具体路径，sample_demux2adec2ao会读取该文件，完成参数解析。
	然后按照参数运行测试。

从命令行启动sample_demux2adec2ao的指令：
	./sample_demux2adec2ao -path /mnt/extsd/sample_demux2adec2ao/sample_demux2adec2ao.conf
	"-path /mnt/extsd/sample_demux2adec2ao/sample_demux2adec2ao.conf"指定了测试参数配置文件的路径。

测试参数的说明：
(1)src_file：指定mp4视频文件的路径。