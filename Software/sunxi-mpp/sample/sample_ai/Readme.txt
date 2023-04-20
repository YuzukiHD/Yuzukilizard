sample_ai测试流程：
	根据配置参数采集对应的pcm数据，并写入到文件中保存。

读取测试参数的流程：
	sample提供了sample_ai.conf，测试参数包括：生成的pcm文件路径(pcm_file_path)、采样率(pcm_sample_rate)、
	通道数目(pcm_channel_cnt)、数据位宽(pcm_bit_width)。
	启动sample_ai时，在命令行参数中给出sample_ai.conf的具体路径，sample_ai会读取该文件，完成参数解析。
	然后按照参数运行测试。

从命令行启动sample_ai的指令：
	./sample_ai -path /mnt/extsd/sample_ai/sample_ai.conf
	"-path /mnt/extsd/sample_ai/sample_ai.conf"指定了测试参数配置文件的路径。

测试参数的说明：
(1)pcm_file_path：指定目标pcm文件的路径，该文件是包含wave头的wav格式文件。
(2)pcm_sample_rate：指定采样率，通常设置为8000。
(3)pcm_channel_cnt：指定通道数目，通常为1或2。
(4)pcm_bit_width：指定位宽，必须设置为16。
(5)pcm_frame_size：指定frame_size，此值可不指定。