sample_ai2aenc测试流程：
	mic录音送入编码器，取得每一帧数据的编码，写到文件中。

读取测试参数的流程：
	sample提供了sample_ai2aenc.conf，测试参数都写在该文件中。
	启动sample_ai2aenc时，在命令行参数中给出sample_ai2aenc.conf的具体路径，sample_ai2aenc会读取sample_ai2aenc.conf，完成参数解析。
	然后按照参数运行测试。
	从命令行启动sample_ai2aenc的指令：
	./sample_ai2aenc -path /mnt/extsd/sample_ai2aenc.conf
	"-path /mnt/extsd/sample_ai2aenc.conf"指定了测试参数配置文件的路径。

测试参数的说明：
(1)dst_file：生成文件路径
(2)encoder_type：编码类型，如“aac”
(3)sample_rate：mic录音采样值
(4)channel_cnt: 录音通道数 (1 or 2)
(5)bit_width:录音采样位宽
(6)frame_size: 帧大小 (如：1024 / 2048)
(7)test_duration: sample一次测试时间(单位：s)