sample_demux2vdec2vo测试流程：
	解码视频文件（如：xxx.mp4）并从屏幕上显示出来.
	Ctrl-C可中止运行。

读取测试参数的流程：
	sample提供了sample_demux2vdec2vo.conf，测试参数都写在该文件中。
	启动sample_demux2vdec2vo时，在命令行参数中给出sample_demux2vdec2vo.conf的具体路径，sample_demux2vdec2vo会读取sample_demux2vdec2vo.conf，完成参数解析。
	然后按照参数运行测试。
	从命令行启动sample_demux2vdec2vo的指令：
	./sample_demux2vdec2vo -path /mnt/extsd/sample_demux2vdec2vo.conf
	"-path /mnt/extsd/sample_demux2vdec2vo.conf"指定了测试参数配置文件的路径。

测试参数的说明：
(1)src_file：指定原始视频文件的路径
(2)seek_position：指定原始视频文件的开始解析位置(ms)
(3)test_duration: sample一次测试时间（单位：s）
(4)display rect:目标显示区域
