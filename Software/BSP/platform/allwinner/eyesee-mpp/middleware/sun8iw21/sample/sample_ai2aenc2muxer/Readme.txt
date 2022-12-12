sample_ai2aenc2muxer测试流程：
	根据配置参数采集对应的pcm数据，然后根据配置信息将pcm数据编码，最后写入到文件中进行保存。

读取测试参数的流程：
	sample提供了sample_ai2aenc2muxer.conf，测试参数包括：生成的编码音频文件路径(dst_file)、采样率(smp_rate)、
	通道数目(chn_cnt)、数据位宽(bit_width)、音频采集持续时间(cap_dura)。
	启动sample_ai2aenc2muxer时，在命令行参数中给出sample_ai2aenc2muxer.conf的具体路径，sample_ai2aenc2muxer会读取该文件，完成参数解析。
	然后按照参数运行测试。

从命令行启动sample_ai的指令：
	./sample_ai2aenc2muxer -path /mnt/extsd/sample_ai2aenc2muxer/sample_ai2aenc2muxer.conf
	"-path /mnt/extsd/sample_ai2aenc2muxer/sample_ai2aenc2muxer.conf"指定了测试参数配置文件的路径。

测试参数的说明：
(1)dst_file：指定目标pcm文件的路径，该文件是包含wave头的wav格式文件。
(2)smp_rate：指定采样率，通常设置为8000。
(3)chn_cnt：指定通道数目，通常为1或2。
(4)bit_width：指定位宽，必须设置为16。
(5)cap_dura：指定采集时间长度，如20代表20s。