sample_aec测试流程：
	本sample主要用来演示aec（回声消除）功能的使用。
	sample创建ao，播放音频文件作为回声。
	sample创建ai，接收外界声音（其中必然包括ao播放出的音频）。mpi_ai启动回声消除功能。
	sample从mpi_ai组件获取采集的数据保存为wav文件，
	    在mpi_ai打开回声消除的情况下，wav文件应已过滤了ao播放的音乐。
	    如果没有打开回声消除，wav文件会混合外界说话声和ao播放的音乐。
	sample也包含了DRC（软件增强音量）的测试。

读取测试参数的流程：
	sample提供了sample_aec.conf，测试参数包括：pcm音频文件路径(dst_file)、采样率(pcm_sample_rate)、
	通道数目(pcm_channel_cnt)、数据位宽(pcm_bit_width)、每次取pcm的帧数目(pcm_frame_size)。
	启动sample_aec时，在命令行参数中给出sample_aec.conf的具体路径，sample_aec会读取该文件，完成参数解析。
	然后按照参数运行测试。

从命令行启动sample_aec的指令：
	./sample_aec -path /mnt/extsd/sample_aec/sample_aec.conf
	"-path /mnt/extsd/sample_aec/sample_aec.conf"指定了测试参数配置文件的路径。

测试参数的说明：
(1)pcm_src_path：指定音频pcm文件的路径，该文件是包含wave头(大小为44Bytes)的wav格式文件，如果找不到这种格式文件，可以用sample_ai生成一个。
(2)pcm_dst_path：指定目标文件的路径，该文件是ai组件采集音频生成的文件，可配置是否带wav头，如果想在PC上播放音频文件，需带wav头。
(3)pcm_sample_rate：指定采样率，设置为文件中的采样率的值，启用aec后，sample rate须为8000.
(4)pcm_channel_cnt：指定通道数目，设置为文件中的通道数，启用aec后，sample rate须为1. 
(5)pcm_bit_width：指定位宽，设置为文件中的位宽，启用aec后，bit_width须为16.
(6)pcm_frame_size：固定指定为1024。
(7)aec_en:	1:启动aec回声消除功能；0：不启用aec回声消除功能；
(8)aec_delay_ms： 启用aec回声消除功能时，设置的延迟时间，主要给aec回声消除算法使用，建议先设置为0；
(9)add_wav_header：保存pcm文件是否需要加wav头。


启用aec功能的方式如下：
	启用aec功能并不复杂，相比于不启用aec功能的操作，只是在设置ai dev属性时多了两个属性参数：
	1)ai_aec_en,是否使能aec，1：enable;0:not enable; 
	2)aec_delay_ms: aec算法用delay参数，unit：ms,建议先设为0；
	然后调用api：AW_MPI_AI_SetPubAttr();

