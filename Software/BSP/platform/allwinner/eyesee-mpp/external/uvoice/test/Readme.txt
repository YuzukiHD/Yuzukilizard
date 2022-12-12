TestUvoice功能：
    测试uvoice智能语音识别库。将采集的音频送入uvoice lib，通过callback获取uvoice的检测结果。
    允许把音频数据保存为wav文件，作为比对依据。
    手动停止运行：ctrl+c

    提供配置文件TestUvoice.conf，用于配置运行参数。
    启动时，在命令行参数中给出配置文件的具体路径，app会读取TestUvoice.conf，完成参数解析，
    然后按照参数运行。
    例如从命令行启动指令：
    ./TestUvoice -path ./TestUvoice.conf
    "-path ./TestUvoice.conf"指定了参数配置文件的路径。
    如果不给配置文件，则app按默认参数运行。

测试参数的说明：
(1)save_dir_path: 指定保存pcm数据的目录，例如/mnt/extsd/TestUvoice_Files。注意不要在最后加/。
(2)save_file_name: 指定保存的文件名，例如TestUvoice.wav。
(3)pcm_sample_rate：指定采样率，例如16000
(4)pcm_channel_cnt：指定声道数，例如1
(5)pcm_bit_width：指定一个声道的sample的bit数，例如16
(6)pcm_cap_duration：指定采集时间，单位为秒，0表示无限。
(7)save_wav：指定是否保存wav文件。1表示保存，0表示不保存。

