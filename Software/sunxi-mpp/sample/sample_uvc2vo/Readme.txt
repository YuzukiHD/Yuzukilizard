sample_uvc2vo测试流程：
    测试mpi_uvc组件的绑定方式输出。
	mpi_uvc组件输出yuyv格式（或其他raw格式）到mpi_vo组件显示。mpi_uvc和mpi_vo组件采用绑定方式连接。

读取测试参数的流程：
	在文件sample_uvc2vo.conf填写配置参数。
	启动app时，在命令行参数中给出sample_uvc2vo.conf的具体路径，app读取sample_uvc2vo.conf，完成参数解析。
	然后按照参数运行测试。
	从命令行启动sample_uvc2vo的指令：
	./sample_uvc2vo -path /mnt/extsd/sample_uvc2vo.conf

测试参数的说明：
(1)dev_name: uvc设备字符串
(2)pic_format: uvc输出图像格式
(3)capture_width: uvc输出图像宽度
(4)capture_height: uvc输出图像高度
(5)display_width:显示宽度
(6)display_height:显示高度
(7)test_duration:测试时间，单位秒
(8)brightness:uvc亮度设置
(9)contrast:uvc对比度设置
(10)hue:uvc色度设置
(11)saturation:uvc饱和度设置
(12)sharpness:uvc锐度设置

