sample_ao_resample_mixer测试流程：
	根据配置参数读取pcm数据，然后播放声音，从耳机口输出声音。

读取测试参数的流程：
	sample提供了sample_ao_resample_mixer.conf，测试参数包括：pcm音频文件路径(dst_file)、采样率(pcm_sample_rate)、
	通道数目(pcm_channel_cnt)、数据位宽(pcm_bit_width)、每次取pcm的桢数目(pcm_frame_size)。
	启动sample_ao_resample_mixer时，在命令行参数中给出sample_ao_resample_mixer.conf的具体路径，sample_ao_resample_mixer会读取该文件，完成参数解析。
	然后按照参数运行测试。

从命令行启动sample_ao_resample_mixer的指令：
	./sample_ao_resample_mixer -path /mnt/extsd/sample_ao_resample_mixer/sample_ao_resample_mixer.conf
	"-path /mnt/extsd/sample_ao_resample_mixer/sample_ao_resample_mixer.conf"指定了测试参数配置文件的路径。

测试参数的说明：
(1)pcm_file_path：指定音频pcm文件的路径，该文件是包含wave头(大小为44Bytes)的wav格式文件，如果找不到这种格式文件，可以用sample_ai生成一个。
(2)pcm_sample_rate：指定采样率，设置为文件中的采样率的值。
(3)pcm_channel_cnt：指定通道数目，设置为文件中的通道数。
(4)pcm_bit_width：指定位宽，设置为文件中的位宽。
(5)pcm_frame_size：固定指定为1024。
(6)pcm_second_chl_en: 是否启动第二路ao,以测试resample及混音功能。


slave file parameters:

pcm_file_path_slave：指定音频pcm文件的路径，该文件是包含wave头(大小为44Bytes)的wav格式文件，如果找不到这种格式文件，可以用sample_ai生成一个。
pcm_sample_rate_slave：指定采样率，设置为文件中的采样率的值。
pcm_channel_cnt_slave：指定通道数目，设置为文件中的通道数。
pcm_bit_width_slave：指定位宽，设置为文件中的位宽。
pcm_frame_size_slave：固定指定为1024。
