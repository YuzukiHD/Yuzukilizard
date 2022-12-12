sample_ai2ao测试流程：
	根据配置从mic采集音频数据，然后播放声音，从耳机口或喇叭输出声音。

读取测试参数的流程：
	sample提供了sample_ai2ao.conf，测试参数包括：采样率(pcm_sample_rate)、
	通道数目(pcm_channel_cnt)、数据位宽(pcm_bit_width)、每次取pcm的桢数目(pcm_frame_size)。
	启动sample_ai2ao时，在命令行参数中给出sample_ai2ao.conf的具体路径，sample_ai2ao会读取该文件，完成参数解析。
	然后按照参数运行测试。

从命令行启动sample_ai的指令：
	./sample_ai2ao -path /mnt/extsd/sample_ai2ao/sample_ai2ao.conf
	"-path /mnt/extsd/sample_ai2ao/sample_ai2ao.conf"指定了测试参数配置文件的路径。

测试参数的说明：
(1)pcm_sample_rate：指定mic采样率。
(2)pcm_channel_cnt：指定mic通道数目
(3)pcm_bit_width：指定位宽，设置为文件中的位宽
(4)pcm_frame_size：固定指定为1024
