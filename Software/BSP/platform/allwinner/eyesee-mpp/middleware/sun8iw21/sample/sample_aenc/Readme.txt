sample_aenc测试流程：
	从pcm文件（如：sample_aenc.wav）中读取每一桢的数据，进行编码，然后保存为aac/mp3/adpcm/pcm/g711a/g711u/g726格式的压缩文件。

读取测试参数的流程：
	sample提供了sample_aenc.conf，测试参数包括：原文件路径(sample_aenc_src_file)、目标文件路径(sample_aenc_dst_file)。
	启动sample_aenc时，在命令行参数中给出sample_aenc.conf的具体路径，sample_aenc会读取该文件，完成参数解析。
	然后按照参数运行测试。

从命令行启动sample_aenc的指令：
	./sample_aenc -path /mnt/extsd/sample_aenc/sample_aenc.conf
	"-path /mnt/extsd/sample_aenc/sample_aenc.conf"指定了测试参数配置文件的路径。

测试参数的说明：
(1)sample_aenc_src_file：指定原始pcm文件的路径，该文件是包含wave头的wav格式文件。
(2)sample_aenc_dst_file：指定编码后生成的aac或mp3或其它格式文件的路径。
(3)sample_aenc_dst_file的后缀名必须按为小写的，用于指定编码方式，否则按aac格式来编码。