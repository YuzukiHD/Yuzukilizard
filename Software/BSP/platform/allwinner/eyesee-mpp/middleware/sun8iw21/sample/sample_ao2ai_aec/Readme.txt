sample_ao2ai测试流程：
	本sample主要用来演示aec功能的使用，sample创建两路ai，一路通过tunnel mode 绑定于audio 
	enc，直接aac编码，并保存编码后的数据；一路采用非tunnel模式，由app获取ai数据，做其他处理。
	sample运行过程如下：
	根据配置参数读取pcm数据，然后播放声音。同时ai采集音频数据，并做aec回声消除处理，后送aac编码或直接获取aec后的数据用做他用。

读取测试参数的流程：
	sample提供了sample_ao2ai.conf，测试参数包括：pcm音频文件路径(dst_file)、采样率(pcm_sample_rate)、
	通道数目(pcm_channel_cnt)、数据位宽(pcm_bit_width)、每次取pcm的桢数目(pcm_frame_size)。
	启动sample_ao2ai时，在命令行参数中给出sample_ao2ai.conf的具体路径，sample_ao2ai会读取该文件，完成参数解析。
	然后按照参数运行测试。

从命令行启动sample_ao2ai的指令：
	./sample_ao2ai -path /mnt/extsd/sample_ao2ai/sample_ao2ai.conf
	"-path /mnt/extsd/sample_ao2ai/sample_ao2ai.conf"指定了测试参数配置文件的路径。

测试参数的说明：
(1)pcm_src_path：指定音频pcm文件的路径，该文件是包含wave头(大小为44Bytes)的wav格式文件，如果找不到这种格式文件，可以用sample_ai生成一个。
(2)pcm_dst_path：指定目标文件的路径，该文件是ai组件采集音频生成的文件，不带wav头，如果想听该音频，需手动加上wave头。
(3)aac_dst_path: 指定编码后aac目标文件路径，该文件为aac raw data。
(4)pcm_sample_rate：指定采样率，设置为文件中的采样率的值，启用aec后，sample rate须为8000.
(5)pcm_channel_cnt：指定通道数目，设置为文件中的通道数，启用aec后，sample rate须为1. 
(6)pcm_bit_width：指定位宽，设置为文件中的位宽，启用aec后，bit_width须为16.
(7)pcm_frame_size：固定指定为1024。
(8)aec_en:	1:启动aec回声消除功能；0：不启用aec回声消除功能；
(9)aec_delay_ms： 启用aec回声消除功能时，设置的延迟时间，主要给aec回声消除算法使用，建议先设置为0；


启用aec功能的方式如下：
	启用aec功能并不复杂，相比于不启用aec功能的操作，只是在设置ai 
	dev属性时多了两个属性参数：
	1）ai_aec_en,是否使能aec，1：enable;0:not enable; 
	2)aec_delay_ms: aec算法用delay参数，unit：ms,建议先设为0；
	然后调用api：AW_MPI_AI_SetPubAttr();

	完成上面对 ai dev属性的设置，还需要将创建的ao chl与 ai 
	chl做绑定，已实现ao数据到ai数据的回传，从而aec获取referenc frame,即有如下的操作：
	AW_MPI_SYS_Bind(&AOChn, &AIChn);
	
	=====
	note：创建多个ai chl时，只需将ao chl与第一个ai chl 绑定即可，不需将ao chl与每个创建的ai 
	chl绑定。

	具体使用过程可参考当前目录下的sample_ao2ai.c文件。

========================20191031 patch  更新及使用说明：

为得到到较理想的回声消除效果，此次patch更新启用了底层的回声采集通路，所以，更新此patch后，
上面说明中要求的 ao与 ai必须bind的动作，不再需要，即务必确保
	AW_MPI_SYS_Bind(&AOChn, &AIChn);
不再调用。	

其他保持不变。

