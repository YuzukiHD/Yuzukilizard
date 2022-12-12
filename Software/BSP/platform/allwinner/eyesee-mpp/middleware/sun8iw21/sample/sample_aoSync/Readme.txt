sample_aoSync 测试流程：
    演示接口AW_MPI_AO_SendFrameSync 的使用方法，采用同步的方式send pcm frame。而sample_ao 是采用异步的方式。
	根据配置参数读取pcm数据，然后播放声音，从耳机口输出声音。

读取测试参数的流程：
	sample提供了sample_aoSync.conf，测试参数包括：pcm音频文件路径(pcm_file_path)、采样率(pcm_sample_rate)、
	通道数目(pcm_channel_cnt)、数据位宽(pcm_bit_width)、每次取pcm的桢数目(pcm_frame_size)、音量(ao_volume)。
	启动sample_ao时，在命令行参数中给出sample_aoSync.conf的具体路径，sample_aoSync会读取该文件，完成参数解析。
	然后按照参数运行测试。

从命令行启动sample_aoSync的指令：
	./sample_aoSync -path /mnt/extsd/sample_aoSync.conf
	"-path /mnt/extsd/sample_aoSync.conf"指定了测试参数配置文件的路径。

测试参数的说明：
pcm_file_path：指定音频pcm文件的路径，该文件是包含wave头(大小为44Bytes)的wav格式文件，如果找不到这种格式文件，可以用sample_ai生成一个。
pcm_frame_size：指定每次取pcm 的帧数，用来与其他参数配合决定分配pcm frame buf 的大小。
ao_volume：指定音量大小。
test_duration：指定测试时间，单位：秒。

parse_wav_header_enable：指定是否使用sample 内部wav header 的解析。
注意：如果使能，wav 文件在sample 内部会自己解析这些参数，并覆盖下面指定的参数值（pcm_sample_rate、pcm_channel_cnt、pcm_bit_width）。

pcm_sample_rate：指定采样率，设置为文件中的采样率的值。
pcm_channel_cnt：指定通道数目，设置为文件中的通道数。
pcm_bit_width：指定位宽，设置为文件中的位宽。