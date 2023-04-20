sample_adec测试流程：
	从已编码的ADTS格式的AAC文件（如：test.aac）中读取每一桢的数据，进行解码，然后保存为WAV格式的pcm文件。

读取测试参数的流程：
	sample提供了sample_adec.conf，测试参数包括：原文件路径(aac_file_path)和目标文件路径(pcm_file_path)。
	启动sample_adec时，在命令行参数中给出sample_adec.conf的具体路径，sample_adec会读取该文件，完成参数解析。
	然后按照参数运行测试。

从命令行启动sample_adec的指令：
	./sample_adec -path /mnt/extsd/sample_adec/sample_adec.conf
	"-path /mnt/extsd/sample_adec/sample_adec.conf"指定了测试参数配置文件的路径。

测试参数的说明：
(1)aac_file_path：指定原始已压缩的音频文件的路径，该文件必须为aac格式的文件。
(2)pcm_file_path：指定解码aac后生成的pcm文件的路径。